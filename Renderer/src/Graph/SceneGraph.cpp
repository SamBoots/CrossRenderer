#include "SceneGraph.hpp"
#include "Storage/Array.h"
#include "Storage/Slotmap.h"
#include "ShaderCompiler.h"

#include "RenderBackend.h"
#include "RenderFrontend.h"
#include "AssetLoader.hpp"

#include "LightSystem.h"
#include "BBjson.hpp"

#include "Math.inl"

using namespace BB;

struct SceneInfo
{
	Mat4x4 view{};
	Mat4x4 projection{};

	float3 ambientLight{};
	float ambientStrength = 0.f;

	uint32_t lightCount = 0;
	uint3 padding{};
};

struct InstanceTransform
{
	Mat4x4 transform;
	Mat4x4 inverse;
};

constexpr uint32_t SCENE_PUSH_CONSTANT_DWORD_COUNT = 2;
struct ScenePushConstantInfo
{
	uint32_t modelMatrixIndex;
	uint32_t textureIndex1;
};

struct SceneDrawCall
{
	PipelineHandle pipeline;
	uint32_t transformIndex;
	uint32_t baseColorIndex;
	uint32_t normalTexture;
	//material here.

	uint32_t meshDescriptorOffset;

	//TEMP, will do drawindirect later.
	uint32_t indexCount;
	uint32_t indexStart;
};

struct SceneDrawCallArray
{
	//jank, later on this should be an indirect draw buffer.
	SceneDrawCall* startMemory;
	SceneDrawCall* currentCall;

	SceneDrawCallArray(Allocator a_Allocator)
	{
		startMemory = BBnewArr(a_Allocator, 256, SceneDrawCall);
		currentCall = startMemory;
	};

	inline size_t ElementSize() const
	{
		return ArraySizeInBytes() / sizeof(SceneDrawCall);
	}

	inline uintptr_t ArraySizeInBytes() const
	{
		return reinterpret_cast<uintptr_t>(currentCall) - reinterpret_cast<uintptr_t>(startMemory);
	};

	inline void AddDrawCall(const SceneDrawCall& a_DrawCall)
	{
		*currentCall = a_DrawCall;
		++currentCall;
	};
	inline void Reset()
	{
		currentCall = startMemory;
	}
};

struct TransformArray
{
	//jank, should get a global upload buffer, and later on this should be an indirect draw buffer.
	UploadBuffer uploadBuffer; //24 bytes
	InstanceTransform* currentMatrices; //32 bytes
	uint32_t elementCount; //44 bytes, jank
	const uint32_t elementMax; //40 bytes, jank

	TransformArray(const size_t a_TransformCount) :
		uploadBuffer(sizeof(InstanceTransform) * a_TransformCount, "TransformArray upload buffer"), elementMax(a_TransformCount)
	{
		elementCount = 0;
		currentMatrices = reinterpret_cast<InstanceTransform*>(uploadBuffer.GetStart());
	};

	inline size_t ElementCount() const
	{
		return elementCount;
	}

	inline uintptr_t ArraySizeInBytes() const
	{
		return reinterpret_cast<uintptr_t>(currentMatrices) - reinterpret_cast<uintptr_t>(uploadBuffer.GetStart());
	};

	inline uint32_t AddTransform(const InstanceTransform& a_Matrices)
	{
		*currentMatrices = a_Matrices;
		++currentMatrices;
		return elementCount++;
	};
	inline void Reset()
	{
		currentMatrices = reinterpret_cast<InstanceTransform*>(uploadBuffer.GetStart());
	}
};

struct SceneFrame
{
	SceneFrame(Allocator a_Allocator, const uint32_t a_TransformCount) : drawArray(a_Allocator), transformArray(a_TransformCount) {}
	SceneDrawCallArray drawArray;
	TransformArray transformArray;
	UploadBufferChunk uploadChunk;

	RenderBufferPart sceneBuffer;
	RenderBufferPart matrixBuffer;
	RenderBufferPart lightBuffer;
};

struct BB::SceneGraph_inst
{
	SceneGraph_inst(Allocator a_Allocator, const RenderBufferCreateInfo& a_BufferInfo, const size_t a_SceneBufferSizePerFrame, const SceneCreateInfo& a_CreateInfo, const uint32_t a_BackBufferCount)
		:	sceneName(a_CreateInfo.sceneName),
			systemAllocator(a_Allocator),
			lights(a_Allocator, a_CreateInfo.lights.size()),
			GPUbuffer(a_BufferInfo),
			uploadbuffer(mbSize * 16 * a_BackBufferCount, "Temporary scene upload buffer")
	{
		sceneWindowWidth = a_CreateInfo.sceneWindowWidth;
		sceneWindowHeight = a_CreateInfo.sceneWindowHeight;

		lights.push_back(a_CreateInfo.lights.data(), a_CreateInfo.lights.size());

		sceneFrames = BBnewArr(systemAllocator, a_BackBufferCount, SceneFrame);
		for (size_t i = 0; i < a_BackBufferCount; i++)
		{
			constexpr uint32_t TRANSFORM_SIZE = 256;
			new (&sceneFrames[i])SceneFrame(systemAllocator, TRANSFORM_SIZE);
			sceneFrames[i].uploadChunk = uploadbuffer.Alloc(mbSize * 16);

			sceneFrames[i].sceneBuffer = GPUbuffer.SubAllocate(sizeof(SceneInfo));
			sceneFrames[i].matrixBuffer = GPUbuffer.SubAllocate(TRANSFORM_SIZE * sizeof(InstanceTransform));
			sceneFrames[i].lightBuffer = GPUbuffer.SubAllocate(lights.capacity() * sizeof(lights[0]));
		}
	};

	~SceneGraph_inst()
	{
		BBfree(systemAllocator, sceneFrames);
	};

	const char* sceneName = nullptr;

	Allocator systemAllocator;

	LinearRenderBuffer GPUbuffer;
	uint32_t sceneWindowWidth;
	uint32_t sceneWindowHeight;

	SceneFrame* sceneFrames;
	SceneFrame* currentFrame;

	RDescriptor sceneDescriptor{};
	DescriptorAllocation sceneAllocation{};

	RDescriptor meshDescriptor;
	PipelineHandle meshPipeline;

	//send to GPU
	SceneInfo sceneInfo{};
	Array<Light> lights;

	//temp
	UploadBuffer uploadbuffer;

	RImageHandle depthImage;
};

SceneGraph::SceneGraph(Allocator a_Allocator, const SceneCreateInfo& a_CreateInfo)
{
	Init(a_Allocator, a_CreateInfo);
}

SceneGraph::SceneGraph(Allocator a_Allocator, Allocator a_TemporaryAllocator, const char* a_JsonPath)
{
	JsonParser t_SceneJson(a_JsonPath);
	t_SceneJson.Parse();
	const JsonObject& t_Head = *t_SceneJson.GetRootNode()->GetObject();
	//Jank, find new way to do hashmap finding that does not return a pointer.
	JsonNode* t_JsonNode = *t_Head.map.find("scene");
	const JsonObject& t_SceneObj = *t_JsonNode->GetObject();

	SceneCreateInfo t_SceneCreateInfo;
	{
		t_JsonNode = *t_SceneObj.map.find("scene_name");
		t_SceneCreateInfo.sceneName = Asset::FindOrCreateString(t_JsonNode->GetString());

		t_JsonNode = *t_SceneObj.map.find("scene_lights");
		const JsonList& t_LightsList = t_JsonNode->GetList();
		//
		Light* t_Lights = BBnewArr(a_TemporaryAllocator, t_LightsList.nodeCount, Light);
		for (size_t i = 0; i < t_LightsList.nodeCount; i++)
		{
			const JsonObject& t_LightObject = *t_LightsList.nodes[i]->GetObject();
			//Jank, find new way to do hashmap finding that does not return a pointer.
			{
				const JsonNode* t_Radius = *t_LightObject.map.find("radius");
				t_Lights->radius = t_Radius->GetNumber();
			}
			{
				const JsonNode* t_PosList = *t_LightObject.map.find("position");
				t_Lights->pos.x = t_PosList->GetList().nodes[0]->GetNumber();
				t_Lights->pos.y = t_PosList->GetList().nodes[1]->GetNumber();
				t_Lights->pos.z = t_PosList->GetList().nodes[2]->GetNumber();
			}
			{
				const JsonNode* t_ColorList = *t_LightObject.map.find("color");
				t_Lights->color.x = t_ColorList->GetList().nodes[0]->GetNumber();
				t_Lights->color.y = t_ColorList->GetList().nodes[1]->GetNumber();
				t_Lights->color.z = t_ColorList->GetList().nodes[2]->GetNumber();
				t_Lights->color.w = t_ColorList->GetList().nodes[3]->GetNumber();
			}
		}

		t_SceneCreateInfo.lights = BB::Slice(t_Lights, t_LightsList.nodeCount);
	}
	const Render_IO t_RIO = Render::GetIO();
	t_SceneCreateInfo.sceneWindowWidth = t_RIO.swapchainWidth;
	t_SceneCreateInfo.sceneWindowHeight = t_RIO.swapchainHeight;

	Init(a_Allocator, t_SceneCreateInfo);
}

void SceneGraph::Init(Allocator a_Allocator, const SceneCreateInfo& a_CreateInfo)
{
	const uint32_t t_BackBufferAmount = RenderBackend::GetFrameBufferAmount();
	const size_t t_GPUBufferSize = (mbSize * 32);
	const size_t t_BufferSize = t_GPUBufferSize * t_BackBufferAmount;

	RenderBufferCreateInfo t_SceneBufferInfo;
	//Maybe add scene name.
	t_SceneBufferInfo.name = "Scene GPU Buffer";
	t_SceneBufferInfo.size = t_BufferSize;
	t_SceneBufferInfo.usage = RENDER_BUFFER_USAGE::STORAGE;
	t_SceneBufferInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;

	inst = BBnew(a_Allocator, SceneGraph_inst)(a_Allocator, t_SceneBufferInfo, t_GPUBufferSize, a_CreateInfo, t_BackBufferAmount);

	RenderDescriptorCreateInfo t_CreateInfo;
	t_CreateInfo.name = "scene descriptor";
	t_CreateInfo.set = RENDER_DESCRIPTOR_SET::PER_PASS;
	FixedArray<DescriptorBinding, 3> t_DescBinds;
	t_CreateInfo.bindings = BB::Slice(t_DescBinds.data(), t_DescBinds.size());
	{//Per frame info Bind
		t_DescBinds[0].binding = 0;
		t_DescBinds[0].descriptorCount = 1;
		t_DescBinds[0].stage = RENDER_SHADER_STAGE::ALL;
		t_DescBinds[0].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
	}
	{//model matrix Binding
		t_DescBinds[1].binding = 1;
		t_DescBinds[1].descriptorCount = 1;
		t_DescBinds[1].stage = RENDER_SHADER_STAGE::ALL;
		t_DescBinds[1].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
	}
	{//Light Binding
		t_DescBinds[2].binding = 2;
		t_DescBinds[2].descriptorCount = 1;
		t_DescBinds[2].stage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
		t_DescBinds[2].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
	}

	inst->sceneDescriptor = RenderBackend::CreateDescriptor(t_CreateInfo);
	inst->sceneAllocation = Render::AllocateDescriptor(inst->sceneDescriptor);

	{ //depth create info
		RenderImageCreateInfo t_DepthInfo{};
		t_DepthInfo.name = "scene depth image";
		t_DepthInfo.width = a_CreateInfo.sceneWindowWidth;
		t_DepthInfo.height = a_CreateInfo.sceneWindowHeight;
		t_DepthInfo.depth = 1;
		t_DepthInfo.arrayLayers = 1;
		t_DepthInfo.mipLevels = 1;
		t_DepthInfo.tiling = RENDER_IMAGE_TILING::OPTIMAL;
		t_DepthInfo.type = RENDER_IMAGE_TYPE::TYPE_2D;
		t_DepthInfo.format = RENDER_IMAGE_FORMAT::DEPTH_STENCIL;

		inst->depthImage = RenderBackend::CreateImage(t_DepthInfo);
	}

	{	//PER_MESH descriptor
		RenderDescriptorCreateInfo t_CreateInfo{};
		t_CreateInfo.name = "3d mesh descriptor";
		//WRONG PER MATERIAL! But we do not do materials just yet :)
		t_CreateInfo.set = RENDER_DESCRIPTOR_SET::PER_MATERIAL;
		FixedArray<DescriptorBinding, 1> t_DescBinds;
		t_CreateInfo.bindings = BB::Slice(t_DescBinds.data(), t_DescBinds.size());

		t_DescBinds[0].binding = 0;
		t_DescBinds[0].descriptorCount = 1;
		t_DescBinds[0].stage = RENDER_SHADER_STAGE::VERTEX;
		t_DescBinds[0].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;

		inst->meshDescriptor = RenderBackend::CreateDescriptor(t_CreateInfo);
	}

	{	//pipeline boiler plate, I do not like that I made this.
		PipelineRenderTargetBlend t_BlendInfo{};
		t_BlendInfo.blendEnable = true;
		t_BlendInfo.srcBlend = RENDER_BLEND_FACTOR::SRC_ALPHA;
		t_BlendInfo.dstBlend = RENDER_BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
		t_BlendInfo.blendOp = RENDER_BLEND_OP::ADD;
		t_BlendInfo.srcBlendAlpha = RENDER_BLEND_FACTOR::ONE;
		t_BlendInfo.dstBlendAlpha = RENDER_BLEND_FACTOR::ZERO;
		t_BlendInfo.blendOpAlpha = RENDER_BLEND_OP::ADD;

		PipelineInitInfo t_PipeInitInfo{};
		t_PipeInitInfo.name = "standard 3d pipeline.";
		t_PipeInitInfo.renderTargetBlends = &t_BlendInfo;
		t_PipeInitInfo.renderTargetBlendCount = 1;
		t_PipeInitInfo.blendLogicOp = RENDER_LOGIC_OP::COPY;
		t_PipeInitInfo.blendLogicOpEnable = false;
		t_PipeInitInfo.rasterizerState.cullMode = RENDER_CULL_MODE::BACK;
		t_PipeInitInfo.rasterizerState.frontCounterClockwise = false;
		t_PipeInitInfo.enableDepthTest = true;

		//We only have 1 index so far.
		t_PipeInitInfo.constantData.dwordSize = SCENE_PUSH_CONSTANT_DWORD_COUNT;
		t_PipeInitInfo.constantData.shaderStage = RENDER_SHADER_STAGE::ALL;

		SamplerCreateInfo t_ImmutableSampler{};
		t_ImmutableSampler.name = "standard sampler";
		t_ImmutableSampler.addressModeU = SAMPLER_ADDRESS_MODE::REPEAT;
		t_ImmutableSampler.addressModeV = SAMPLER_ADDRESS_MODE::REPEAT;
		t_ImmutableSampler.addressModeW = SAMPLER_ADDRESS_MODE::REPEAT;
		t_ImmutableSampler.filter = SAMPLER_FILTER::LINEAR;
		t_ImmutableSampler.maxAnistoropy = 1.0f;
		t_ImmutableSampler.maxLod = 100.f;
		t_ImmutableSampler.minLod = -100.f;

		t_PipeInitInfo.immutableSamplers = BB::Slice(&t_ImmutableSampler, 1);

		PipelineBuilder t_BasicPipe{ t_PipeInitInfo };

		t_BasicPipe.BindDescriptor(Render::GetGlobalDescriptorSet());
		t_BasicPipe.BindDescriptor(inst->sceneDescriptor);
		t_BasicPipe.BindDescriptor(inst->meshDescriptor);

		const wchar_t* t_ShaderPath[2]{};
		t_ShaderPath[0] = L"Resources/Shaders/HLSLShaders/DebugVert.hlsl";
		t_ShaderPath[1] = L"Resources/Shaders/HLSLShaders/DebugFrag.hlsl";

		const Render_IO t_RenderIO = Render::GetIO();

		ShaderCodeHandle t_ShaderHandles[2];
		t_ShaderHandles[0] = Shader::CompileShader(
			t_ShaderPath[0],
			L"main",
			RENDER_SHADER_STAGE::VERTEX,
			t_RenderIO.renderAPI);
		t_ShaderHandles[1] = Shader::CompileShader(
			t_ShaderPath[1],
			L"main",
			RENDER_SHADER_STAGE::FRAGMENT_PIXEL,
			t_RenderIO.renderAPI);

		Buffer t_ShaderBuffer;
		Shader::GetShaderCodeBuffer(t_ShaderHandles[0], t_ShaderBuffer);
		ShaderCreateInfo t_ShaderBuffers[2]{};
		t_ShaderBuffers[0].optionalShaderpath = "Resources/Shaders/HLSLShaders/DebugVert.hlsl";
		t_ShaderBuffers[0].buffer = t_ShaderBuffer;
		t_ShaderBuffers[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;

		Shader::GetShaderCodeBuffer(t_ShaderHandles[1], t_ShaderBuffer);
		t_ShaderBuffers[1].optionalShaderpath = "Resources/Shaders/HLSLShaders/DebugFrag.hlsl";
		t_ShaderBuffers[1].buffer = t_ShaderBuffer;
		t_ShaderBuffers[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;

		t_BasicPipe.BindShaders(BB::Slice(t_ShaderBuffers, 2));

		inst->meshPipeline = t_BasicPipe.BuildPipeline();

		for (size_t i = 0; i < _countof(t_ShaderHandles); i++)
		{
			Shader::ReleaseShaderCode(t_ShaderHandles[i]);
		}
	}
}

SceneGraph::~SceneGraph()
{
	RenderBackend::DestroyImage(inst->depthImage);
	Allocator t_Allocator = inst->systemAllocator;
	BBfree(t_Allocator, inst);
}

void PreRenderFunc(const CommandListHandle a_CmdList, const GraphPreRenderInfo& a_PreRenderInfo)
{
	SceneGraph* t_Scene = reinterpret_cast<SceneGraph*>(a_PreRenderInfo.instance);
	t_Scene->StartScene(a_CmdList);
}

void RenderFunc(const CommandListHandle a_CmdList, const GraphRenderInfo& a_RenderInfo)
{
	SceneGraph* t_Scene = reinterpret_cast<SceneGraph*>(a_RenderInfo.instance);
	t_Scene->RenderScene(a_CmdList, a_RenderInfo.currentLayout, a_RenderInfo.renderLayout, a_RenderInfo.endLayout);
}

void PostRenderFunc(const CommandListHandle a_CmdList, const GraphPostRenderInfo& a_PostRenderInfo)
{
	SceneGraph* t_Scene = reinterpret_cast<SceneGraph*>(a_PostRenderInfo.instance);
	t_Scene->EndScene(a_CmdList);
}

SceneGraph::operator FrameGraphRenderPass()
{
	FrameGraphRenderPass t_RenderPass{};
	t_RenderPass.instance = this;
	t_RenderPass.preRenderFunc = PreRenderFunc;
	t_RenderPass.renderFunc = RenderFunc;
	t_RenderPass.postRenderFunc = PostRenderFunc;
	return t_RenderPass;
}

void SceneGraph::StartScene(const CommandListHandle a_GraphicList)
{
	FixedArray<WriteDescriptorData, 3> t_WriteDatas;
	WriteDescriptorInfos t_BufferUpdate{};
	t_BufferUpdate.allocation = inst->sceneAllocation;
	t_BufferUpdate.descriptorHandle = inst->sceneDescriptor;
	t_BufferUpdate.data = BB::Slice(t_WriteDatas.data(), t_WriteDatas.size());

	inst->currentFrame = &inst->sceneFrames[RenderBackend::GetCurrentFrameBufferIndex()];
	const SceneFrame& t_SceneFrame = *inst->currentFrame;
	uint32_t t_UploadUsed = 0;

	inst->sceneInfo.ambientLight = { 1.0f, 1.0f, 1.0f };
	inst->sceneInfo.ambientStrength = 0.1f;
	//if (inst->sceneInfo.lightCount != inst->lights.size())
	{	//If we hvae more lights then we upload them. Maybe do a bool to check instead.
		inst->sceneInfo.lightCount = static_cast<uint32_t>(inst->lights.size());

		Memory::Copy(Pointer::Add(t_SceneFrame.uploadChunk.memory, t_UploadUsed),
			inst->lights.data(),
			inst->lights.size());
	}
	
	{//Copy over transfer buffer to GPU
		//Copy the perframe buffer over.
		RenderCopyBufferInfo t_CopyInfo;
		t_CopyInfo.src = inst->uploadbuffer.Buffer();
		t_CopyInfo.dst = t_SceneFrame.lightBuffer.buffer;
		t_CopyInfo.size = inst->lights.size() * sizeof(inst->lights[0]);
		t_CopyInfo.srcOffset = static_cast<uint64_t>(t_SceneFrame.uploadChunk.offset) + t_UploadUsed;
		t_CopyInfo.dstOffset = t_SceneFrame.lightBuffer.offset;
		BB_ASSERT(t_CopyInfo.size + t_UploadUsed < t_SceneFrame.uploadChunk.size, "Upload buffer overflow, uploading too many lights!");

		RenderBackend::CopyBuffer(a_GraphicList, t_CopyInfo);

		//temporarily shift the buffer part for the scene upload
		t_UploadUsed += static_cast<uint32_t>(t_CopyInfo.size);

		t_WriteDatas[2].binding = 2;
		t_WriteDatas[2].descriptorIndex = 0;
		t_WriteDatas[2].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
		t_WriteDatas[2].buffer.buffer = t_CopyInfo.dst;
		t_WriteDatas[2].buffer.offset = t_CopyInfo.dstOffset;
		t_WriteDatas[2].buffer.range = t_CopyInfo.size;
	}

	{	//always upload the current scene info.
		memcpy(Pointer::Add(t_SceneFrame.uploadChunk.memory, t_UploadUsed),
			&inst->sceneInfo,
			sizeof(inst->sceneInfo));

		//Copy the perframe buffer and matrices.
		RenderCopyBufferInfo t_SceneCopyInfo;
		t_SceneCopyInfo.src = inst->uploadbuffer.Buffer();
		t_SceneCopyInfo.dst = t_SceneFrame.sceneBuffer.buffer;
		t_SceneCopyInfo.size = t_SceneFrame.sceneBuffer.size;
		t_SceneCopyInfo.srcOffset = static_cast<uint64_t>(t_SceneFrame.uploadChunk.offset) + t_UploadUsed;
		t_SceneCopyInfo.dstOffset = t_SceneFrame.sceneBuffer.offset;
		BB_ASSERT(t_SceneCopyInfo.size + t_UploadUsed < t_SceneFrame.uploadChunk.size, "Upload buffer overflow");

		RenderBackend::CopyBuffer(a_GraphicList, t_SceneCopyInfo);

		t_UploadUsed += static_cast<uint32_t>(t_SceneCopyInfo.size);

		t_WriteDatas[0].binding = 0;
		t_WriteDatas[0].descriptorIndex = 0;
		t_WriteDatas[0].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
		t_WriteDatas[0].buffer.buffer = t_SceneCopyInfo.dst;
		t_WriteDatas[0].buffer.offset = t_SceneCopyInfo.dstOffset;
		t_WriteDatas[0].buffer.range = t_SceneCopyInfo.size;

		{	//upload matrices
			RenderCopyBufferInfo t_MatrixCopyInfo;
			t_MatrixCopyInfo.src = t_SceneFrame.transformArray.uploadBuffer.Buffer();
			t_MatrixCopyInfo.dst = t_SceneFrame.matrixBuffer.buffer;
			t_MatrixCopyInfo.size = t_SceneFrame.transformArray.ArraySizeInBytes();
			t_MatrixCopyInfo.srcOffset = 0; //WHEN UPLOADBYFFER SYSTEM IN PLACE THIS CANNOT BE 0
			t_MatrixCopyInfo.dstOffset = t_SceneFrame.matrixBuffer.offset;
			BB_ASSERT(t_MatrixCopyInfo.size + t_UploadUsed < t_SceneFrame.uploadChunk.size, "Upload buffer overflow, uploading too many model matrices!");

			t_WriteDatas[1].binding = 1;
			t_WriteDatas[1].descriptorIndex = 0;
			t_WriteDatas[1].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
			t_WriteDatas[1].buffer.buffer = t_MatrixCopyInfo.dst;
			t_WriteDatas[1].buffer.offset = t_MatrixCopyInfo.dstOffset;
			t_WriteDatas[1].buffer.range = t_MatrixCopyInfo.size;

			RenderBackend::CopyBuffer(a_GraphicList, t_MatrixCopyInfo);

			t_UploadUsed += static_cast<uint32_t>(t_MatrixCopyInfo.size);
		}
	}

	RenderBackend::WriteDescriptors(t_BufferUpdate);
}

void SceneGraph::RenderScene(const CommandListHandle a_GraphicList, const RENDER_IMAGE_LAYOUT a_CurrentLayout, const RENDER_IMAGE_LAYOUT a_RenderLayout, const RENDER_IMAGE_LAYOUT a_EndLayout)
{
	StartRenderingInfo t_StartRenderInfo;
	t_StartRenderInfo.viewportWidth = inst->sceneWindowWidth;
	t_StartRenderInfo.viewportHeight = inst->sceneWindowHeight;
	t_StartRenderInfo.colorLoadOp = RENDER_LOAD_OP::CLEAR;
	t_StartRenderInfo.colorStoreOp = RENDER_STORE_OP::STORE;
	t_StartRenderInfo.colorInitialLayout = a_CurrentLayout;
	t_StartRenderInfo.colorFinalLayout = a_RenderLayout;
	t_StartRenderInfo.clearColor[0] = 1.0f;
	t_StartRenderInfo.clearColor[1] = 0.0f;
	t_StartRenderInfo.clearColor[2] = 0.0f;
	t_StartRenderInfo.clearColor[3] = 1.0f;
	t_StartRenderInfo.depthStencil = inst->depthImage;

	//Record rendering commands.
	RenderBackend::StartRendering(a_GraphicList, t_StartRenderInfo);

	//early out if we have nothing to render. Still do image transitions.
	if (inst->currentFrame->drawArray.ArraySizeInBytes() == 0)
	{
		EndRenderingInfo t_EndRenderingInfo{};
		t_EndRenderingInfo.colorInitialLayout = a_RenderLayout;
		t_EndRenderingInfo.colorFinalLayout = a_EndLayout;
		RenderBackend::EndRendering(a_GraphicList, t_EndRenderingInfo);
		return;
	}


	const SceneDrawCall* t_DrawCall = inst->currentFrame->drawArray.startMemory;
	uint32_t t_MeshDescriptorOffset = t_DrawCall->meshDescriptorOffset;
	PipelineHandle t_Pipeline = t_DrawCall->pipeline;

	const uint32_t t_FrameNum = RenderBackend::GetCurrentFrameBufferIndex();

	RenderBackend::BindPipeline(a_GraphicList, t_Pipeline);

	{
		const uint32_t t_IsSamplerHeap[3]{ false, false, false };
		const size_t t_BufferOffsets[3]{ Render::GetIO().globalDescAllocation.offset, inst->sceneAllocation.offset, t_MeshDescriptorOffset };
		//PER_PASS and mesh at the same time.
		RenderBackend::SetDescriptorHeapOffsets(a_GraphicList, RENDER_DESCRIPTOR_SET::ENGINE_GLOBAL, 3, t_IsSamplerHeap, t_BufferOffsets);
		//Bind once, the offsets will be handles by a_FirstIndex.
		RenderBackend::BindIndexBuffer(a_GraphicList, Render::GetIndexBuffer().GetBuffer(), 0);
	}

	for (auto t_It = inst->currentFrame->drawArray.startMemory; t_It < inst->currentFrame->drawArray.currentCall; t_It++)
	{
		//check material, for later
		if (false)
		{
			//change material.....
		}

		if (t_It->pipeline != t_Pipeline)
		{
			t_Pipeline = t_It->pipeline;
			RenderBackend::BindPipeline(a_GraphicList, t_Pipeline);
		}

		if (t_It->meshDescriptorOffset != t_MeshDescriptorOffset)
		{
			t_MeshDescriptorOffset = t_It->meshDescriptorOffset;
			const uint32_t t_IsSamplerHeap[1]{ false };
			const size_t t_BufferOffsets[1]{ t_MeshDescriptorOffset };
			RenderBackend::SetDescriptorHeapOffsets(a_GraphicList, RENDER_DESCRIPTOR_SET::PER_MATERIAL, 1, t_IsSamplerHeap, t_BufferOffsets);
		}

		ScenePushConstantInfo t_PushInfo
		{
			t_It->transformIndex,
			t_It->baseColorIndex
		};

		RenderBackend::BindConstant(a_GraphicList, 0, SCENE_PUSH_CONSTANT_DWORD_COUNT, 0, &t_PushInfo);
		RenderBackend::DrawIndexed(a_GraphicList,
			t_It->indexCount,
			1,
			t_It->indexStart,
			0,
			0);
	}

	EndRenderingInfo t_EndRenderingInfo{};
	t_EndRenderingInfo.colorInitialLayout = a_RenderLayout;
	t_EndRenderingInfo.colorFinalLayout = a_EndLayout;
	RenderBackend::EndRendering(a_GraphicList, t_EndRenderingInfo);
}

void SceneGraph::EndScene(const CommandListHandle a_GraphicList)
{

}

void SceneGraph::SetProjection(const Mat4x4& a_Proj)
{
	inst->sceneInfo.projection = a_Proj;
}

void SceneGraph::SetView(const Mat4x4& a_View)
{
	inst->sceneInfo.view = a_View;
}

void SceneGraph::RenderModel(const RModelHandle a_Model, const Mat4x4& a_Transform)
{
	const Model& t_Model = Render::GetModel(a_Model);

	SceneDrawCall t_DrawCall;
	t_DrawCall.meshDescriptorOffset = t_Model.descAllocation.offset;
	t_DrawCall.pipeline = t_Model.pipelineHandle;

	for (size_t i = 0; i < t_Model.linearNodeCount; i++)
	{
		const Model::Node& t_Node = t_Model.linearNodes[i];
		if (t_Node.meshIndex != MESH_INVALID_INDEX)
		{
			const Model::Mesh& t_Mesh = t_Model.meshes[t_Node.meshIndex];
			InstanceTransform t_Transform;
			//wrong, need to go through the childeren and then update the transforms that way.
			t_Transform.transform = a_Transform * t_Node.transform;
			t_Transform.inverse = Mat4x4Inverse(t_Transform.transform);
			t_DrawCall.transformIndex = inst->currentFrame->transformArray.AddTransform(t_Transform);

			for (size_t t_PrimIndex = 0; t_PrimIndex < t_Mesh.primitiveCount; t_PrimIndex++)
			{
				const Model::Primitive& t_Prim = t_Model.primitives[t_Mesh.primitiveOffset + t_PrimIndex];

				BB_ASSERT(t_Prim.indexCount + t_Prim.indexStart < t_Model.indexView.size, "index buffer reading out of bounds");
				t_DrawCall.baseColorIndex = t_Prim.baseColorIndex.index;
				t_DrawCall.normalTexture = t_Prim.normalIndex.index;
				t_DrawCall.indexCount = t_Prim.indexCount;
				//Hacky way to only set the index buffer once, and let the drawindexed just index deep into the buffer.
				t_DrawCall.indexStart = t_Prim.indexStart + (t_Model.indexView.offset / (sizeof(uint32_t)));
				inst->currentFrame->drawArray.AddDrawCall(t_DrawCall);
			}
		}
	}
}

void SceneGraph::RenderModels(const RModelHandle* a_Models, const Mat4x4* a_Transforms, const uint32_t a_ObjectCount)
{
	for (uint32_t i = 0; i < a_ObjectCount; i++)
	{
		RenderModel(a_Models[i], a_Transforms[i]);
	}
}

BB::Slice<Light> SceneGraph::GetLights()
{
	return inst->lights;
}

const RDescriptor SceneGraph::GetSceneDescriptor() const
{
	return inst->sceneDescriptor;
}

const char* SceneGraph::GetSceneName() const
{
	return inst->sceneName;
}

const RDescriptor SceneGraph::GetMeshDescriptor() const
{
	return inst->meshDescriptor;
}

const PipelineHandle SceneGraph::GetPipelineHandle() const
{
	return inst->meshPipeline;
}
#include "SceneGraph.hpp"
#include "Storage/Array.h"
#include "Storage/Slotmap.h"
#include "ShaderCompiler.h"

#include "RenderBackend.h"
#include "RenderFrontend.h"

#include "LightSystem.h"

using namespace BB;

struct SceneInfo
{
	glm::mat4 view{};
	glm::mat4 projection{};

	float3 ambientLight{};
	float ambientStrength = 0.f;

	uint32_t lightCount = 0;
	uint3 padding{};
};

struct BB::SceneGraph_inst
{
	SceneGraph_inst(Allocator a_Allocator, const RenderBufferCreateInfo& a_BufferInfo, const size_t a_SceneBufferSizePerFrame, const SceneCreateInfo& a_CreateInfo, const uint32_t a_BackBufferCount)
		:	systemAllocator(a_Allocator),
			drawObjects(a_Allocator, 256),
			transformPool(a_Allocator, 256),
			lights(a_Allocator, a_CreateInfo.lights.size()),
			GPUbuffer(a_BufferInfo),
			sceneBufferSizePerFrame(a_SceneBufferSizePerFrame),
			uploadbuffer(mbSize * 16 * a_BackBufferCount, "Temporary scene upload buffer")
	{
		backbufferCount = a_BackBufferCount;
		sceneWindowWidth = a_CreateInfo.sceneWindowWidth;
		sceneWindowHeight = a_CreateInfo.sceneWindowHeight;

		lights.push_back(a_CreateInfo.lights.data(), a_CreateInfo.lights.size());

		uploadBufferChunk = BBnewArr(systemAllocator, a_BackBufferCount, UploadBufferChunk);
		for (size_t i = 0; i < a_BackBufferCount; i++)
		{
			uploadBufferChunk[i] = uploadbuffer.Alloc(mbSize * 16);
		}
	};

	~SceneGraph_inst()
	{
		BBfree(systemAllocator, uploadBufferChunk);
	};

	Allocator systemAllocator;

	LinearRenderBuffer GPUbuffer;
	uint32_t sceneWindowWidth;
	uint32_t sceneWindowHeight;

	Slotmap<DrawObject> drawObjects;

	TransformPool transformPool;
	RenderBufferPart matrixBuffer;

	Array<Light> lights;
	RenderBufferPart lightBuffer;

	RDescriptor sceneDescriptor{};
	DescriptorAllocation sceneAllocation{};
	size_t sceneBufferSizePerFrame;
	RenderBufferPart sceneBuffer;

	RDescriptor meshDescriptor;
	PipelineHandle meshPipeline;

	//send to GPU
	SceneInfo sceneInfo{};

	//temp
	UploadBuffer uploadbuffer;
	uint32_t backbufferCount;
	UploadBufferChunk* uploadBufferChunk;

	RImageHandle depthImage;
};

SceneGraph::SceneGraph(Allocator a_Allocator, const SceneCreateInfo& a_CreateInfo)
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

	RenderDescriptorCreateInfo t_CreateInfo{};
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

	size_t t_SceneBufferSize = sizeof(SceneInfo);
	t_SceneBufferSize *= t_BackBufferAmount;

	inst->sceneBuffer = inst->GPUbuffer.SubAllocate(t_SceneBufferSize);
	inst->matrixBuffer = inst->GPUbuffer.SubAllocate(inst->transformPool.PoolSize() * sizeof(ModelBufferInfo));
	inst->lightBuffer = inst->GPUbuffer.SubAllocate(inst->lights.capacity() * sizeof(inst->lights[0]), sizeof(size_t));

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
		t_PipeInitInfo.constantData.dwordSize = 1;
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

void SceneGraph::StartScene(RecordingCommandListHandle a_GraphicList)
{
	const uint32_t t_FrameNum = RenderBackend::GetCurrentFrameBufferIndex();
	const size_t t_SceneBufferOffset = inst->sceneBufferSizePerFrame * t_FrameNum;

	UploadBufferChunk t_UploadBuffer = inst->uploadBufferChunk[t_FrameNum];

	inst->sceneInfo.ambientLight = { 1.0f, 1.0f, 1.0f };
	inst->sceneInfo.ambientStrength = 0.1f;
	if (inst->sceneInfo.lightCount != inst->lights.size())
	{
		inst->sceneInfo.lightCount = static_cast<uint32_t>(inst->lights.size());

		//Copy the perframe buffer over.
		RenderCopyBufferInfo t_CopyInfo;
		t_CopyInfo.src = inst->uploadbuffer.Buffer();
		t_CopyInfo.dst = inst->lightBuffer.buffer;
		t_CopyInfo.size = inst->lights.size() * sizeof(inst->lights[0]);
		t_CopyInfo.srcOffset = t_UploadBuffer.offset;
		t_CopyInfo.dstOffset = inst->lightBuffer.offset;
		BB_ASSERT(t_UploadBuffer.offset + t_CopyInfo.size < t_UploadBuffer.size, "Upload buffer overflow, uploading too many lights!");

		RenderBackend::CopyBuffer(a_GraphicList, t_CopyInfo);

		//temporarily shift the buffer part for the scene upload
		t_UploadBuffer.offset += static_cast<uint32_t>(t_CopyInfo.size);
		t_UploadBuffer.size -= static_cast<uint32_t>(t_CopyInfo.size);
	}

	{
		const size_t t_SceneUploadBuffer = sizeof(SceneInfo);
		//always upload the current scene info.
		memcpy(Pointer::Add(t_UploadBuffer.memory, t_UploadBuffer.offset),
			&inst->sceneInfo, 
			sizeof(inst->sceneInfo));

		//Copy the perframe buffer and matrices.
		RenderCopyBufferInfo t_SceneCopyInfo;
		t_SceneCopyInfo.src = inst->uploadbuffer.Buffer();
		t_SceneCopyInfo.dst = inst->sceneBuffer.buffer;
		t_SceneCopyInfo.size = inst->sceneBuffer.size;
		t_SceneCopyInfo.srcOffset = t_UploadBuffer.offset;
		t_SceneCopyInfo.dstOffset = inst->sceneBuffer.offset + t_SceneBufferOffset;
		BB_ASSERT(t_UploadBuffer.offset + t_SceneCopyInfo.size < t_UploadBuffer.size, "Upload buffer overflow");

		RenderBackend::CopyBuffer(a_GraphicList, t_SceneCopyInfo);

		t_UploadBuffer.offset += static_cast<uint32_t>(t_SceneCopyInfo.size);
		t_UploadBuffer.size -= static_cast<uint32_t>(t_SceneCopyInfo.size);

		{//upload matrices
			//Copy the perframe buffer over.
			RenderCopyBufferInfo t_MatrixCopyInfo;
			t_MatrixCopyInfo.src = inst->transformPool.PoolGPUUploadBuffer().Buffer();
			t_MatrixCopyInfo.dst = inst->matrixBuffer.buffer;
			t_MatrixCopyInfo.size = inst->matrixBuffer.size;
			t_MatrixCopyInfo.srcOffset = 0;
			t_MatrixCopyInfo.dstOffset = inst->matrixBuffer.offset + t_SceneBufferOffset;
			BB_ASSERT(t_UploadBuffer.offset + t_MatrixCopyInfo.size < t_UploadBuffer.size, "Upload buffer overflow, uploading too many model matrices!");

			RenderBackend::CopyBuffer(a_GraphicList, t_MatrixCopyInfo);
		}
	}

	FixedArray<WriteDescriptorData, 3> t_WriteDatas;
	WriteDescriptorInfos t_BufferUpdate{};
	t_BufferUpdate.allocation = inst->sceneAllocation;
	t_BufferUpdate.descriptorHandle = inst->sceneDescriptor;
	t_BufferUpdate.data = BB::Slice(t_WriteDatas.data(), t_WriteDatas.size());

	t_WriteDatas[0].binding = 0;
	t_WriteDatas[0].descriptorIndex = 0;
	t_WriteDatas[0].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
	t_WriteDatas[0].buffer.buffer = inst->sceneBuffer.buffer;
	t_WriteDatas[0].buffer.offset = t_SceneBufferOffset;
	t_WriteDatas[0].buffer.range = sizeof(SceneInfo);

	t_WriteDatas[1].binding = 1;
	t_WriteDatas[1].descriptorIndex = 0;
	t_WriteDatas[1].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
	t_WriteDatas[1].buffer.buffer = inst->sceneBuffer.buffer;
	t_WriteDatas[1].buffer.offset = sizeof(SceneInfo) + t_SceneBufferOffset;
	t_WriteDatas[1].buffer.range = inst->transformPool.PoolSize() * sizeof(ModelBufferInfo);

	t_WriteDatas[2].binding = 2;
	t_WriteDatas[2].descriptorIndex = 0;
	t_WriteDatas[2].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
	t_WriteDatas[2].buffer.buffer = inst->lightBuffer.buffer;
	t_WriteDatas[2].buffer.offset = inst->lightBuffer.offset;
	t_WriteDatas[2].buffer.range = inst->lightBuffer.size;

	RenderBackend::WriteDescriptors(t_BufferUpdate);
}

void SceneGraph::RenderScene(RecordingCommandListHandle a_GraphicList)
{
	StartRenderingInfo t_StartRenderInfo;
	t_StartRenderInfo.viewportWidth = inst->sceneWindowWidth;
	t_StartRenderInfo.viewportHeight = inst->sceneWindowHeight;
	t_StartRenderInfo.colorLoadOp = RENDER_LOAD_OP::CLEAR;
	t_StartRenderInfo.colorStoreOp = RENDER_STORE_OP::STORE;
	t_StartRenderInfo.colorInitialLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
	t_StartRenderInfo.colorFinalLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	t_StartRenderInfo.clearColor[0] = 1.0f;
	t_StartRenderInfo.clearColor[1] = 0.0f;
	t_StartRenderInfo.clearColor[2] = 0.0f;
	t_StartRenderInfo.clearColor[3] = 1.0f;
	t_StartRenderInfo.depthStencil = inst->depthImage;

	//Record rendering commands.
	RenderBackend::StartRendering(a_GraphicList, t_StartRenderInfo);

	RModelHandle t_CurrentModel = inst->drawObjects.begin()->modelHandle;
	Model& t_Model = Render::GetModel(t_CurrentModel.handle);

	const uint32_t t_FrameNum = RenderBackend::GetCurrentFrameBufferIndex();

	RenderBackend::BindDescriptorHeaps(a_GraphicList, Render::GetGPUHeap(t_FrameNum), nullptr);
	RenderBackend::BindPipeline(a_GraphicList, t_Model.pipelineHandle);

	{
		const uint32_t t_IsSamplerHeap[2]{ false, false };
		const size_t t_BufferOffsets[2]{ inst->sceneAllocation.offset, t_Model.descAllocation.offset };
		//PER_PASS and mesh at the same time.
		RenderBackend::SetDescriptorHeapOffsets(a_GraphicList, RENDER_DESCRIPTOR_SET::PER_PASS, 2, t_IsSamplerHeap, t_BufferOffsets);
		RenderBackend::BindIndexBuffer(a_GraphicList, t_Model.indexView.buffer, t_Model.indexView.offset);
	}

	for (auto t_It = inst->drawObjects.begin(); t_It < inst->drawObjects.end(); t_It++)
	{
		if (t_CurrentModel != t_It->modelHandle)
		{
			t_CurrentModel = t_It->modelHandle;
			Model& t_NewModel = Render::GetModel(t_CurrentModel.handle);

			if (t_NewModel.pipelineHandle != t_Model.pipelineHandle)
			{
				RenderBackend::BindPipeline(a_GraphicList, t_NewModel.pipelineHandle);
			}

			const uint32_t t_IsSamplerHeap[1]{ false };
			const size_t t_BufferOffsets[1]{ t_NewModel.descAllocation.offset };
			RenderBackend::SetDescriptorHeapOffsets(a_GraphicList, RENDER_DESCRIPTOR_SET::PER_MATERIAL, 1, t_IsSamplerHeap, t_BufferOffsets);
			RenderBackend::BindIndexBuffer(a_GraphicList, t_NewModel.indexView.buffer, t_NewModel.indexView.offset);

			t_Model = t_NewModel;
		}

		RenderBackend::BindConstant(a_GraphicList, 0, 1, 0, &t_It->transformHandle.index);
		for (uint32_t i = 0; i < t_Model.linearNodeCount; i++)
		{
			const Model::Node& t_Node = t_Model.linearNodes[i];
			if (t_Node.meshIndex != MESH_INVALID_INDEX)
			{
				const Model::Mesh& t_Mesh = t_Model.meshes[t_Node.meshIndex];
				for (size_t t_PrimIndex = 0; t_PrimIndex < t_Mesh.primitiveCount; t_PrimIndex++)
				{
					const Model::Primitive& t_Prim = t_Model.primitives[t_Mesh.primitiveOffset + t_PrimIndex];
					RenderBackend::DrawIndexed(a_GraphicList,
						t_Prim.indexCount,
						1,
						t_Prim.indexStart,
						0,
						0);
				}
			}
		}
	}

	EndRenderingInfo t_EndRenderingInfo{};
	t_EndRenderingInfo.colorInitialLayout = t_StartRenderInfo.colorFinalLayout;
	t_EndRenderingInfo.colorFinalLayout = RENDER_IMAGE_LAYOUT::PRESENT;
	RenderBackend::EndRendering(a_GraphicList, t_EndRenderingInfo);
}

void SceneGraph::EndScene()
{
	//??, maybe to the EndRendering call here. 
	inst->transformPool.UpdateTransforms();
}

void SceneGraph::SetProjection(const glm::mat4& a_Proj)
{
	inst->sceneInfo.projection = a_Proj;
}

void SceneGraph::SetView(const glm::mat4& a_View)
{
	inst->sceneInfo.view = a_View;
}

DrawObjectHandle SceneGraph::CreateDrawObject(const RModelHandle a_Model, const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians, const glm::vec3 a_Scale)
{
	DrawObject t_DrawObject{ a_Model, inst->transformPool.CreateTransform(a_Position, a_Axis, a_Radians, a_Scale)};
	return DrawObjectHandle(inst->drawObjects.emplace(t_DrawObject).handle);
}

BB::Slice<DrawObject> SceneGraph::GetDrawObjects()
{
	return BB::Slice(inst->drawObjects.data(), inst->drawObjects.size());
}

void SceneGraph::DestroyDrawObject(const DrawObjectHandle a_Handle)
{
	inst->transformPool.FreeTransform(inst->drawObjects[a_Handle.handle].transformHandle);
	inst->drawObjects.erase(a_Handle.handle);
}

Transform& SceneGraph::GetTransform(const DrawObjectHandle a_Handle) const
{
	return inst->transformPool.GetTransform(inst->drawObjects[a_Handle.handle].transformHandle);
}

const RDescriptor SceneGraph::GetSceneDescriptor() const
{
	return inst->sceneDescriptor;
}

const RDescriptor SceneGraph::GetMeshDescriptor() const
{
	return inst->meshDescriptor;
}

const PipelineHandle SceneGraph::GetPipelineHandle() const
{
	return inst->meshPipeline;
}
#include "RenderFrontend.h"
#include "ShaderCompiler.h"

#include "Transform.h"
#include "OS/Program.h"
#include "LightSystem.h"
#include "imgui_impl_CrossRenderer.h"
#include "Editor.h"

#include "Storage/Slotmap.h"

#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#pragma warning (pop)

using namespace BB;
using namespace BB::Render;

void LoadglTFModel(Allocator a_TempAllocator, Allocator a_SystemAllocator, Model& a_Model, UploadBuffer& a_UploadBuffer, const RecordingCommandListHandle a_TransferCmdList, const char* a_Path);
FreelistAllocator_t s_SystemAllocator{ mbSize * 4 };
static TemporaryAllocator s_TempAllocator{ s_SystemAllocator };

struct Render_inst
{
	Render_inst(
		const RenderBufferCreateInfo& a_VertexBufferInfo,
		const RenderBufferCreateInfo& a_IndexBufferInfo,
		const DescriptorHeapCreateInfo& a_DescriptorManagerInfo,
		const uint32_t a_BackbufferAmount)
		:	vertexBuffer(a_VertexBufferInfo),
			indexBuffer(a_IndexBufferInfo),
			descriptorManager(s_SystemAllocator, a_DescriptorManagerInfo, a_BackbufferAmount),
			models(s_SystemAllocator, 64)
	{
		frameBufferAmount = a_BackbufferAmount;
	}

	uint32_t swapchainWidth = 0;
	uint32_t swapchainHeight = 0;

	uint32_t frameBufferAmount = 0;
	const uint32_t modelMatrixMax = 1024;

	RENDER_API renderAPI = RENDER_API::NONE;

	DescriptorManager descriptorManager;
	LinearRenderBuffer vertexBuffer;
	LinearRenderBuffer indexBuffer;

	Slotmap<Model> models;
};



struct GlobalInfo
{
	LightSystem* lightSystem;

	CameraRenderData* cameraData = nullptr;
	BaseFrameInfo* perFrameInfo = nullptr;
	uint64_t perFrameBufferSize = 0;

	RBufferHandle perFrameBuffer{};
	RBufferHandle perFrameTransferBuffer{};

	void* transferBufferStart = nullptr;
	void* transferBufferBaseFrameInfoStart = nullptr;
	void* transferBufferCameraStart = nullptr;
	void* transferBufferMatrixStart = nullptr;
};

CommandQueueHandle graphicsQueue;
CommandQueueHandle transferQueue;

CommandAllocatorHandle t_CommandAllocators[3];
CommandAllocatorHandle t_TransferAllocator[3];

CommandListHandle t_GraphicCommands[3];
CommandListHandle t_TransferCommands[3];

RecordingCommandListHandle t_RecordingGraphics;
RecordingCommandListHandle t_RecordingTransfer;

RFenceHandle t_SwapchainFence[3];

RDescriptor t_Descriptor1;
RDescriptor t_MeshDescriptor;
PipelineHandle t_Pipeline;

DescriptorAllocation sceneDescAllocation;

UploadBuffer* t_UploadBuffer;

RImageHandle t_ExampleImage;
RImageHandle t_DepthImage;

static FrameIndex s_CurrentFrame;

static Render_inst* s_RenderInst;
static GlobalInfo s_GlobalInfo;

static const uint64_t PERFRAME_TRANSFER_BUFFER_SIZE =
sizeof(BaseFrameInfo) +
sizeof(CameraRenderData) +
sizeof(ModelBufferInfo) * s_RenderInst.modelMatrixMax;

static void UpdateSceneDescriptors()
{
	const size_t t_BufferOffset = PERFRAME_TRANSFER_BUFFER_SIZE * s_CurrentFrame;

	FixedArray<WriteDescriptorData, 3> t_WriteDatas;
	WriteDescriptorInfos t_BufferUpdate{};
	t_BufferUpdate.allocation = sceneDescAllocation;
	t_BufferUpdate.descriptorHandle = t_Descriptor1;
	t_BufferUpdate.data = BB::Slice(t_WriteDatas.data(), t_WriteDatas.size());

	t_WriteDatas[0].binding = 0;
	t_WriteDatas[0].descriptorIndex = 0;
	t_WriteDatas[0].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
	t_WriteDatas[0].buffer.buffer = s_GlobalInfo.perFrameBuffer;
	t_WriteDatas[0].buffer.offset = t_BufferOffset;
	t_WriteDatas[0].buffer.range = sizeof(BaseFrameInfo);

	t_WriteDatas[1].binding = 1;
	t_WriteDatas[1].descriptorIndex = 0;
	t_WriteDatas[1].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
	t_WriteDatas[1].buffer.buffer = s_GlobalInfo.perFrameBuffer;
	t_WriteDatas[1].buffer.offset = sizeof(BaseFrameInfo) + t_BufferOffset;
	t_WriteDatas[1].buffer.range = sizeof(CameraRenderData);

	t_WriteDatas[2].binding = 2;
	t_WriteDatas[2].descriptorIndex = 0;
	t_WriteDatas[2].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
	t_WriteDatas[2].buffer.buffer = s_GlobalInfo.perFrameBuffer;
	t_WriteDatas[2].buffer.offset = sizeof(BaseFrameInfo) + sizeof(CameraRenderData) + t_BufferOffset;
	t_WriteDatas[2].buffer.range = sizeof(ModelBufferInfo) * s_RenderInst->modelMatrixMax;

	RenderBackend::WriteDescriptors(t_BufferUpdate);
}

void Draw3DFrame()
{
	s_GlobalInfo.perFrameInfo->ambientLight = { 1.0f, 1.0f, 1.0f };
	s_GlobalInfo.perFrameInfo->ambientStrength = 0.1f;
	s_GlobalInfo.perFrameInfo->lightCount = s_GlobalInfo.lightSystem->GetLightPool().GetLightCount();

	ImGui::Render();

	//Copy the perframe buffer over.
	RenderCopyBufferInfo t_CopyInfo;
	t_CopyInfo.src = s_GlobalInfo.perFrameTransferBuffer;
	t_CopyInfo.dst = s_GlobalInfo.perFrameBuffer;
	t_CopyInfo.size = PERFRAME_TRANSFER_BUFFER_SIZE;
	t_CopyInfo.srcOffset = 0;
	t_CopyInfo.dstOffset = t_CopyInfo.size * s_CurrentFrame;

	RenderBackend::CopyBuffer(t_RecordingTransfer, t_CopyInfo);

	StartRenderingInfo t_StartRenderInfo;
	t_StartRenderInfo.viewportWidth = s_RenderInst->swapchainWidth;
	t_StartRenderInfo.viewportHeight = s_RenderInst->swapchainHeight;
	t_StartRenderInfo.colorLoadOp = RENDER_LOAD_OP::CLEAR;
	t_StartRenderInfo.colorStoreOp = RENDER_STORE_OP::STORE;
	t_StartRenderInfo.colorInitialLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
	t_StartRenderInfo.colorFinalLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	t_StartRenderInfo.clearColor[0] = 1.0f;
	t_StartRenderInfo.clearColor[1] = 0.0f;
	t_StartRenderInfo.clearColor[2] = 0.0f;
	t_StartRenderInfo.clearColor[3] = 1.0f;
	t_StartRenderInfo.depthStencil = t_DepthImage;

	//Record rendering commands.
	RenderBackend::StartRendering(t_RecordingGraphics, t_StartRenderInfo);

	
	EndRenderingInfo t_EndRenderingInfo{};
	t_EndRenderingInfo.colorInitialLayout = t_StartRenderInfo.colorFinalLayout;
	t_EndRenderingInfo.colorFinalLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	RenderBackend::EndRendering(t_RecordingGraphics, t_EndRenderingInfo);
	{
		StartRenderingInfo t_ImguiStart;
		t_ImguiStart.viewportWidth = s_RenderInst->swapchainWidth;
		t_ImguiStart.viewportHeight = s_RenderInst->swapchainHeight;
		t_ImguiStart.colorLoadOp = RENDER_LOAD_OP::LOAD;
		t_ImguiStart.colorStoreOp = RENDER_STORE_OP::STORE;
		t_ImguiStart.colorInitialLayout = t_EndRenderingInfo.colorFinalLayout;
		t_ImguiStart.colorFinalLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		t_ImguiStart.clearColor[0] = 1.0f;
		t_ImguiStart.clearColor[1] = 0.0f;
		t_ImguiStart.clearColor[2] = 0.0f;
		t_ImguiStart.clearColor[3] = 1.0f;
		RenderBackend::StartRendering(t_RecordingGraphics, t_ImguiStart);

		ImDrawData* t_DrawData = ImGui::GetDrawData();
		ImGui_ImplCross_RenderDrawData(*t_DrawData, t_RecordingGraphics, t_RecordingTransfer);

		EndRenderingInfo t_ImguiEnd{};
		t_ImguiEnd.colorInitialLayout = t_ImguiStart.colorFinalLayout;
		t_ImguiEnd.colorFinalLayout = RENDER_IMAGE_LAYOUT::PRESENT;
		RenderBackend::EndRendering(t_RecordingGraphics, t_ImguiEnd);
	}
	RenderBackend::EndCommandList(t_RecordingGraphics);
}

void BB::Render::InitRenderer(const RenderInitInfo& a_InitInfo)
{
	Shader::InitShaderCompiler();

	BB::Array<RENDER_EXTENSIONS> t_Extensions{ s_TempAllocator };
	t_Extensions.emplace_back(RENDER_EXTENSIONS::STANDARD_VULKAN_INSTANCE);
	//t_Extensions.emplace_back(RENDER_EXTENSIONS::PHYSICAL_DEVICE_EXTRA_PROPERTIES); Now all these are included in STANDARD_VULKAN_INSTANCE
	if (a_InitInfo.debug)
	{
		t_Extensions.emplace_back(RENDER_EXTENSIONS::DEBUG);
	}
	BB::Array<RENDER_EXTENSIONS> t_DeviceExtensions{ s_TempAllocator };
	t_DeviceExtensions.emplace_back(RENDER_EXTENSIONS::STANDARD_VULKAN_DEVICE);
	t_DeviceExtensions.emplace_back(RENDER_EXTENSIONS::PIPELINE_EXTENDED_DYNAMIC_STATE);

	int t_WindowWidth;
	int t_WindowHeight;
	GetWindowSize(a_InitInfo.windowHandle, t_WindowWidth, t_WindowHeight);

	RenderBackendCreateInfo t_BackendCreateInfo;
	t_BackendCreateInfo.getApiFuncPtr = (PFN_RenderGetAPIFunctions)LibLoadFunc(a_InitInfo.renderDll, "GetRenderAPIFunctions");
	t_BackendCreateInfo.extensions = t_Extensions;
	t_BackendCreateInfo.deviceExtensions = t_DeviceExtensions;
	t_BackendCreateInfo.windowHandle = a_InitInfo.windowHandle;
	t_BackendCreateInfo.validationLayers = a_InitInfo.debug;
	t_BackendCreateInfo.appName = "TestName";
	t_BackendCreateInfo.engineName = "TestEngine";
	t_BackendCreateInfo.windowWidth = static_cast<uint32_t>(t_WindowWidth);
	t_BackendCreateInfo.windowHeight = static_cast<uint32_t>(t_WindowHeight);

	RenderBackend::InitBackend(t_BackendCreateInfo, s_SystemAllocator);
	{
		//Write some background info.
		constexpr size_t SUBHEAPSIZE = 64 * 1024;
		DescriptorHeapCreateInfo t_HeapInfo{};
		t_HeapInfo.name = "Resource Heap";
		t_HeapInfo.descriptorCount = SUBHEAPSIZE;
		t_HeapInfo.isSampler = false;

		RenderBufferCreateInfo t_VertexBufferInfo;
		t_VertexBufferInfo.name = "Big vertex buffer";
		t_VertexBufferInfo.size = mbSize * 128;
		t_VertexBufferInfo.usage = RENDER_BUFFER_USAGE::STORAGE;
		t_VertexBufferInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;

		RenderBufferCreateInfo t_IndexBufferInfo;
		t_IndexBufferInfo.name = "Big index buffer";
		t_IndexBufferInfo.size = mbSize * 32;
		t_IndexBufferInfo.usage = RENDER_BUFFER_USAGE::INDEX;
		t_IndexBufferInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;

		s_RenderInst = BBnew(s_SystemAllocator, Render_inst)(t_VertexBufferInfo, t_IndexBufferInfo, t_HeapInfo, RenderBackend::GetFrameBufferAmount());
		s_RenderInst->renderAPI = a_InitInfo.renderAPI;
		s_RenderInst->swapchainWidth = t_BackendCreateInfo.windowWidth;
		s_RenderInst->swapchainHeight = t_BackendCreateInfo.windowHeight;
	}
#pragma region LightSystem
	{
		constexpr size_t LIGHT_COUNT_MAX = 1024;
		constexpr size_t LIGHT_ALLOC_SIZE = LIGHT_COUNT_MAX * sizeof(Light);

		s_GlobalInfo.lightSystem = BBnew(s_SystemAllocator, LightSystem)(LIGHT_COUNT_MAX);
	}
#pragma endregion //LightSystem

#pragma region PipelineCreation
	{
		RenderBufferCreateInfo t_PerFrameTransferBuffer;
		t_PerFrameTransferBuffer.name = "per-frame transfer data";
		t_PerFrameTransferBuffer.size = PERFRAME_TRANSFER_BUFFER_SIZE;
		t_PerFrameTransferBuffer.usage = RENDER_BUFFER_USAGE::STAGING;
		t_PerFrameTransferBuffer.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
		s_GlobalInfo.perFrameTransferBuffer = RenderBackend::CreateBuffer(t_PerFrameTransferBuffer);
		s_GlobalInfo.transferBufferStart = RenderBackend::MapMemory(s_GlobalInfo.perFrameTransferBuffer);
		s_GlobalInfo.transferBufferBaseFrameInfoStart = s_GlobalInfo.transferBufferStart;
		s_GlobalInfo.transferBufferCameraStart = Pointer::Add(s_GlobalInfo.transferBufferBaseFrameInfoStart, sizeof(BaseFrameInfo));
		s_GlobalInfo.transferBufferMatrixStart = Pointer::Add(s_GlobalInfo.transferBufferCameraStart, sizeof(CameraRenderData));

		s_GlobalInfo.perFrameInfo = reinterpret_cast<BaseFrameInfo*>(s_GlobalInfo.transferBufferBaseFrameInfoStart);
		s_GlobalInfo.cameraData = reinterpret_cast<CameraRenderData*>(s_GlobalInfo.transferBufferCameraStart);

		const uint64_t t_perFrameBufferEntireSize = PERFRAME_TRANSFER_BUFFER_SIZE * s_RenderInst->frameBufferAmount;

		RenderBufferCreateInfo t_PerFrameBuffer;
		t_PerFrameBuffer.name = "per-frame buffer";
		t_PerFrameBuffer.size = t_perFrameBufferEntireSize;
		t_PerFrameBuffer.usage = RENDER_BUFFER_USAGE::STORAGE;
		t_PerFrameBuffer.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
		s_GlobalInfo.perFrameBuffer = RenderBackend::CreateBuffer(t_PerFrameBuffer);
	}

	int x, y, c;
	stbi_uc* t_Pixels = stbi_load("Resources/Textures/DuckCM.png", &x, &y, &c, 4);
	BB_ASSERT(t_Pixels, "Failed to load test image!");
	STBI_FREE(t_Pixels); //HACK, will fix later.
	{
		RenderImageCreateInfo t_ImageInfo{};
		t_ImageInfo.name = "example texture";
		t_ImageInfo.width = static_cast<uint32_t>(x);
		t_ImageInfo.height = static_cast<uint32_t>(y);
		t_ImageInfo.depth = 1;
		t_ImageInfo.arrayLayers = 1;
		t_ImageInfo.mipLevels = 1;
		t_ImageInfo.tiling = RENDER_IMAGE_TILING::OPTIMAL;
		t_ImageInfo.type = RENDER_IMAGE_TYPE::TYPE_2D;
		t_ImageInfo.format = RENDER_IMAGE_FORMAT::RGBA8_SRGB;

		t_ExampleImage = RenderBackend::CreateImage(t_ImageInfo);
	}

	{ //depth create info
		RenderImageCreateInfo t_DepthInfo{};
		t_DepthInfo.name = "depth image";
		t_DepthInfo.width = s_RenderInst->swapchainWidth;
		t_DepthInfo.height =s_RenderInst->swapchainHeight;
		t_DepthInfo.depth = 1;
		t_DepthInfo.arrayLayers = 1;
		t_DepthInfo.mipLevels = 1;
		t_DepthInfo.tiling = RENDER_IMAGE_TILING::OPTIMAL;
		t_DepthInfo.type = RENDER_IMAGE_TYPE::TYPE_2D;
		t_DepthInfo.format = RENDER_IMAGE_FORMAT::DEPTH_STENCIL;

		t_DepthImage = RenderBackend::CreateImage(t_DepthInfo);
	}

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
	
	{
		RenderDescriptorCreateInfo t_CreateInfo{};
		t_CreateInfo.name = "3d scene descriptor";
		FixedArray<DescriptorBinding, 5> t_DescBinds;
		t_CreateInfo.bindings = BB::Slice(t_DescBinds.data(), t_DescBinds.size());
		{//Per frame info Bind
			t_DescBinds[0].binding = 0;
			t_DescBinds[0].descriptorCount = 1;
			t_DescBinds[0].stage = RENDER_SHADER_STAGE::ALL;
			t_DescBinds[0].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
			t_DescBinds[0].flags = RENDER_DESCRIPTOR_FLAG::NONE;
		}
		{//Cam Bind
			t_DescBinds[1].binding = 1;
			t_DescBinds[1].descriptorCount = 1;
			t_DescBinds[1].stage = RENDER_SHADER_STAGE::VERTEX;
			t_DescBinds[1].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
			t_DescBinds[1].flags = RENDER_DESCRIPTOR_FLAG::NONE;
		}
		{//Model Bind
			t_DescBinds[2].binding = 2;
			t_DescBinds[2].descriptorCount = 1;
			t_DescBinds[2].stage = RENDER_SHADER_STAGE::VERTEX;
			t_DescBinds[2].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
			t_DescBinds[2].flags = RENDER_DESCRIPTOR_FLAG::NONE;
		}
		{//Light Binding
			t_DescBinds[3].binding = 3;
			t_DescBinds[3].descriptorCount = 1;
			t_DescBinds[3].stage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
			t_DescBinds[3].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
			t_DescBinds[3].flags = RENDER_DESCRIPTOR_FLAG::NONE;
		}
		{//Image Binds
			t_DescBinds[4].binding = 4;
			t_DescBinds[4].descriptorCount = DESCRIPTOR_IMAGE_MAX;
			t_DescBinds[4].stage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
			t_DescBinds[4].type = RENDER_DESCRIPTOR_TYPE::IMAGE;
			t_DescBinds[4].flags = RENDER_DESCRIPTOR_FLAG::BINDLESS;
		}

		t_Descriptor1 = RenderBackend::CreateDescriptor(t_CreateInfo);
		sceneDescAllocation = Render::AllocateDescriptor(t_Descriptor1);
	}

	{
		RenderDescriptorCreateInfo t_CreateInfo{};
		t_CreateInfo.name = "3d mesh descriptor";
		FixedArray<DescriptorBinding, 1> t_DescBinds;
		t_CreateInfo.bindings = BB::Slice(t_DescBinds.data(), t_DescBinds.size());

		t_DescBinds[0].binding = 0;
		t_DescBinds[0].descriptorCount = 1;
		t_DescBinds[0].stage = RENDER_SHADER_STAGE::VERTEX;
		t_DescBinds[0].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
		t_DescBinds[0].flags = RENDER_DESCRIPTOR_FLAG::NONE;

		t_MeshDescriptor = RenderBackend::CreateDescriptor(t_CreateInfo);
	}

	{
		FixedArray<WriteDescriptorData, 1> t_WriteDatas;
		WriteDescriptorInfos t_BufferUpdate{};
		t_BufferUpdate.allocation = sceneDescAllocation;
		t_BufferUpdate.descriptorHandle = t_Descriptor1;
		t_BufferUpdate.data = BB::Slice(t_WriteDatas.data(), t_WriteDatas.size());

		//example image
		t_WriteDatas[0].binding = 4;
		t_WriteDatas[0].descriptorIndex = 0;
		t_WriteDatas[0].type = RENDER_DESCRIPTOR_TYPE::IMAGE;
		t_WriteDatas[0].image.image = t_ExampleImage;
		t_WriteDatas[0].image.layout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
		t_WriteDatas[0].image.sampler = nullptr;

		RenderBackend::WriteDescriptors(t_BufferUpdate);

		s_GlobalInfo.lightSystem->UpdateDescriptor(t_Descriptor1, sceneDescAllocation);
	}

	t_BasicPipe.BindDescriptor(t_Descriptor1);
	t_BasicPipe.BindDescriptor(t_MeshDescriptor);

	const wchar_t* t_ShaderPath[2]{};
	t_ShaderPath[0] = L"Resources/Shaders/HLSLShaders/DebugVert.hlsl";
	t_ShaderPath[1] = L"Resources/Shaders/HLSLShaders/DebugFrag.hlsl";

	ShaderCodeHandle t_ShaderHandles[2];
	t_ShaderHandles[0] = Shader::CompileShader(
		t_ShaderPath[0],
		L"main",
		RENDER_SHADER_STAGE::VERTEX,
		s_RenderInst->renderAPI);
	t_ShaderHandles[1] = Shader::CompileShader(
		t_ShaderPath[1],
		L"main",
		RENDER_SHADER_STAGE::FRAGMENT_PIXEL,
		s_RenderInst->renderAPI);

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

	t_Pipeline = t_BasicPipe.BuildPipeline();

#pragma endregion //PipelineCreation
	RenderCommandQueueCreateInfo t_QueueCreateInfo{};
	t_QueueCreateInfo.name = "Graphics queue";
	t_QueueCreateInfo.queue = RENDER_QUEUE_TYPE::GRAPHICS;
	t_QueueCreateInfo.flags = RENDER_FENCE_FLAGS::CREATE_SIGNALED;
	graphicsQueue = RenderBackend::CreateCommandQueue(t_QueueCreateInfo);
	t_QueueCreateInfo.name = "Transfer queue";
	t_QueueCreateInfo.queue = RENDER_QUEUE_TYPE::TRANSFER_COPY;
	transferQueue = RenderBackend::CreateCommandQueue(t_QueueCreateInfo);

	RenderCommandAllocatorCreateInfo t_AllocatorCreateInfo{};
	t_AllocatorCreateInfo.name = "Graphics command allocator";
	t_AllocatorCreateInfo.commandListCount = 10;
	t_AllocatorCreateInfo.queueType = RENDER_QUEUE_TYPE::GRAPHICS;
	t_CommandAllocators[0] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
	t_CommandAllocators[1] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
	t_CommandAllocators[2] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);

	t_AllocatorCreateInfo.name = "Transfer command allocator";
	t_AllocatorCreateInfo.queueType = RENDER_QUEUE_TYPE::TRANSFER_COPY;
	t_TransferAllocator[0] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
	t_TransferAllocator[1] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
	t_TransferAllocator[2] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);

	RenderCommandListCreateInfo t_CmdCreateInfo{};
	t_CmdCreateInfo.name = "Graphic Commandlist";
	t_CmdCreateInfo.commandAllocator = t_CommandAllocators[0];
	t_GraphicCommands[0] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	t_CmdCreateInfo.commandAllocator = t_CommandAllocators[1];
	t_GraphicCommands[1] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	t_CmdCreateInfo.commandAllocator = t_CommandAllocators[2];
	t_GraphicCommands[2] = RenderBackend::CreateCommandList(t_CmdCreateInfo);

	//just reuse the struct above.
	t_CmdCreateInfo.name = "Transfer Commandlist";
	t_CmdCreateInfo.commandAllocator = t_TransferAllocator[0];
	t_TransferCommands[0] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	t_CmdCreateInfo.commandAllocator = t_TransferAllocator[1];
	t_TransferCommands[1] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	t_CmdCreateInfo.commandAllocator = t_TransferAllocator[2];
	t_TransferCommands[2] = RenderBackend::CreateCommandList(t_CmdCreateInfo);

	FenceCreateInfo t_CreateInfo{};
	t_CreateInfo.flags = RENDER_FENCE_FLAGS::CREATE_SIGNALED;

	for (size_t i = 0; i < _countof(t_SwapchainFence); i++)
		t_SwapchainFence[i] = RenderBackend::CreateFence(t_CreateInfo);

	//Create upload buffer.
	constexpr const uint64_t UPLOAD_BUFFER_SIZE = static_cast<uint64_t>(mbSize * 32);
	t_UploadBuffer = BBnew(s_SystemAllocator, UploadBuffer)(UPLOAD_BUFFER_SIZE, "upload buffer");

	{//implement imgui here.
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsClassic();

		const wchar_t* t_ShaderPath[2]{};
		t_ShaderPath[0] = L"Resources/Shaders/HLSLShaders/ImguiVert.hlsl";
		t_ShaderPath[1] = L"Resources/Shaders/HLSLShaders/ImguiFrag.hlsl";

		ShaderCodeHandle t_ImguiShaders[2];
		t_ImguiShaders[0] = Shader::CompileShader(
			t_ShaderPath[0],
			L"main",
			RENDER_SHADER_STAGE::VERTEX,
			s_RenderInst->renderAPI);
		t_ImguiShaders[1] = Shader::CompileShader(
			t_ShaderPath[1],
			L"main",
			RENDER_SHADER_STAGE::FRAGMENT_PIXEL,
			s_RenderInst->renderAPI);

		ShaderCreateInfo t_ShaderInfos[2];
		t_ShaderInfos[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;
		Shader::GetShaderCodeBuffer(t_ImguiShaders[0], t_ShaderInfos[0].buffer);

		t_ShaderInfos[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
		Shader::GetShaderCodeBuffer(t_ImguiShaders[1], t_ShaderInfos[1].buffer);

		ImGui_ImplCross_InitInfo t_ImguiInfo{};
		t_ImguiInfo.imageCount = s_RenderInst->frameBufferAmount;
		t_ImguiInfo.minImageCount = s_RenderInst->frameBufferAmount;
		t_ImguiInfo.vertexShader = t_ImguiShaders[0];
		t_ImguiInfo.fragmentShader = t_ImguiShaders[1];

		t_ImguiInfo.window = a_InitInfo.windowHandle;
		ImGui_ImplCross_Init(t_ImguiInfo);

		t_RecordingGraphics = RenderBackend::StartCommandList(t_GraphicCommands[s_CurrentFrame]);
		ImGui_ImplCross_CreateFontsTexture(t_RecordingGraphics, *t_UploadBuffer);
		ExecuteCommandsInfo* t_ExecuteInfo = BBnew(
			s_TempAllocator,
			ExecuteCommandsInfo);
		RenderBackend::EndCommandList(t_RecordingGraphics);

		t_ExecuteInfo[0] = {};
		t_ExecuteInfo[0].commands = &t_GraphicCommands[s_CurrentFrame];
		t_ExecuteInfo[0].commandCount = 1;
		t_ExecuteInfo[0].signalQueues = &graphicsQueue;
		t_ExecuteInfo[0].signalQueueCount = 1;

		RenderBackend::ExecuteCommands(graphicsQueue, t_ExecuteInfo, 1);

		for (size_t i = 0; i < _countof(t_ImguiShaders); i++)
		{
			Shader::ReleaseShaderCode(t_ImguiShaders[i]);
		}
	}

	for (size_t i = 0; i < _countof(t_ShaderHandles); i++)
	{
		Shader::ReleaseShaderCode(t_ShaderHandles[i]);
	}

	CommandQueueHandle t_Queues[1]{ graphicsQueue };
	RenderWaitCommandsInfo t_WaitInfo{};
	t_WaitInfo.queues = Slice(t_Queues, _countof(t_Queues));
	RenderBackend::WaitCommands(t_WaitInfo);
}

void BB::Render::DestroyRenderer()
{
	{
		CommandQueueHandle t_Queues[2]{ graphicsQueue, transferQueue };
		RenderWaitCommandsInfo t_WaitInfo{};
		t_WaitInfo.queues = Slice(t_Queues, _countof(t_Queues));
		RenderBackend::WaitCommands(t_WaitInfo);
	}

	for (auto it = s_RenderInst->models.begin(); it < s_RenderInst->models.end(); it++)
	{
		
	}
	BBfree(s_SystemAllocator, t_UploadBuffer);

	RenderBackend::DestroyDescriptor(t_Descriptor1);
	RenderBackend::DestroyBuffer(s_GlobalInfo.perFrameBuffer);
	RenderBackend::UnmapMemory(s_GlobalInfo.perFrameTransferBuffer);
	RenderBackend::DestroyBuffer(s_GlobalInfo.perFrameTransferBuffer);

	RenderBackend::DestroyPipeline(t_Pipeline);
	for (size_t i = 0; i < _countof(t_GraphicCommands); i++)
	{
		RenderBackend::DestroyCommandList(t_GraphicCommands[i]);
	}
	for (size_t i = 0; i < _countof(t_TransferCommands); i++)
	{
		RenderBackend::DestroyCommandList(t_TransferCommands[i]);
	}
	for (size_t i = 0; i < _countof(t_CommandAllocators); i++)
	{
		RenderBackend::DestroyCommandAllocator(t_CommandAllocators[i]);
	}
	for (size_t i = 0; i < _countof(t_TransferAllocator); i++)
	{
		RenderBackend::DestroyCommandAllocator(t_TransferAllocator[i]);
	}

	RenderBackend::DestroyBackend();
}

RDescriptorHeap BB::Render::GetGPUHeap(const uint32_t a_FrameNum)
{
	return s_RenderInst->descriptorManager.GetGPUHeap(a_FrameNum);
}

DescriptorAllocation BB::Render::AllocateDescriptor(const RDescriptor a_Descriptor)
{
	return s_RenderInst->descriptorManager.Allocate(a_Descriptor);
}

void BB::Render::UploadDescriptorsToGPU(const uint32_t a_FrameNum)
{
	s_RenderInst->descriptorManager.UploadToGPUHeap(a_FrameNum);
}

RenderBufferPart BB::Render::AllocateFromVertexBuffer(const size_t a_Size)
{
	return s_RenderInst->vertexBuffer.SubAllocate(a_Size, __alignof(Vertex));
}

RenderBufferPart BB::Render::AllocateFromIndexBuffer(const size_t a_Size)
{
	return s_RenderInst->indexBuffer.SubAllocate(a_Size, __alignof(uint32_t));
}

void BB::Render::Update(const float a_DeltaTime)
{
	s_GlobalInfo.lightSystem->Editor();
	RenderBackend::DisplayDebugInfo();

	UpdateSceneDescriptors();
	Draw3DFrame();
}

void BB::Render::SetProjection(const glm::mat4& a_Proj)
{
	s_GlobalInfo.cameraData->projection = a_Proj;
}

void BB::Render::SetView(const glm::mat4& a_View)
{
	s_GlobalInfo.cameraData->view = a_View;
}

void* BB::Render::GetMatrixBufferSpace(uint32_t& a_MatrixSpace)
{
	a_MatrixSpace = s_RenderInst->modelMatrixMax;
	return s_GlobalInfo.transferBufferMatrixStart;
}

RModelHandle BB::Render::CreateRawModel(const CreateRawModelInfo& a_CreateInfo)
{
	Model t_Model;

	//t_Model.pipelineHandle = a_CreateInfo.pipeline;
	t_Model.pipelineHandle = t_Pipeline;

	{
		RenderTransitionImageInfo t_ImageTransInfo{};
		t_ImageTransInfo.srcMask = RENDER_ACCESS_MASK::NONE;
		t_ImageTransInfo.dstMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
		t_ImageTransInfo.image = t_ExampleImage;
		t_ImageTransInfo.oldLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
		t_ImageTransInfo.newLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
		t_ImageTransInfo.layerCount = 1;
		t_ImageTransInfo.levelCount = 1;
		t_ImageTransInfo.baseArrayLayer = 0;
		t_ImageTransInfo.baseMipLevel = 0;
		t_ImageTransInfo.srcStage = RENDER_PIPELINE_STAGE::TOP_OF_PIPELINE;
		t_ImageTransInfo.dstStage = RENDER_PIPELINE_STAGE::TRANSFER;
		RenderBackend::TransitionImage(t_RecordingGraphics, t_ImageTransInfo);

		constexpr size_t TEXTURE_BYTE_ALIGNMENT = 512;
		int x, y, c;
		//hacky way, whatever we do it for now.
		stbi_uc* t_Pixels = stbi_load(a_CreateInfo.imagePath, &x, &y, &c, 4);
		ImageReturnInfo t_ImageInfo = RenderBackend::GetImageInfo(t_ExampleImage);

		//Add some extra for alignment.
		size_t t_AlignedOffset = Pointer::AlignPad(t_UploadBuffer->GetCurrentOffset(), TEXTURE_BYTE_ALIGNMENT);
		size_t t_AllocAddition = t_AlignedOffset - t_UploadBuffer->GetCurrentOffset();
		UploadBufferChunk t_StageBuffer = t_UploadBuffer->Alloc(t_ImageInfo.allocInfo.imageAllocByteSize + t_AllocAddition);
		const UINT64 t_SourcePitch = static_cast<UINT64>(t_ImageInfo.width) * sizeof(uint32_t);

		void* t_ImageSrc = t_Pixels;
		void* t_ImageDst = Pointer::Add(t_StageBuffer.memory, t_AllocAddition);
		//Layouts should be only 1 right now due to mips.
		for (uint32_t i = 0; i < t_ImageInfo.allocInfo.footHeight; i++)
		{
			memcpy(t_ImageDst, t_ImageSrc, t_SourcePitch);

			t_ImageSrc = Pointer::Add(t_ImageSrc, t_SourcePitch);
			t_ImageDst = Pointer::Add(t_ImageDst, t_ImageInfo.allocInfo.footRowPitch);
		}

		RenderCopyBufferImageInfo t_CopyImage{};
		t_CopyImage.srcBuffer = t_UploadBuffer->Buffer();
		t_CopyImage.srcBufferOffset = static_cast<uint32_t>(t_AlignedOffset);
		t_CopyImage.dstImage = t_ExampleImage;
		t_CopyImage.dstImageInfo.sizeX = static_cast<uint32_t>(x);
		t_CopyImage.dstImageInfo.sizeY = static_cast<uint32_t>(y);
		t_CopyImage.dstImageInfo.sizeZ = 1;
		t_CopyImage.dstImageInfo.offsetX = 0;
		t_CopyImage.dstImageInfo.offsetY = 0;
		t_CopyImage.dstImageInfo.offsetZ = 0;
		t_CopyImage.dstImageInfo.layerCount = 1;
		t_CopyImage.dstImageInfo.mipLevel = 0;
		t_CopyImage.dstImageInfo.baseArrayLayer = 0;
		t_CopyImage.dstImageInfo.layout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;

		RenderBackend::CopyBufferImage(t_RecordingGraphics, t_CopyImage);


		t_ImageTransInfo.srcMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
		t_ImageTransInfo.dstMask = RENDER_ACCESS_MASK::SHADER_READ;
		t_ImageTransInfo.oldLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
		t_ImageTransInfo.newLayout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
		t_ImageTransInfo.srcStage = RENDER_PIPELINE_STAGE::TRANSFER;
		t_ImageTransInfo.dstStage = RENDER_PIPELINE_STAGE::FRAGMENT_SHADER;
		RenderBackend::TransitionImage(t_RecordingGraphics, t_ImageTransInfo);
	}

	{
		UploadBufferChunk t_StageBuffer = t_UploadBuffer->Alloc(a_CreateInfo.vertices.sizeInBytes());
		memcpy(t_StageBuffer.memory, a_CreateInfo.vertices.data(), a_CreateInfo.vertices.sizeInBytes());

		t_Model.vertexView = AllocateFromVertexBuffer(a_CreateInfo.vertices.sizeInBytes());

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = t_UploadBuffer->Buffer();
		t_CopyInfo.dst = t_Model.vertexView.buffer;
		t_CopyInfo.srcOffset = t_StageBuffer.bufferOffset;
		t_CopyInfo.dstOffset = t_Model.vertexView.offset;
		t_CopyInfo.size = t_Model.vertexView.size;

		RenderBackend::CopyBuffer(t_RecordingTransfer, t_CopyInfo);
	}

	{
		UploadBufferChunk t_StageBuffer = t_UploadBuffer->Alloc(a_CreateInfo.indices.sizeInBytes());
		memcpy(t_StageBuffer.memory, a_CreateInfo.indices.data(), a_CreateInfo.indices.sizeInBytes());

		t_Model.indexView = AllocateFromIndexBuffer(a_CreateInfo.indices.sizeInBytes());

		RenderCopyBufferInfo t_CopyInfo;
		t_CopyInfo.src = t_UploadBuffer->Buffer();
		t_CopyInfo.dst = t_Model.indexView.buffer;
		t_CopyInfo.srcOffset = t_StageBuffer.bufferOffset;
		t_CopyInfo.dstOffset = t_Model.indexView.offset;
		t_CopyInfo.size = t_Model.indexView.size;

		RenderBackend::CopyBuffer(t_RecordingTransfer, t_CopyInfo);
	}

	{ //descriptor allocation
		t_Model.meshDescriptor = t_MeshDescriptor;
		t_Model.descAllocation = Render::AllocateDescriptor(t_Model.meshDescriptor);

		WriteDescriptorData t_WriteData{};
		t_WriteData.binding = 0;
		t_WriteData.descriptorIndex = 0;
		t_WriteData.type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
		t_WriteData.buffer.buffer = t_Model.vertexView.buffer;
		t_WriteData.buffer.offset = t_Model.vertexView.offset;
		t_WriteData.buffer.range = t_Model.vertexView.size;
		WriteDescriptorInfos t_WriteInfos{};
		t_WriteInfos.allocation = t_Model.descAllocation;
		t_WriteInfos.descriptorHandle = t_MeshDescriptor;
		t_WriteInfos.data = Slice(&t_WriteData, 1);
		RenderBackend::WriteDescriptors(t_WriteInfos);
	}
	
	t_Model.linearNodes = BBnewArr(s_SystemAllocator, 1, Model::Node);
	t_Model.linearNodeCount = 1;
	t_Model.nodes = t_Model.linearNodes;
	t_Model.nodeCount = 1;

	t_Model.primitives = BBnewArr(s_SystemAllocator, 1, Model::Primitive);
	t_Model.primitiveCount = 1;
	t_Model.primitives->indexStart = 0;
	t_Model.primitives->indexCount = static_cast<uint32_t>(a_CreateInfo.indices.size());

	t_Model.meshCount = 1;
	t_Model.meshes = BBnewArr(s_SystemAllocator, 1, Model::Mesh);
	t_Model.meshes->primitiveCount = 1;
	t_Model.meshes->primitiveOffset = 0;

	return RModelHandle(s_RenderInst->models.insert(t_Model).handle);
}

RModelHandle BB::Render::LoadModel(const LoadModelInfo& a_LoadInfo)
{
	Model t_Model;

	t_Model.pipelineHandle = t_Pipeline;

	switch (a_LoadInfo.modelType)
	{
	case BB::MODEL_TYPE::GLTF:
		LoadglTFModel(s_TempAllocator,
			s_SystemAllocator,
			t_Model,
			*t_UploadBuffer,
			t_RecordingTransfer,
			a_LoadInfo.path);
		break;
	}
	
	t_Model.image = t_ExampleImage;

	return RModelHandle(s_RenderInst->models.insert(t_Model).handle);
}

const LightHandle BB::Render::AddLights(const BB::Slice<Light> a_Lights, const LIGHT_TYPE a_LightType)
{
	const LightHandle t_Lights = s_GlobalInfo.lightSystem->AddLights(a_Lights, a_LightType, t_RecordingTransfer);
	return t_Lights;
}

void BB::Render::StartFrame()
{
	StartFrameInfo t_StartInfo{};
	//We do not use these currently.
	//t_StartInfo.fences = &t_SwapchainFence[s_CurrentFrame];
	//t_StartInfo.fenceCount = 1;
	RenderBackend::StartFrame(t_StartInfo);
	//Prepare the commandallocator for a new frame
	//TODO, send a fence that waits until the image was presented.
	RenderBackend::ResetCommandAllocator(t_CommandAllocators[s_CurrentFrame]);
	RenderBackend::ResetCommandAllocator(t_TransferAllocator[s_CurrentFrame]);

	t_RecordingGraphics = RenderBackend::StartCommandList(t_GraphicCommands[s_CurrentFrame]);
	t_RecordingTransfer = RenderBackend::StartCommandList(t_TransferCommands[s_CurrentFrame]);
	ImGui_ImplCross_NewFrame();
	ImGui::NewFrame();
}

void BB::Render::EndFrame()
{
	UploadDescriptorsToGPU(s_CurrentFrame);
	RenderBackend::EndCommandList(t_RecordingTransfer);
	ImGui::EndFrame();

	ExecuteCommandsInfo* t_ExecuteInfos = BBnewArr(
		s_TempAllocator,
		2,
		ExecuteCommandsInfo);

	t_ExecuteInfos[0] = {};
	t_ExecuteInfos[0].commands = &t_TransferCommands[s_CurrentFrame];
	t_ExecuteInfos[0].commandCount = 1;
	t_ExecuteInfos[0].signalQueues = &transferQueue;
	t_ExecuteInfos[0].signalQueueCount = 1;

	RenderBackend::ExecuteCommands(transferQueue, &t_ExecuteInfos[0], 1);

	uint64_t t_WaitValue = RenderBackend::NextQueueFenceValue(transferQueue) - 1;

	//We write to vertex information (vertex buffer, index buffer and the storage buffer storing all the matrices.)
	RENDER_PIPELINE_STAGE t_WaitStage = RENDER_PIPELINE_STAGE::VERTEX_SHADER;
	t_ExecuteInfos[1] = {};
	t_ExecuteInfos[1].commands = &t_GraphicCommands[s_CurrentFrame];
	t_ExecuteInfos[1].commandCount = 1;
	t_ExecuteInfos[1].waitQueueCount = 1;
	t_ExecuteInfos[1].waitQueues = &transferQueue;
	t_ExecuteInfos[1].waitValues = &t_WaitValue;
	t_ExecuteInfos[1].waitStages = &t_WaitStage;

	RenderBackend::ExecutePresentCommands(graphicsQueue, t_ExecuteInfos[1]);
	PresentFrameInfo t_PresentFrame{};
	s_CurrentFrame = RenderBackend::PresentFrame(t_PresentFrame);
}

void BB::Render::ResizeWindow(const uint32_t a_X, const uint32_t a_Y)
{
	s_RenderInst->swapchainWidth = a_X;
	s_RenderInst->swapchainHeight = a_Y;
	RenderBackend::ResizeWindow(a_X, a_Y);

	RenderBackend::DestroyImage(t_DepthImage);

	{ //depth create info
		RenderImageCreateInfo t_DepthInfo{};
		t_DepthInfo.width = s_RenderInst->swapchainWidth;
		t_DepthInfo.height = s_RenderInst->swapchainHeight;
		t_DepthInfo.depth = 1;
		t_DepthInfo.arrayLayers = 1;
		t_DepthInfo.mipLevels = 1;
		t_DepthInfo.tiling = RENDER_IMAGE_TILING::OPTIMAL;
		t_DepthInfo.type = RENDER_IMAGE_TYPE::TYPE_2D;
		t_DepthInfo.format = RENDER_IMAGE_FORMAT::DEPTH_STENCIL;

		t_DepthImage = RenderBackend::CreateImage(t_DepthInfo);
	}
}


//We handle the light system externally since we might want to access the system differently later.
void BB::Editor::DisplayLightSystem(const BB::LightSystem& a_System)
{
	ImGui::Begin("Light Pool");

	if (ImGui::CollapsingHeader("lights"))
	{
		const LightPool& t_Pl = a_System.GetLightPool();
		ImGui::Text("Light amount: %u/%u", t_Pl.GetLightCount(), t_Pl.GetLightMax());
		const Slice<Light> t_Lights = t_Pl.GetLights();

		if (ImGui::Button("Rebuild lights"))
		{
			t_Pl.SubmitLightsToGPU(t_RecordingTransfer);
		}

		for (size_t i = 0; i < t_Lights.size(); i++)
		{
			if (ImGui::TreeNode((void*)(intptr_t)i, "Light %d", i))
			{
				ImGui::SliderFloat3("Position", &t_Lights[i].pos.x, -100, 100);
				ImGui::SliderFloat("Radius", &t_Lights[i].radius, 0, 10);
				ImGui::SliderFloat4("Color", &t_Lights[i].color.x, 0, 255);
				ImGui::TreePop();
			}
		}
	}

	ImGui::End();
}

#pragma warning(push, 0)
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#pragma warning (pop)

static inline void* GetAccessorDataPtr(const cgltf_accessor* a_Accessor)
{
	void* t_Data = a_Accessor->buffer_view->buffer->data;
	t_Data = Pointer::Add(t_Data, a_Accessor->buffer_view->offset);
	t_Data = Pointer::Add(t_Data, a_Accessor->offset);
	return t_Data;
}

static inline uint32_t GetChildNodeCount(const cgltf_node& a_Node)
{
	uint32_t t_NodeCount = 0;
	for (size_t i = 0; i < a_Node.children_count; i++)
	{
		t_NodeCount += GetChildNodeCount(*a_Node.children[i]);
	}
	return t_NodeCount;
}

//Maybe use own allocators for this?
void LoadglTFModel(Allocator a_TempAllocator, Allocator a_SystemAllocator, Model& a_Model, UploadBuffer& a_UploadBuffer, const RecordingCommandListHandle a_TransferCmdList, const char* a_Path)
{
	cgltf_options t_Options = {};
	cgltf_data* t_Data = { 0 };

	cgltf_result t_ParseResult = cgltf_parse_file(&t_Options, a_Path, &t_Data);

	BB_ASSERT(t_ParseResult == cgltf_result_success, "Failed to load glTF model, cgltf_parse_file.");

	cgltf_load_buffers(&t_Options, t_Data, a_Path);

	BB_ASSERT(cgltf_validate(t_Data) == cgltf_result_success, "GLTF model validation failed!");

	uint32_t t_IndexCount = 0;
	uint32_t t_VertexCount = 0;
	uint32_t t_LinearNodeCount = static_cast<uint32_t>(t_Data->nodes_count);
	uint32_t t_MeshCount = static_cast<uint32_t>(t_Data->meshes_count);
	uint32_t t_PrimitiveCount = 0;

	//Get the node count.
	for (size_t nodeIndex = 0; nodeIndex < t_Data->nodes_count; nodeIndex++)
	{
		const cgltf_node& t_Node = t_Data->nodes[nodeIndex];
		t_LinearNodeCount += GetChildNodeCount(t_Node);
	}

	//Get the sizes first for efficient allocation.
	for (size_t meshIndex = 0; meshIndex < t_Data->meshes_count; meshIndex++)
	{
		cgltf_mesh& t_Mesh = t_Data->meshes[meshIndex];
		for (size_t primitiveIndex = 0; primitiveIndex < t_Mesh.primitives_count; primitiveIndex++)
		{
			++t_PrimitiveCount;
			cgltf_primitive& t_Primitive = t_Mesh.primitives[primitiveIndex];
			t_IndexCount += static_cast<uint32_t>(t_Mesh.primitives[meshIndex].indices->count);

			for (size_t attrIndex = 0; attrIndex < t_Primitive.attributes_count; attrIndex++)
			{
				cgltf_attribute& t_Attribute = t_Primitive.attributes[attrIndex];
				if (t_Attribute.type == cgltf_attribute_type_position)
				{
					BB_ASSERT(t_Attribute.data->type == cgltf_type_vec3, "GLTF position type is not a vec3!");
					t_VertexCount += static_cast<uint32_t>(t_Attribute.data->count);
				}
			}
		}
	}

	//Maybe allocate this all in one go
	Model::Mesh* t_Meshes = BBnewArr(
		a_SystemAllocator,
		t_MeshCount,
		Model::Mesh);
	a_Model.meshes = t_Meshes;
	a_Model.meshCount = t_MeshCount;

	Model::Primitive* t_Primitives = BBnewArr(
		a_SystemAllocator,
		t_PrimitiveCount,
		Model::Primitive);
	a_Model.primitives = t_Primitives;
	a_Model.primitiveCount = t_PrimitiveCount;

	Model::Node* t_LinearNodes = BBnewArr(
		a_SystemAllocator,
		t_LinearNodeCount,
		Model::Node);
	a_Model.linearNodes = t_LinearNodes;
	a_Model.linearNodeCount = t_LinearNodeCount;

	//Temporary stuff
	uint32_t* t_Indices = BBnewArr(
		a_TempAllocator,
		t_IndexCount,
		uint32_t);
	Vertex* t_Vertices = BBnewArr(
		a_TempAllocator,
		t_VertexCount,
		Vertex);

	uint32_t t_CurrentIndex = 0;

	uint32_t t_CurrentNode = 0;
	uint32_t t_CurrentMesh = 0;
	uint32_t t_CurrentPrimitive = 0;

	for (size_t nodeIndex = 0; nodeIndex < t_LinearNodeCount; nodeIndex++)
	{
		//TODO: we do not handle childeren now, we should!
		const cgltf_node& t_Node = t_Data->nodes[nodeIndex];
		Model::Node& t_ModelNode = t_LinearNodes[t_CurrentNode++];
		t_ModelNode.childCount = 0;
		t_ModelNode.childeren = nullptr; //For now we don't care.
		t_ModelNode.meshIndex = MESH_INVALID_INDEX;
		if (t_Node.mesh != nullptr)
		{
			const cgltf_mesh& t_Mesh = *t_Node.mesh;

			Model::Mesh& t_ModelMesh = t_Meshes[t_CurrentMesh];
			t_ModelNode.meshIndex = t_CurrentMesh++;
			t_ModelMesh.primitiveOffset = t_CurrentPrimitive;
			t_ModelMesh.primitiveCount = static_cast<uint32_t>(t_Mesh.primitives_count);

			for (size_t primitiveIndex = 0; primitiveIndex < t_Mesh.primitives_count; primitiveIndex++)
			{
				const cgltf_primitive& t_Primitive = t_Mesh.primitives[primitiveIndex];
				Model::Primitive& t_MeshPrimitive = t_Primitives[t_CurrentPrimitive++];
				t_MeshPrimitive.indexCount = static_cast<uint32_t>(t_Primitive.indices->count);
				t_MeshPrimitive.indexStart = t_CurrentIndex;

				void* t_IndexData = GetAccessorDataPtr(t_Primitive.indices);
				if (t_Primitive.indices->component_type == cgltf_component_type_r_32u)
				{
					for (size_t i = 0; i < t_Primitive.indices->count; i++)
						t_Indices[t_CurrentIndex++] = reinterpret_cast<uint32_t*>(t_IndexData)[i];
				}
				else if (t_Primitive.indices->component_type == cgltf_component_type_r_16u)
				{
					for (size_t i = 0; i < t_Primitive.indices->count; i++)
						t_Indices[t_CurrentIndex++] = reinterpret_cast<uint16_t*>(t_IndexData)[i];
				}
				else
				{
					BB_ASSERT(false, "GLTF mesh has an index type that is not supported!");
				}

				for (size_t i = 0; i < t_VertexCount; i++)
				{
					t_Vertices[i].color.x = 1.0f;
					t_Vertices[i].color.y = 1.0f;
					t_Vertices[i].color.z = 1.0f;
				}

				for (size_t attrIndex = 0; attrIndex < t_Primitive.attributes_count; attrIndex++)
				{
					const cgltf_attribute& t_Attribute = t_Primitive.attributes[attrIndex];
					float* t_PosData = nullptr;
					size_t t_CurrentVertex = 0;

					switch (t_Attribute.type)
					{
					case cgltf_attribute_type_position:
						t_PosData = reinterpret_cast<float*>(GetAccessorDataPtr(t_Attribute.data));

						for (size_t posIndex = 0; posIndex < t_Attribute.data->count; posIndex++)
						{
							t_Vertices[t_CurrentVertex].pos.x = t_PosData[0];
							t_Vertices[t_CurrentVertex].pos.y = t_PosData[1];
							t_Vertices[t_CurrentVertex].pos.z = t_PosData[2];

							t_PosData = reinterpret_cast<float*>(Pointer::Add(t_PosData, t_Attribute.data->stride));
							++t_CurrentVertex;
						}
						break;
					case cgltf_attribute_type_texcoord:
						t_PosData = reinterpret_cast<float*>(GetAccessorDataPtr(t_Attribute.data));

						for (size_t posIndex = 0; posIndex < t_Attribute.data->count; posIndex++)
						{
							t_Vertices[t_CurrentVertex].uv.x = t_PosData[0];
							t_Vertices[t_CurrentVertex].uv.y = t_PosData[1];

							t_PosData = reinterpret_cast<float*>(Pointer::Add(t_PosData, t_Attribute.data->stride));
							++t_CurrentVertex;
						}

						break;
					case cgltf_attribute_type_normal:
						t_PosData = reinterpret_cast<float*>(GetAccessorDataPtr(t_Attribute.data));

						for (size_t posIndex = 0; posIndex < t_Attribute.data->count; posIndex++)
						{
							t_Vertices[t_CurrentVertex].normal.x = t_PosData[0];
							t_Vertices[t_CurrentVertex].normal.y = t_PosData[1];
							t_Vertices[t_CurrentVertex].normal.z = t_PosData[2];

							t_PosData = reinterpret_cast<float*>(Pointer::Add(t_PosData, t_Attribute.data->stride));
							++t_CurrentVertex;
						}
						break;
					}
					BB_ASSERT(t_VertexCount >= t_CurrentVertex, "Overwriting vertices in the gltf loader!");
				}
			}
		}
	}

	//get it all in GPU buffers now.
	{
		const size_t t_VertexBufferSize = t_VertexCount * sizeof(Vertex);

		const UploadBufferChunk t_VertChunk = a_UploadBuffer.Alloc(t_VertexBufferSize);
		memcpy(t_VertChunk.memory, t_Vertices, t_VertexBufferSize);

		a_Model.vertexView = AllocateFromVertexBuffer(t_VertexBufferSize);

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = a_UploadBuffer.Buffer();
		t_CopyInfo.dst = a_Model.vertexView.buffer;
		t_CopyInfo.srcOffset = t_VertChunk.bufferOffset;
		t_CopyInfo.dstOffset = a_Model.vertexView.offset;
		t_CopyInfo.size = a_Model.vertexView.size;

		RenderBackend::CopyBuffer(a_TransferCmdList, t_CopyInfo);
	}

	{
		const size_t t_IndexBufferSize = t_IndexCount * sizeof(uint32_t);

		const UploadBufferChunk t_IndexChunk = a_UploadBuffer.Alloc(t_IndexBufferSize);
		memcpy(t_IndexChunk.memory, t_Indices, t_IndexBufferSize);

		a_Model.indexView = AllocateFromIndexBuffer(t_IndexBufferSize);

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = a_UploadBuffer.Buffer();
		t_CopyInfo.dst = a_Model.indexView.buffer;
		t_CopyInfo.srcOffset = t_IndexChunk.bufferOffset;
		t_CopyInfo.dstOffset = a_Model.indexView.offset;
		t_CopyInfo.size = a_Model.indexView.size;

		RenderBackend::CopyBuffer(a_TransferCmdList, t_CopyInfo);
	}

	{ //descriptor allocation
		a_Model.meshDescriptor = t_MeshDescriptor;
		a_Model.descAllocation = Render::AllocateDescriptor(a_Model.meshDescriptor);

		WriteDescriptorData t_WriteData{};
		t_WriteData.binding = 0;
		t_WriteData.descriptorIndex = 0;
		t_WriteData.type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
		t_WriteData.buffer.buffer = a_Model.vertexView.buffer;
		t_WriteData.buffer.offset = a_Model.vertexView.offset;
		t_WriteData.buffer.range = a_Model.vertexView.size;
		WriteDescriptorInfos t_WriteInfos{};
		t_WriteInfos.allocation = a_Model.descAllocation;
		t_WriteInfos.descriptorHandle = t_MeshDescriptor;
		t_WriteInfos.data = Slice(&t_WriteData, 1);
		RenderBackend::WriteDescriptors(t_WriteInfos);
	}

	cgltf_free(t_Data);
}
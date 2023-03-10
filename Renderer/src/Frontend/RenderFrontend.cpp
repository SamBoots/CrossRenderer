#include "RenderFrontend.h"
#include "ShaderCompiler.h"

#include "Transform.h"

#include "Storage/Slotmap.h"
#include "OS/Program.h"
#include "ModelLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"


using namespace BB;
using namespace BB::Render;

static FreelistAllocator_t m_SystemAllocator{ mbSize * 4 };
static TemporaryAllocator m_TempAllocator{ m_SystemAllocator };

struct RendererInst
{
	uint32_t swapchainWidth = 0;
	uint32_t swapchainHeight = 0;

	uint32_t frameBufferAmount;
	uint32_t modelMatrixMax = 10;

	RENDER_API renderAPI = RENDER_API::NONE;

	Slotmap<Model> models{ m_SystemAllocator };
	Slotmap<DrawObject> drawObjects{ m_SystemAllocator };
};

struct PerFrameInfo
{
	uint64_t transferBufferSize;

	RBufferHandle perFrameBuffer;
	RBufferHandle perFrameTransferBuffer;
	void* transferBufferPtr;
};

CommandQueueHandle t_GraphicsQueue;
CommandQueueHandle t_TransferQueue;

CommandAllocatorHandle t_CommandAllocators[3];
CommandAllocatorHandle t_TransferAllocator[3];

CommandListHandle t_GraphicCommands[3];
CommandListHandle t_TransferCommands[3];

RecordingCommandListHandle t_RecordingGraphics;
RecordingCommandListHandle t_RecordingTransfer;

RFenceHandle t_SwapchainFence[3];

RDescriptorHandle t_Descriptor1;
RDescriptorHandle t_Descriptor2;
PipelineHandle t_Pipeline;

UploadBuffer* t_UploadBuffer;

RImageHandle t_ExampleImage;

static FrameIndex s_CurrentFrame;

static RendererInst s_RendererInst;
static PerFrameInfo s_PerFrameInfo;

static void Draw3DFrame()
{
	//Copy the perframe buffer over.
	RenderCopyBufferInfo t_CopyInfo;
	t_CopyInfo.src = s_PerFrameInfo.perFrameTransferBuffer;
	t_CopyInfo.dst = s_PerFrameInfo.perFrameBuffer;
	t_CopyInfo.size = sizeof(CameraBufferInfo) + (sizeof(ModelBufferInfo) * s_RendererInst.modelMatrixMax);
	t_CopyInfo.srcOffset = 0;
	t_CopyInfo.dstOffset = t_CopyInfo.size * s_CurrentFrame;

	RenderBackend::CopyBuffer(t_RecordingTransfer, t_CopyInfo);

	StartRenderingInfo t_StartRenderInfo;
	t_StartRenderInfo.viewportWidth = s_RendererInst.swapchainWidth;
	t_StartRenderInfo.viewportHeight = s_RendererInst.swapchainHeight;
	t_StartRenderInfo.colorLoadOp = RENDER_LOAD_OP::CLEAR;
	t_StartRenderInfo.colorStoreOp = RENDER_STORE_OP::STORE;
	t_StartRenderInfo.colorInitialLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
	t_StartRenderInfo.colorFinalLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	t_StartRenderInfo.clearColor[0] = 1.0f;
	t_StartRenderInfo.clearColor[1] = 0.0f;
	t_StartRenderInfo.clearColor[2] = 0.0f;
	t_StartRenderInfo.clearColor[3] = 1.0f;

	//Record rendering commands.
	RenderBackend::StartRendering(t_RecordingGraphics, t_StartRenderInfo);
	
	RModelHandle t_CurrentModel = s_RendererInst.drawObjects.begin()->modelHandle;
	Model* t_Model = &s_RendererInst.models.find(t_CurrentModel.handle);
	uint32_t t_CamOffset = (sizeof(CameraBufferInfo) + sizeof(ModelBufferInfo) * s_RendererInst.modelMatrixMax) * s_CurrentFrame;
	uint32_t t_MatrixOffset = t_CamOffset + sizeof(CameraBufferInfo);
	uint32_t t_DynOffSets[2]{ t_CamOffset, t_MatrixOffset };
	RenderBackend::BindPipeline(t_RecordingGraphics, t_Model->pipelineHandle);
	RDescriptorHandle t_Descriptors[]{ t_Descriptor1 , t_Descriptor2 };
	RenderBackend::BindDescriptors(t_RecordingGraphics, t_Descriptors, _countof(t_Descriptors), 2, t_DynOffSets);

	uint64_t t_BufferOffsets[1]{ 0 };
	RenderBackend::BindVertexBuffers(t_RecordingGraphics, &t_Model->vertexBuffer, t_BufferOffsets, 1);
	RenderBackend::BindIndexBuffer(t_RecordingGraphics, t_Model->indexBuffer, 0);

	for (auto t_It = s_RendererInst.drawObjects.begin(); t_It < s_RendererInst.drawObjects.end(); t_It++)
	{
		if (t_CurrentModel != t_It->modelHandle)
		{
			t_CurrentModel = t_It->modelHandle;
			Model* t_NewModel = &s_RendererInst.models.find(t_CurrentModel.handle);

			if (t_NewModel->pipelineHandle != t_Model->pipelineHandle)
			{
				RenderBackend::BindPipeline(t_RecordingGraphics, t_NewModel->pipelineHandle);
				RenderBackend::BindDescriptors(t_RecordingGraphics, t_Descriptors, _countof(t_Descriptors), 2, t_DynOffSets);
			}

			RenderBackend::BindVertexBuffers(t_RecordingGraphics, &t_NewModel->vertexBuffer, t_BufferOffsets, 1);
			RenderBackend::BindIndexBuffer(t_RecordingGraphics, t_NewModel->indexBuffer, 0);

			t_Model = t_NewModel;
		}

		RenderBackend::BindConstant(t_RecordingGraphics, 0, 1, 0, &t_It->transformHandle.index);
		for (uint32_t i = 0; i < t_Model->linearNodeCount; i++)
		{
			const Model::Node& t_Node = t_Model->linearNodes[i];
			if (t_Node.meshIndex != MESH_INVALID_INDEX) 
			{
				const Model::Mesh& t_Mesh = t_Model->meshes[t_Node.meshIndex];
				for (size_t t_PrimIndex = 0; t_PrimIndex < t_Mesh.primitiveCount; t_PrimIndex++)
				{
					const Model::Primitive& t_Prim = t_Model->primitives[t_Mesh.primitiveOffset + t_PrimIndex];
					RenderBackend::DrawIndexed(t_RecordingGraphics,
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

	RenderBackend::EndRendering(t_RecordingGraphics, t_EndRenderingInfo);
	RenderBackend::EndCommandList(t_RecordingGraphics);
}

void BB::Render::InitRenderer(const RenderInitInfo& a_InitInfo)
{
	Shader::InitShaderCompiler();

	BB::Array<RENDER_EXTENSIONS> t_Extensions{ m_TempAllocator };
	t_Extensions.emplace_back(RENDER_EXTENSIONS::STANDARD_VULKAN_INSTANCE);
	//t_Extensions.emplace_back(RENDER_EXTENSIONS::PHYSICAL_DEVICE_EXTRA_PROPERTIES); Now all these are included in STANDARD_VULKAN_INSTANCE
	if (a_InitInfo.debug)
	{
		t_Extensions.emplace_back(RENDER_EXTENSIONS::DEBUG);
	}
	BB::Array<RENDER_EXTENSIONS> t_DeviceExtensions{ m_TempAllocator };
	t_DeviceExtensions.emplace_back(RENDER_EXTENSIONS::STANDARD_VULKAN_DEVICE);
	t_DeviceExtensions.emplace_back(RENDER_EXTENSIONS::PIPELINE_EXTENDED_DYNAMIC_STATE);

	int t_WindowWidth;
	int t_WindowHeight;
	GetWindowSize(a_InitInfo.windowHandle, t_WindowWidth, t_WindowHeight);
	s_RendererInst.swapchainWidth = static_cast<uint32_t>(t_WindowWidth);
	s_RendererInst.swapchainHeight = static_cast<uint32_t>(t_WindowHeight);


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

	RenderBackend::InitBackend(t_BackendCreateInfo);
	s_RendererInst.frameBufferAmount = RenderBackend::GetFrameBufferAmount();
	s_RendererInst.renderAPI = a_InitInfo.renderAPI;


#pragma region PipelineCreation
	const uint64_t t_PerFrameBufferSingleFrame = sizeof(CameraBufferInfo) + sizeof(ModelBufferInfo) * s_RendererInst.modelMatrixMax;

	RenderBufferCreateInfo t_PerFrameTransferBuffer;
	t_PerFrameTransferBuffer.size = t_PerFrameBufferSingleFrame;
	t_PerFrameTransferBuffer.usage = RENDER_BUFFER_USAGE::STAGING;
	t_PerFrameTransferBuffer.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
	t_PerFrameTransferBuffer.data = nullptr;
	s_PerFrameInfo.perFrameTransferBuffer = RenderBackend::CreateBuffer(t_PerFrameTransferBuffer);
	s_PerFrameInfo.transferBufferPtr = RenderBackend::MapMemory(s_PerFrameInfo.perFrameTransferBuffer);

	const uint64_t t_perFrameBufferEntireSize = t_PerFrameBufferSingleFrame * s_RendererInst.frameBufferAmount;

	RenderBufferCreateInfo t_PerFrameBuffer;
	t_PerFrameBuffer.size = t_perFrameBufferEntireSize;
	t_PerFrameBuffer.usage = RENDER_BUFFER_USAGE::STORAGE;
	t_PerFrameBuffer.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
	t_PerFrameBuffer.data = nullptr;
	s_PerFrameInfo.perFrameBuffer = RenderBackend::CreateBuffer(t_PerFrameBuffer);

	int x, y, c;
	stbi_uc* t_Pixels = stbi_load("../Resources/Textures/Test.jpg", &x, &y, &c, 4);
	STBI_FREE(t_Pixels); //HACK, will fix later.
	{
		RenderImageCreateInfo t_ImageInfo{};
		t_ImageInfo.width = static_cast<uint32_t>(x);
		t_ImageInfo.height = static_cast<uint32_t>(y);
		t_ImageInfo.depth = 1;
		t_ImageInfo.arrayLayers = 1;
		t_ImageInfo.mipLevels = 1;
		t_ImageInfo.tiling = RENDER_IMAGE_TILING::OPTIMAL;
		t_ImageInfo.type = RENDER_IMAGE_TYPE::TYPE_2D;
		t_ImageInfo.usage = RENDER_IMAGE_USAGE::SAMPLER;
		t_ImageInfo.format = RENDER_IMAGE_FORMAT::SRGB;

		t_ExampleImage = RenderBackend::CreateImage(t_ImageInfo);
	}


	PipelineInitInfo t_PipeInitInfo{};

	PipelineBuilder t_BasicPipe{ t_PipeInitInfo };
	
	{
		RenderDescriptorCreateInfo t_CreateInfo{};
		FixedArray<DescriptorBinding, 2> t_DescBinds;
		t_CreateInfo.bindingSet = RENDER_BINDING_SET::PER_FRAME;
		t_CreateInfo.bindings = BB::Slice(t_DescBinds.data(), t_DescBinds.size());

		{//CamBind
			t_DescBinds[0].binding = 0;
			t_DescBinds[0].descriptorCount = 1;
			t_DescBinds[0].stage = RENDER_SHADER_STAGE::VERTEX;
			t_DescBinds[0].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER_DYNAMIC;
			t_DescBinds[0].flags = RENDER_DESCRIPTOR_FLAG::NONE;
		}
		{//ModelBind
			t_DescBinds[1].binding = 1;
			t_DescBinds[1].descriptorCount = 1;
			t_DescBinds[1].stage = RENDER_SHADER_STAGE::VERTEX;
			t_DescBinds[1].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER_DYNAMIC;
			t_DescBinds[1].flags = RENDER_DESCRIPTOR_FLAG::NONE;
		}

		t_Descriptor1 = RenderBackend::CreateDescriptor(t_CreateInfo);
	}

	{
		DescriptorBinding t_DescBind{};
		RenderDescriptorCreateInfo t_CreateInfo{};
		t_CreateInfo.bindingSet = RENDER_BINDING_SET::PER_PASS;
		t_CreateInfo.bindings = BB::Slice(&t_DescBind, 1);
		{//Image Binds
			t_DescBind.binding = 0;
			t_DescBind.descriptorCount = DESCRIPTOR_IMAGE_MAX;
			t_DescBind.stage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
			t_DescBind.type = RENDER_DESCRIPTOR_TYPE::COMBINED_IMAGE_SAMPLER;
			t_DescBind.flags = RENDER_DESCRIPTOR_FLAG::BINDLESS;
		}

		t_Descriptor2 = RenderBackend::CreateDescriptor(t_CreateInfo);
	}

	{
		UpdateDescriptorBufferInfo t_BufferUpdate{};
		t_BufferUpdate.binding = 0;
		t_BufferUpdate.descriptorIndex = 0;
		t_BufferUpdate.set = t_Descriptor1;
		t_BufferUpdate.type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER_DYNAMIC;

		t_BufferUpdate.buffer = s_PerFrameInfo.perFrameBuffer;
		t_BufferUpdate.bufferOffset = 0;
		t_BufferUpdate.bufferSize = sizeof(CameraBufferInfo);

		RenderBackend::UpdateDescriptorBuffer(t_BufferUpdate);

		t_BufferUpdate.binding = 1;
		t_BufferUpdate.descriptorIndex = 0;
		t_BufferUpdate.bufferOffset = 0;
		t_BufferUpdate.bufferSize = sizeof(ModelBufferInfo) * s_RendererInst.modelMatrixMax;

		RenderBackend::UpdateDescriptorBuffer(t_BufferUpdate);
	}

	{
		UpdateDescriptorImageInfo t_ImageUpdate{};
		t_ImageUpdate.binding = 0;
		t_ImageUpdate.descriptorIndex = 0;
		t_ImageUpdate.set = t_Descriptor2;
		t_ImageUpdate.type = RENDER_DESCRIPTOR_TYPE::COMBINED_IMAGE_SAMPLER;

		t_ImageUpdate.imageLayout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
		t_ImageUpdate.image = t_ExampleImage;

		RenderBackend::UpdateDescriptorImage(t_ImageUpdate);
	}

	t_BasicPipe.BindDescriptor(t_Descriptor1);
	t_BasicPipe.BindDescriptor(t_Descriptor2);

	const wchar_t* t_ShaderPath[2];
	t_ShaderPath[0] = L"../Resources/Shaders/HLSLShaders/DebugVert.hlsl";
	t_ShaderPath[1] = L"../Resources/Shaders/HLSLShaders/DebugFrag.hlsl";

	Shader::ShaderCodeHandle t_ShaderHandles[2];
	t_ShaderHandles[0] = Shader::CompileShader(
		t_ShaderPath[0],
		L"main",
		RENDER_SHADER_STAGE::VERTEX,
		s_RendererInst.renderAPI);
	t_ShaderHandles[1] = Shader::CompileShader(
		t_ShaderPath[1],
		L"main",
		RENDER_SHADER_STAGE::FRAGMENT_PIXEL,
		s_RendererInst.renderAPI);

	Buffer t_ShaderBuffer;
	Shader::GetShaderCodeBuffer(t_ShaderHandles[0], t_ShaderBuffer);
	ShaderCreateInfo t_ShaderBuffers[2];
	t_ShaderBuffers[0].buffer = t_ShaderBuffer;
	t_ShaderBuffers[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;

	Shader::GetShaderCodeBuffer(t_ShaderHandles[1], t_ShaderBuffer);
	t_ShaderBuffers[1].buffer = t_ShaderBuffer;
	t_ShaderBuffers[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;

	t_BasicPipe.BindShaders(BB::Slice(t_ShaderBuffers, 2));

	t_Pipeline = t_BasicPipe.BuildPipeline();

#pragma endregion //PipelineCreation


	RenderCommandQueueCreateInfo t_QueueCreateInfo;
	t_QueueCreateInfo.queue = RENDER_QUEUE_TYPE::GRAPHICS;
	t_QueueCreateInfo.flags = RENDER_FENCE_FLAGS::CREATE_SIGNALED;
	t_GraphicsQueue = RenderBackend::CreateCommandQueue(t_QueueCreateInfo);
	t_QueueCreateInfo.queue = RENDER_QUEUE_TYPE::TRANSFER_COPY;
	t_TransferQueue = RenderBackend::CreateCommandQueue(t_QueueCreateInfo);

	RenderCommandAllocatorCreateInfo t_AllocatorCreateInfo;
	t_AllocatorCreateInfo.commandListCount = 10;
	t_AllocatorCreateInfo.queueType = RENDER_QUEUE_TYPE::GRAPHICS;
	t_CommandAllocators[0] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
	t_CommandAllocators[1] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
	t_CommandAllocators[2] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);

	t_AllocatorCreateInfo.queueType = RENDER_QUEUE_TYPE::TRANSFER_COPY;
	t_TransferAllocator[0] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
	t_TransferAllocator[1] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
	t_TransferAllocator[2] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);

	RenderCommandListCreateInfo t_CmdCreateInfo;
	t_CmdCreateInfo.commandAllocator = t_CommandAllocators[0];
	t_GraphicCommands[0] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	t_CmdCreateInfo.commandAllocator = t_CommandAllocators[1];
	t_GraphicCommands[1] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	t_CmdCreateInfo.commandAllocator = t_CommandAllocators[2];
	t_GraphicCommands[2] = RenderBackend::CreateCommandList(t_CmdCreateInfo);

	//just reuse the struct above.
	t_CmdCreateInfo.commandAllocator = t_TransferAllocator[0];
	t_TransferCommands[0] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	t_CmdCreateInfo.commandAllocator = t_TransferAllocator[1];
	t_TransferCommands[1] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	t_CmdCreateInfo.commandAllocator = t_TransferAllocator[2];
	t_TransferCommands[2] = RenderBackend::CreateCommandList(t_CmdCreateInfo);

	FenceCreateInfo t_CreateInfo;
	t_CreateInfo.flags = RENDER_FENCE_FLAGS::CREATE_SIGNALED;

	for (size_t i = 0; i < _countof(t_SwapchainFence); i++)
		t_SwapchainFence[i] = RenderBackend::CreateFence(t_CreateInfo);

	//Create upload buffer.
	constexpr const uint64_t UPLOAD_BUFFER_SIZE = mbSize * 32;
	t_UploadBuffer = BBnew(m_SystemAllocator, UploadBuffer)(UPLOAD_BUFFER_SIZE);

	for (size_t i = 0; i < _countof(t_ShaderHandles); i++)
	{
		Shader::ReleaseShaderCode(t_ShaderHandles[i]);
	}
}

void BB::Render::DestroyRenderer()
{
	RenderBackend::WaitGPUReady();

	for (auto it = s_RendererInst.models.begin(); it < s_RendererInst.models.end(); it++)
	{
		RenderBackend::DestroyBuffer(it->indexBuffer);
		RenderBackend::DestroyBuffer(it->vertexBuffer);
	}
	BBfree(m_SystemAllocator, t_UploadBuffer);

	RenderBackend::DestroyDescriptor(t_Descriptor1);
	RenderBackend::DestroyDescriptor(t_Descriptor2);
	RenderBackend::DestroyBuffer(s_PerFrameInfo.perFrameBuffer);
	RenderBackend::UnmapMemory(s_PerFrameInfo.perFrameTransferBuffer);
	RenderBackend::DestroyBuffer(s_PerFrameInfo.perFrameTransferBuffer);

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

void BB::Render::Update(const float a_DeltaTime)
{
	Draw3DFrame();
}

void BB::Render::SetProjection(const glm::mat4& a_Proj)
{
	memcpy(Pointer::Add(s_PerFrameInfo.transferBufferPtr, sizeof(glm::mat4)), &a_Proj, sizeof(glm::mat4));
}

void BB::Render::SetView(const glm::mat4& a_View)
{
	memcpy(s_PerFrameInfo.transferBufferPtr, &a_View, sizeof(glm::mat4));
}

void* BB::Render::GetMatrixBufferSpace(uint32_t& a_MatrixSpace)
{
	a_MatrixSpace = s_RendererInst.modelMatrixMax;
	return Pointer::Add(s_PerFrameInfo.transferBufferPtr, sizeof(CameraBufferInfo));
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


		int x, y, c;
		//hacky way, whatever we do it for now.
		stbi_uc* t_Pixels = stbi_load(a_CreateInfo.imagePath, &x, &y, &c, 4);
		ImageReturnInfo t_ImageInfo = RenderBackend::GetImageInfo(t_ExampleImage);

		UploadBufferChunk t_StageBuffer = t_UploadBuffer->Alloc(t_ImageInfo.allocInfo.imageAllocByteSize);
		const UINT64 t_SourcePitch = t_ImageInfo.width * sizeof(uint32_t);

		void* t_ImageSrc = t_Pixels;
		void* t_ImageDst = t_StageBuffer.memory;
		//Layouts should be only 1 right now due to mips.
		for (uint32_t i = 0; i < t_ImageInfo.allocInfo.footHeight; i++)
		{
			memcpy(t_ImageDst, t_ImageSrc, t_SourcePitch);

			t_ImageSrc = Pointer::Add(t_ImageSrc, t_SourcePitch);
			t_ImageDst = Pointer::Add(t_ImageDst, t_ImageInfo.allocInfo.footRowPitch);
		}

		RenderCopyBufferImageInfo t_CopyImage{};
		t_CopyImage.srcBuffer = t_UploadBuffer->Buffer();
		t_CopyImage.srcBufferOffset = t_StageBuffer.offset;
		t_CopyImage.dstImage = t_ExampleImage;
		t_CopyImage.dstImageInfo.sizeX = static_cast<uint32_t>(t_ImageInfo.width);
		t_CopyImage.dstImageInfo.sizeY = static_cast<uint32_t>(t_ImageInfo.height);
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

		RenderBufferCreateInfo t_VertexInfo;
		t_VertexInfo.usage = RENDER_BUFFER_USAGE::VERTEX;
		t_VertexInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
		t_VertexInfo.size = a_CreateInfo.vertices.sizeInBytes();
		t_VertexInfo.data = nullptr;

		t_Model.vertexBuffer = RenderBackend::CreateBuffer(t_VertexInfo);

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = t_UploadBuffer->Buffer();
		t_CopyInfo.dst = t_Model.vertexBuffer;
		t_CopyInfo.srcOffset = t_StageBuffer.offset;
		t_CopyInfo.dstOffset = 0;
		t_CopyInfo.size = a_CreateInfo.vertices.sizeInBytes();

		RenderBackend::CopyBuffer(t_RecordingTransfer, t_CopyInfo);
	}

	{
		UploadBufferChunk t_StageBuffer = t_UploadBuffer->Alloc(a_CreateInfo.indices.sizeInBytes());
		memcpy(t_StageBuffer.memory, a_CreateInfo.indices.data(), a_CreateInfo.indices.sizeInBytes());

		RenderBufferCreateInfo t_IndexInfo;
		t_IndexInfo.usage = RENDER_BUFFER_USAGE::INDEX;
		t_IndexInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
		t_IndexInfo.size = a_CreateInfo.indices.sizeInBytes();
		t_IndexInfo.data = nullptr;

		t_Model.indexBuffer = RenderBackend::CreateBuffer(t_IndexInfo);

		RenderCopyBufferInfo t_CopyInfo;
		t_CopyInfo.src = t_UploadBuffer->Buffer();
		t_CopyInfo.dst = t_Model.indexBuffer;
		t_CopyInfo.srcOffset = t_StageBuffer.offset;
		t_CopyInfo.dstOffset = 0;
		t_CopyInfo.size = a_CreateInfo.indices.sizeInBytes();

		RenderBackend::CopyBuffer(t_RecordingTransfer, t_CopyInfo);
	}
	
	t_Model.linearNodes = BBnewArr(m_SystemAllocator, 1, Model::Node);
	t_Model.linearNodeCount = 1;
	t_Model.nodes = t_Model.linearNodes;
	t_Model.nodeCount = 1;

	t_Model.primitives = BBnewArr(m_SystemAllocator, 1, Model::Primitive);
	t_Model.primitiveCount = 1;
	t_Model.primitives->indexStart = 0;
	t_Model.primitives->indexCount = static_cast<uint32_t>(a_CreateInfo.indices.size());

	t_Model.meshCount = 1;
	t_Model.meshes = BBnewArr(m_SystemAllocator, 1, Model::Mesh);
	t_Model.meshes->primitiveCount = 1;
	t_Model.meshes->primitiveOffset = 0;

	return RModelHandle(s_RendererInst.models.insert(t_Model).handle);
}

RModelHandle BB::Render::LoadModel(const LoadModelInfo& a_LoadInfo)
{
	Model t_Model;

	t_Model.pipelineHandle = t_Pipeline;

	switch (a_LoadInfo.modelType)
	{
	case BB::MODEL_TYPE::GLTF:
		LoadglTFModel(m_TempAllocator,
			m_SystemAllocator,
			t_Model,
			*t_UploadBuffer,
			t_RecordingTransfer,
			a_LoadInfo.path);
		break;
	}
	
	return RModelHandle(s_RendererInst.models.insert(t_Model).handle);
}

DrawObjectHandle BB::Render::CreateDrawObject(const RModelHandle a_Model, const TransformHandle a_TransformHandle)
{
	DrawObject t_DrawObject{ a_Model, a_TransformHandle };
	return DrawObjectHandle(s_RendererInst.drawObjects.emplace(t_DrawObject).handle);
}

void BB::Render::DestroyDrawObject(const DrawObjectHandle a_Handle)
{
	s_RendererInst.drawObjects.erase(a_Handle.handle);
}

void BB::Render::StartFrame()
{
	StartFrameInfo t_StartInfo{};
	t_StartInfo.fences = &t_SwapchainFence[s_CurrentFrame];
	t_StartInfo.fenceCount = 1;
	RenderBackend::StartFrame(t_StartInfo);
	//Prepare the commandallocator for a new frame
	//TODO, send a fence that waits until the image was presented.
	RenderBackend::ResetCommandAllocator(t_CommandAllocators[s_CurrentFrame]);
	RenderBackend::ResetCommandAllocator(t_TransferAllocator[s_CurrentFrame]);

	t_RecordingGraphics = RenderBackend::StartCommandList(t_GraphicCommands[s_CurrentFrame]);
	t_RecordingTransfer = RenderBackend::StartCommandList(t_TransferCommands[s_CurrentFrame]);

}

bool firstTimeTransfer = true;

void BB::Render::EndFrame()
{
	RenderBackend::EndCommandList(t_RecordingTransfer);
	
	ExecuteCommandsInfo* t_ExecuteInfos = BBnewArr(
		m_TempAllocator,
		2,
		ExecuteCommandsInfo);

	t_ExecuteInfos[0] = {};
	t_ExecuteInfos[0].commands = &t_TransferCommands[s_CurrentFrame];
	t_ExecuteInfos[0].commandCount = 1;
	t_ExecuteInfos[0].signalQueues = &t_TransferQueue;
	t_ExecuteInfos[0].signalQueueCount = 1;

	RenderBackend::ExecuteCommands(t_TransferQueue, &t_ExecuteInfos[0], 1);

	uint64_t t_WaitValue = RenderBackend::NextQueueFenceValue(t_TransferQueue) - 1;

	//We write to vertex information (vertex buffer, index buffer and the storage buffer storing all the matrices.)
	RENDER_PIPELINE_STAGE t_WaitStage = RENDER_PIPELINE_STAGE::VERTEX_SHADER;
	t_ExecuteInfos[1] = {};
	t_ExecuteInfos[1].commands = &t_GraphicCommands[s_CurrentFrame];
	t_ExecuteInfos[1].commandCount = 1;
	t_ExecuteInfos[1].waitQueueCount = 1;
	t_ExecuteInfos[1].waitQueues = &t_TransferQueue;
	t_ExecuteInfos[1].waitValues = &t_WaitValue;
	t_ExecuteInfos[1].waitStages = &t_WaitStage;

	RenderBackend::ExecutePresentCommands(t_GraphicsQueue, t_ExecuteInfos[1]);
	PresentFrameInfo t_PresentFrame{};
	//t_PresentFrame = 1;
	//t_PresentFrame.waitSemaphores = &t_RenderSemaphores[s_CurrentFrame];
	s_CurrentFrame = RenderBackend::PresentFrame(t_PresentFrame);
}

void BB::Render::ResizeWindow(const uint32_t a_X, const uint32_t a_Y)
{
	s_RendererInst.swapchainWidth = a_X;
	s_RendererInst.swapchainHeight = a_Y;
	RenderBackend::ResizeWindow(a_X, a_Y);
}
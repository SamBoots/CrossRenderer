#include "RenderFrontend.h"
#include "RenderBackend.h"
#include "ShaderCompiler.h"

#include "Transform.h"

#include "Storage/Slotmap.h"
#include "OS/Program.h"


using namespace BB;
using namespace BB::Render;

static FreelistAllocator_t m_SystemAllocator{ mbSize * 4 };
static TemporaryAllocator m_TempAllocator{ m_SystemAllocator };

struct RendererInst
{
	uint32_t frameBufferAmount;
	uint32_t modelMatrixMax = 10;

	Slotmap<Model> models{ m_SystemAllocator };
	Slotmap<DrawObject> drawObjects{ m_SystemAllocator };
};

struct PerFrameInfo
{
	uint64_t transferBufferSize;

	RBufferHandle perFrameBuffer;
	RBufferHandle perFrameTransferBuffer;
	void* transferBufferPtr;
	RDescriptorLayoutHandle perFrameDescriptorLayout;
	RDescriptorHandle perFrameDescriptor;
};

struct UploadBufferChunk
{
	void* memory;
	uint64_t offset;
};

class UploadBuffer
{
public:
	UploadBuffer(const uint64_t a_Size)
		:	size(a_Size)
	{
		RenderBufferCreateInfo t_UploadBufferInfo;
		t_UploadBufferInfo.size = size;
		t_UploadBufferInfo.usage = RENDER_BUFFER_USAGE::STAGING;
		t_UploadBufferInfo.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
		t_UploadBufferInfo.data = nullptr;
		buffer = RenderBackend::CreateBuffer(t_UploadBufferInfo);

		offset = 0;
		start = RenderBackend::MapMemory(buffer);
		position = start;
	}

	~UploadBuffer()
	{
		RenderBackend::UnmapMemory(buffer);
		RenderBackend::DestroyBuffer(buffer);
	}

	UploadBufferChunk Alloc(const uint64_t a_Size)
	{
		UploadBufferChunk t_Chunk;
		t_Chunk.memory = position;
		t_Chunk.offset = offset;
		position = Pointer::Add(position, a_Size);
		offset += a_Size;
		return t_Chunk;
	}

	void Clear()
	{
		offset = 0;
		position = start;
	}

	const RBufferHandle Buffer() const { return buffer; }

private:
	RBufferHandle buffer;
	const uint64_t size;
	uint64_t offset;
	void* start;
	void* position;
};

FrameBufferHandle t_FrameBuffer;

CommandQueueHandle t_GraphicsQueue;
CommandQueueHandle t_TransferQueue;

CommandAllocatorHandle t_CommandAllocators[3];
CommandAllocatorHandle t_TransferAllocator[3];

CommandListHandle t_GraphicCommands[3];
CommandListHandle t_TransferCommands[3];

RecordingCommandListHandle t_RecordingGraphics;
RecordingCommandListHandle t_RecordingTransfer;

RFenceHandle t_SwapchainFence[3];
PipelineHandle t_Pipeline;

UploadBuffer* t_UploadBuffer;

static FrameIndex s_CurrentFrame;

static RendererInst s_RendererInst;
static PerFrameInfo s_PerFrameInfo;

static void Draw3DFrame()
{
	//Copy the perframe buffer over.
	RenderCopyBufferInfo t_CopyInfo;
	t_CopyInfo.transferCommandHandle = t_RecordingTransfer;
	t_CopyInfo.src = s_PerFrameInfo.perFrameTransferBuffer;
	t_CopyInfo.dst = s_PerFrameInfo.perFrameBuffer;
	RenderCopyBufferInfo::CopyRegions t_CopyRegion;
	t_CopyRegion.size = sizeof(CameraBufferInfo) + (sizeof(ModelBufferInfo) * s_RendererInst.modelMatrixMax);
	t_CopyRegion.srcOffset = 0;
	t_CopyRegion.dstOffset = t_CopyRegion.size * s_CurrentFrame;

	t_CopyInfo.copyRegions = &t_CopyRegion;
	t_CopyInfo.CopyRegionCount = 1;

	RenderBackend::CopyBuffer(t_CopyInfo);

	//Record rendering commands.
	RenderBackend::StartRenderPass(t_RecordingGraphics, t_FrameBuffer);
	
	RModelHandle t_CurrentModel = s_RendererInst.drawObjects.begin()->modelHandle;
	Model& t_Model = s_RendererInst.models.find(t_CurrentModel.handle);
	RenderBackend::BindPipeline(t_RecordingGraphics, t_Model.pipelineHandle);

	uint32_t t_CamOffset = (sizeof(CameraBufferInfo) + sizeof(ModelBufferInfo) * s_RendererInst.modelMatrixMax) * s_CurrentFrame;
	uint32_t t_MatrixOffset = t_CamOffset + sizeof(CameraBufferInfo);
	uint32_t t_DynOffSets[2]{ t_CamOffset, t_MatrixOffset };
	RenderBackend::BindDescriptorSets(t_RecordingGraphics, 0, 1, &s_PerFrameInfo.perFrameDescriptor, 2, t_DynOffSets);


	uint64_t t_BufferOffsets[1]{ 0 };
	RenderBackend::BindVertexBuffers(t_RecordingGraphics, &t_Model.vertexBuffer, t_BufferOffsets, 1);
	RenderBackend::BindIndexBuffer(t_RecordingGraphics, t_Model.indexBuffer, 0);

	for (auto t_It = s_RendererInst.drawObjects.begin(); t_It < s_RendererInst.drawObjects.end(); t_It++)
	{
		if (t_CurrentModel != t_It->modelHandle)
		{
			t_CurrentModel = t_It->modelHandle;
			t_Model = s_RendererInst.models.find(t_CurrentModel.handle);
			RenderBackend::BindPipeline(t_RecordingGraphics, t_Model.pipelineHandle);
			RenderBackend::BindVertexBuffers(t_RecordingGraphics, &t_Model.vertexBuffer, t_BufferOffsets, 1);
			RenderBackend::BindIndexBuffer(t_RecordingGraphics, t_Model.indexBuffer, 0);
		}

		RenderBackend::BindConstant(t_RecordingGraphics, RENDER_SHADER_STAGE::VERTEX, 0, sizeof(uint32_t), &t_It->transformHandle.index);
		for (uint32_t i = 0; i < t_Model.linearNodeCount; i++)
		{
			for (size_t j = 0; j < t_Model.linearNodes[i].mesh->primitiveCount; j++)
			{
				RenderBackend::DrawIndexed(t_RecordingGraphics,
					t_Model.linearNodes[i].mesh->primitives[j].indexCount,
					1,
					t_Model.linearNodes[i].mesh->primitives[j].indexStart,
					0,
					0);
			}
		}
	}
	RenderBackend::EndRenderPass(t_RecordingGraphics);
	RenderBackend::EndCommandList(t_RecordingGraphics);
}

void BB::Render::InitRenderer(const WindowHandle a_WindowHandle, const LibHandle a_RenderLib, const bool a_Debug)
{
	Shader::InitShaderCompiler();

	BB::Array<RENDER_EXTENSIONS> t_Extensions{ m_TempAllocator };
	t_Extensions.emplace_back(RENDER_EXTENSIONS::STANDARD_VULKAN_INSTANCE);
	//t_Extensions.emplace_back(RENDER_EXTENSIONS::PHYSICAL_DEVICE_EXTRA_PROPERTIES); Now all these are included in STANDARD_VULKAN_INSTANCE
	if (a_Debug)
	{
		t_Extensions.emplace_back(RENDER_EXTENSIONS::DEBUG);
	}
	BB::Array<RENDER_EXTENSIONS> t_DeviceExtensions{ m_TempAllocator };
	t_DeviceExtensions.emplace_back(RENDER_EXTENSIONS::STANDARD_VULKAN_DEVICE);
	t_DeviceExtensions.emplace_back(RENDER_EXTENSIONS::PIPELINE_EXTENDED_DYNAMIC_STATE);

	int t_WindowWidth;
	int t_WindowHeight;
	Program::GetWindowSize(a_WindowHandle, t_WindowWidth, t_WindowHeight);

	RenderBackendCreateInfo t_BackendCreateInfo;
	t_BackendCreateInfo.getApiFuncPtr = (PFN_RenderGetAPIFunctions)Program::LibLoadFunc(a_RenderLib, "GetRenderAPIFunctions");
	t_BackendCreateInfo.extensions = t_Extensions;
	t_BackendCreateInfo.deviceExtensions = t_DeviceExtensions;
	t_BackendCreateInfo.hwnd = reinterpret_cast<HWND>(Program::GetOSWindowHandle(a_WindowHandle));
	t_BackendCreateInfo.version = 2;
	t_BackendCreateInfo.validationLayers = a_Debug;
	t_BackendCreateInfo.appName = "TestName";
	t_BackendCreateInfo.engineName = "TestEngine";
	t_BackendCreateInfo.windowWidth = static_cast<uint32_t>(t_WindowWidth);
	t_BackendCreateInfo.windowHeight = static_cast<uint32_t>(t_WindowHeight);

	RenderBackend::InitBackend(t_BackendCreateInfo);
	s_RendererInst.frameBufferAmount = RenderBackend::GetFrameBufferAmount();

	RenderFrameBufferCreateInfo t_FrameBufferCreateInfo;
	//VkRenderpass info
	t_FrameBufferCreateInfo.colorLoadOp = RENDER_LOAD_OP::CLEAR;
	t_FrameBufferCreateInfo.colorStoreOp = RENDER_STORE_OP::STORE;
	t_FrameBufferCreateInfo.colorInitialLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
	t_FrameBufferCreateInfo.colorFinalLayout = RENDER_IMAGE_LAYOUT::PRESENT;

	//VkFrameBuffer info
	t_FrameBufferCreateInfo.width = static_cast<uint32_t>(t_WindowWidth);
	t_FrameBufferCreateInfo.height = static_cast<uint32_t>(t_WindowHeight);

	t_FrameBuffer = RenderBackend::CreateFrameBuffer(t_FrameBufferCreateInfo);


#pragma region //Descriptor
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

	RenderDescriptorCreateInfo t_DescriptorCreateInfo{};
	FixedArray<RenderDescriptorCreateInfo::BufferBind, 2> t_BufferBinds;
	t_DescriptorCreateInfo.bufferBinds = BB::Slice(t_BufferBinds.data(), t_BufferBinds.size());
	FixedArray<RenderDescriptorCreateInfo::ImageBind, 0> t_ImageBinds;
	t_DescriptorCreateInfo.ImageBinds = BB::Slice(t_ImageBinds.data(), t_ImageBinds.size());
	FixedArray<RenderDescriptorCreateInfo::ConstantBind, 1> t_ConstantBinds;
	t_DescriptorCreateInfo.constantBinds = BB::Slice(t_ConstantBinds.data(), t_ConstantBinds.size());
	{//CamBind
		t_BufferBinds[0].binding = 0;
		t_BufferBinds[0].stage = RENDER_SHADER_STAGE::VERTEX;
		t_BufferBinds[0].type = DESCRIPTOR_BUFFER_TYPE::STORAGE_BUFFER_DYNAMIC;
		t_BufferBinds[0].buffer = s_PerFrameInfo.perFrameBuffer;
		t_BufferBinds[0].bufferOffset = 0;
		t_BufferBinds[0].bufferSize = sizeof(CameraBufferInfo);
	}
	{//ModelBind
		t_BufferBinds[1].binding = 1;
		t_BufferBinds[1].stage = RENDER_SHADER_STAGE::VERTEX;
		t_BufferBinds[1].type = DESCRIPTOR_BUFFER_TYPE::STORAGE_BUFFER_DYNAMIC;
		t_BufferBinds[1].buffer = s_PerFrameInfo.perFrameBuffer;
		t_BufferBinds[1].bufferOffset = 0;
		t_BufferBinds[1].bufferSize = sizeof(ModelBufferInfo) * s_RendererInst.modelMatrixMax;
	}
	{//IndexConstantBind
		t_ConstantBinds[0].offset = 0;
		t_ConstantBinds[0].stage = RENDER_SHADER_STAGE::VERTEX;
		t_ConstantBinds[0].size = 64;
	}



	s_PerFrameInfo.perFrameDescriptorLayout.ptrHandle = nullptr;
	s_PerFrameInfo.perFrameDescriptor = RenderBackend::CreateDescriptor(
		s_PerFrameInfo.perFrameDescriptorLayout,
		t_DescriptorCreateInfo);



#pragma endregion //Descriptor
	const wchar_t* t_DX12ShaderPaths[2];
	t_DX12ShaderPaths[0] = L"../Resources/Shaders/HLSLShaders/DebugVert.hlsl";
	t_DX12ShaderPaths[1] = L"../Resources/Shaders/HLSLShaders/DebugFrag.hlsl";

	Shader::ShaderCodeHandle t_ShaderHandles[2];
	t_ShaderHandles[0] = Shader::CompileShader(
		t_DX12ShaderPaths[0],
		L"main",
		RENDER_SHADER_STAGE::VERTEX);
	t_ShaderHandles[1] = Shader::CompileShader(
		t_DX12ShaderPaths[1],
		L"main",
		RENDER_SHADER_STAGE::FRAGMENT_PIXEL);

	Buffer t_ShaderBuffer;
	Shader::GetShaderCodeBuffer(t_ShaderHandles[0], t_ShaderBuffer);
	ShaderCreateInfo t_ShaderBuffers[2];
	t_ShaderBuffers[0].buffer = t_ShaderBuffer;
	t_ShaderBuffers[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;

	Shader::GetShaderCodeBuffer(t_ShaderHandles[1], t_ShaderBuffer);
	t_ShaderBuffers[1].buffer = t_ShaderBuffer;
	t_ShaderBuffers[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;

	//Constant buffer for indices.
	ConstantBufferInfo a_ConstBufferInfo;
	a_ConstBufferInfo.offset = 0;
	a_ConstBufferInfo.size = 64;
	a_ConstBufferInfo.stage = RENDER_SHADER_STAGE::VERTEX;

	RenderPipelineCreateInfo t_PipelineCreateInfo{};
	t_PipelineCreateInfo.framebufferHandle = t_FrameBuffer;
	t_PipelineCreateInfo.shaderCreateInfos = BB::Slice(t_ShaderBuffers, 2);
	t_PipelineCreateInfo.descLayoutHandles = &s_PerFrameInfo.perFrameDescriptorLayout;
	t_PipelineCreateInfo.descLayoutCount = 1;
	t_PipelineCreateInfo.constantBuffers = &a_ConstBufferInfo;
	t_PipelineCreateInfo.constantBufferCount = 1;

	t_Pipeline = RenderBackend::CreatePipeline(t_PipelineCreateInfo);


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
	RenderBackend::DestroyBuffer(s_PerFrameInfo.perFrameBuffer);
	RenderBackend::UnmapMemory(s_PerFrameInfo.perFrameTransferBuffer);
	RenderBackend::DestroyBuffer(s_PerFrameInfo.perFrameTransferBuffer);
	RenderBackend::DestroyDescriptorSetLayout(s_PerFrameInfo.perFrameDescriptorLayout);

	RenderBackend::DestroyPipeline(t_Pipeline);
	RenderBackend::DestroyFrameBuffer(t_FrameBuffer);
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
	return Pointer::Add(s_PerFrameInfo.transferBufferPtr, sizeof(glm::mat4) * 2);
}

RModelHandle BB::Render::CreateRawModel(const CreateRawModelInfo& a_CreateInfo)
{
	Model t_Model;

	//t_Model.pipelineHandle = a_CreateInfo.pipeline;
	t_Model.pipelineHandle = t_Pipeline;

	{
		UploadBufferChunk t_StageBuffer = t_UploadBuffer->Alloc(a_CreateInfo.vertices.sizeInBytes());
		memcpy(t_StageBuffer.memory, a_CreateInfo.vertices.data(), a_CreateInfo.vertices.sizeInBytes());

		RenderBufferCreateInfo t_VertexInfo;
		t_VertexInfo.usage = RENDER_BUFFER_USAGE::VERTEX;
		t_VertexInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
		t_VertexInfo.size = a_CreateInfo.vertices.sizeInBytes();
		t_VertexInfo.data = nullptr;

		t_Model.vertexBuffer = RenderBackend::CreateBuffer(t_VertexInfo);
		t_Model.vertexBufferView;

		RenderCopyBufferInfo t_CopyInfo;
		t_CopyInfo.transferCommandHandle = t_RecordingTransfer;
		t_CopyInfo.src = t_UploadBuffer->Buffer();
		t_CopyInfo.dst = t_Model.vertexBuffer;
		t_CopyInfo.CopyRegionCount = 1;
		t_CopyInfo.copyRegions = BBnewArr(m_TempAllocator, 1, RenderCopyBufferInfo::CopyRegions);
		t_CopyInfo.copyRegions->srcOffset = t_StageBuffer.offset;
		t_CopyInfo.copyRegions->dstOffset = 0;
		t_CopyInfo.copyRegions->size = a_CreateInfo.vertices.sizeInBytes();

		RenderBackend::CopyBuffer(t_CopyInfo);
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
		t_Model.indexBufferView;

		RenderCopyBufferInfo t_CopyInfo;
		t_CopyInfo.transferCommandHandle = t_RecordingTransfer;
		t_CopyInfo.src = t_UploadBuffer->Buffer();
		t_CopyInfo.dst = t_Model.indexBuffer;
		t_CopyInfo.CopyRegionCount = 1;
		t_CopyInfo.copyRegions = BBnewArr(m_TempAllocator, 1, RenderCopyBufferInfo::CopyRegions);
		t_CopyInfo.copyRegions->srcOffset = t_StageBuffer.offset;
		t_CopyInfo.copyRegions->dstOffset = 0;
		t_CopyInfo.copyRegions->size = a_CreateInfo.indices.sizeInBytes();

		RenderBackend::CopyBuffer(t_CopyInfo);
	}
	
	t_Model.linearNodes = BBnewArr(m_SystemAllocator, 1, Model::Node);
	t_Model.nodes = t_Model.linearNodes;

	t_Model.linearNodes->mesh = BBnew(m_SystemAllocator, Model::Mesh);
	t_Model.linearNodes->mesh->primitiveCount = 1;
	t_Model.linearNodes->mesh->primitives = BBnew(m_SystemAllocator, Model::Primitive);
	t_Model.linearNodes->mesh->primitives->indexStart = 0;
	t_Model.linearNodes->mesh->primitives->indexCount = static_cast<uint32_t>(a_CreateInfo.indices.size());
	t_Model.linearNodeCount = 1;
	t_Model.nodeCount = 1;

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
	t_ExecuteInfos[1] = {};
	t_ExecuteInfos[1].commands = &t_GraphicCommands[s_CurrentFrame];
	t_ExecuteInfos[1].commandCount = 1;
	t_ExecuteInfos[1].waitQueueCount = 1;
	t_ExecuteInfos[1].waitQueues = &t_TransferQueue;
	t_ExecuteInfos[1].waitValues = &t_WaitValue;

	RenderBackend::ExecutePresentCommands(t_GraphicsQueue, t_ExecuteInfos[1]);
	PresentFrameInfo t_PresentFrame{};
	//t_PresentFrame = 1;
	//t_PresentFrame.waitSemaphores = &t_RenderSemaphores[s_CurrentFrame];
	s_CurrentFrame = RenderBackend::PresentFrame(t_PresentFrame);
	RenderBackend::Update();
}

void BB::Render::ResizeWindow(const uint32_t a_X, const uint32_t a_Y)
{
	RenderBackend::ResizeWindow(a_X, a_Y);
}
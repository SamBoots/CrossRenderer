#include "RenderFrontend.h"
#include "RenderBackend.h"

#include "Storage/Slotmap.h"
#include "OS/OSDevice.h"


using namespace BB;
using namespace BB::Render;

static FreelistAllocator_t m_SystemAllocator{ mbSize * 4 };
static TemporaryAllocator m_TempAllocator{ m_SystemAllocator };

struct RendererInst
{
	uint32_t frameBufferAmount;
	uint32_t modelMatrixMax = 1;

	Slotmap<Model> models{ m_SystemAllocator };
};

struct PerFrameInfo
{
	uint64_t transferBufferSize;

	RBufferHandle perFrameBuffer;
	RBufferHandle perFrameTransferBuffer;
	void* transferBufferPtr;
	RDescriptorLayoutHandle perFrameDescriptorLayout;
	RDescriptorHandle* perFrameDescriptor;
};

FrameBufferHandle t_FrameBuffer;
CommandListHandle t_CommandLists[3];
CommandListHandle t_TransferCommandList;
PipelineHandle t_Pipeline;

ModelBufferInfo t_ModelInfo;

static FrameIndex s_CurrentFrame;

static RendererInst s_RendererInst;
static PerFrameInfo s_PerFrameInfo;

void BB::Render::InitRenderer(const WindowHandle a_WindowHandle, const LibHandle a_RenderLib, const bool a_Debug)
{
	BB::Array<RENDER_EXTENSIONS> t_Extensions{ m_TempAllocator };
	t_Extensions.emplace_back(RENDER_EXTENSIONS::STANDARD_VULKAN_INSTANCE);
	t_Extensions.emplace_back(RENDER_EXTENSIONS::PHYSICAL_DEVICE_EXTRA_PROPERTIES);
	if (a_Debug)
	{
		t_Extensions.emplace_back(RENDER_EXTENSIONS::DEBUG);
	}
	BB::Array<RENDER_EXTENSIONS> t_DeviceExtensions{ m_TempAllocator };
	t_DeviceExtensions.emplace_back(RENDER_EXTENSIONS::STANDARD_VULKAN_DEVICE);
	t_DeviceExtensions.emplace_back(RENDER_EXTENSIONS::PIPELINE_EXTENDED_DYNAMIC_STATE);

	int t_WindowWidth;
	int t_WindowHeight;
	OS::GetWindowSize(a_WindowHandle, t_WindowWidth, t_WindowHeight);

	RenderBackendCreateInfo t_BackendCreateInfo;
	t_BackendCreateInfo.getApiFuncPtr = (PFN_RenderGetAPIFunctions)OS::LibLoadFunc(a_RenderLib, "GetRenderAPIFunctions");
	t_BackendCreateInfo.extensions = t_Extensions;
	t_BackendCreateInfo.deviceExtensions = t_DeviceExtensions;
	t_BackendCreateInfo.hwnd = reinterpret_cast<HWND>(OS::GetOSWindowHandle(a_WindowHandle));
	t_BackendCreateInfo.version = 1;
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

	t_ModelInfo.model = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	memcpy(Pointer::Add(s_PerFrameInfo.transferBufferPtr, sizeof(CameraBufferInfo)), &t_ModelInfo, sizeof(t_ModelInfo));

	RenderBufferCreateInfo t_PerFrameBuffer;
	t_PerFrameBuffer.size = t_perFrameBufferEntireSize;
	t_PerFrameBuffer.usage = RENDER_BUFFER_USAGE::STORAGE;
	t_PerFrameBuffer.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
	t_PerFrameBuffer.data = nullptr;
	s_PerFrameInfo.perFrameBuffer = RenderBackend::CreateBuffer(t_PerFrameBuffer);

	RenderDescriptorCreateInfo t_DescriptorCreateInfo{};
	t_DescriptorCreateInfo.bufferBindCount = 1;
	t_DescriptorCreateInfo.textureBindCount = 0;
	t_DescriptorCreateInfo.bufferBind = BBnewArr(m_TempAllocator,
		t_DescriptorCreateInfo.bufferBindCount,
		RenderDescriptorCreateInfo::BufferBind);
	{
		t_DescriptorCreateInfo.bufferBind[0].binding = RENDER_DESCRIPTOR_BINDING::PER_FRAME;
		t_DescriptorCreateInfo.bufferBind[0].stage = RENDER_SHADER_STAGE::VERTEX;
		t_DescriptorCreateInfo.bufferBind[0].type = DESCRIPTOR_BUFFER_TYPE::STORAGE_BUFFER_DYNAMIC;
		t_DescriptorCreateInfo.bufferBind[0].bufferInfoCount = 1;
		t_DescriptorCreateInfo.bufferBind[0].bufferInfos = BBnewArr(m_TempAllocator,
			t_DescriptorCreateInfo.bufferBind[0].bufferInfoCount,
			RenderDescriptorCreateInfo::BufferBind::BufferInfo);
		{//Camera buffer
			t_DescriptorCreateInfo.bufferBind[0].bufferInfos[0].buffer = s_PerFrameInfo.perFrameBuffer;
			//Do offset in the forloop due to the per frame nature
			t_DescriptorCreateInfo.bufferBind[0].bufferInfos[0].offset = 0;
			t_DescriptorCreateInfo.bufferBind[0].bufferInfos[0].size = sizeof(CameraBufferInfo) + sizeof(ModelBufferInfo);
		}
	}

	s_PerFrameInfo.perFrameDescriptorLayout.ptrHandle = nullptr;
	s_PerFrameInfo.perFrameDescriptor = RenderBackend::CreateDescriptors(
		m_SystemAllocator,
		s_PerFrameInfo.perFrameDescriptorLayout,
		t_DescriptorCreateInfo);



#pragma endregion //Descriptor

	ShaderCreateInfo t_ShaderBuffers[2];
	t_ShaderBuffers[0].buffer = OS::ReadFile(m_SystemAllocator, "../Resources/Shaders/Vulkan/debugVert.spv");
	t_ShaderBuffers[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;
	t_ShaderBuffers[1].buffer = OS::ReadFile(m_SystemAllocator, "../Resources/Shaders/Vulkan/debugFrag.spv");
	t_ShaderBuffers[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;


	const wchar_t* t_DX12ShaderPaths[2];
	t_DX12ShaderPaths[0] = L"../Resources/Shaders/HLSLShaders/DebugVert.hlsl";
	t_DX12ShaderPaths[1] = L"../Resources/Shaders/HLSLShaders/DebugFrag.hlsl";

	RenderPipelineCreateInfo t_PipelineCreateInfo{};
	t_PipelineCreateInfo.framebufferHandle = t_FrameBuffer;
	t_PipelineCreateInfo.shaderCreateInfos = BB::Slice(t_ShaderBuffers, 2);
	t_PipelineCreateInfo.shaderPaths = t_DX12ShaderPaths;
	t_PipelineCreateInfo.shaderPathCount = 2;
	t_PipelineCreateInfo.descLayoutHandles = &s_PerFrameInfo.perFrameDescriptorLayout;
	t_PipelineCreateInfo.descLayoutSize = 1;

	t_Pipeline = RenderBackend::CreatePipeline(t_PipelineCreateInfo);

	RenderCommandListCreateInfo t_CmdCreateInfo;
	t_CmdCreateInfo.queueType = RENDER_QUEUE_TYPE::GRAPHICS;
	t_CmdCreateInfo.bufferCount = 5;
	t_CommandLists[0] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	t_CommandLists[1] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	t_CommandLists[2] = RenderBackend::CreateCommandList(t_CmdCreateInfo);

	//just reuse the struct above.
	t_CmdCreateInfo.queueType = RENDER_QUEUE_TYPE::TRANSFER;
	t_CmdCreateInfo.bufferCount = 1;
	t_TransferCommandList = RenderBackend::CreateCommandList(t_CmdCreateInfo);

	BBfree(m_SystemAllocator, t_ShaderBuffers[0].buffer.data);
	BBfree(m_SystemAllocator, t_ShaderBuffers[1].buffer.data);
}

void BB::Render::DestroyRenderer()
{
	RenderBackend::WaitGPUReady();
	for (auto it = s_RendererInst.models.begin(); it < s_RendererInst.models.end(); it++)
	{
		RenderBackend::DestroyBuffer(it->indexBuffer);
		RenderBackend::DestroyBuffer(it->vertexBuffer);
		//RenderBackend::DestroyBuffer(it->value.indexBuffer);
	}
	RenderBackend::DestroyBuffer(s_PerFrameInfo.perFrameBuffer);
	RenderBackend::UnmapMemory(s_PerFrameInfo.perFrameTransferBuffer);
	RenderBackend::DestroyBuffer(s_PerFrameInfo.perFrameTransferBuffer);
	RenderBackend::DestroyDescriptorSetLayout(s_PerFrameInfo.perFrameDescriptorLayout);

	RenderBackend::DestroyPipeline(t_Pipeline);
	RenderBackend::DestroyFrameBuffer(t_FrameBuffer);
	for (size_t i = 0; i < _countof(t_CommandLists); i++)
	{
		RenderBackend::DestroyCommandList(t_CommandLists[i]);
	}
	RenderBackend::DestroyCommandList(t_TransferCommandList);
	RenderBackend::DestroyBackend();

	BBfreeArr(m_SystemAllocator, s_PerFrameInfo.perFrameDescriptor);
}

void BB::Render::Update(const float a_DeltaTime)
{
	for (auto t_It = s_RendererInst.models.begin(); t_It < s_RendererInst.models.end(); t_It++)
	{
		Render::StartFrame();
		//Record rendering commands.
		auto t_Recording = Render::StartRecordCmds();

		ModelBufferInfo t_ModelInfo;
		t_ModelInfo.model = glm::rotate(glm::mat4(1.0f),  glm::radians(90.0f * a_DeltaTime), glm::vec3(0.0f, 0.0f, 1.0f));
		//copy over our matrix.
		memcpy(Pointer::Add(s_PerFrameInfo.transferBufferPtr, sizeof(CameraBufferInfo)), &t_ModelInfo, sizeof(t_ModelInfo));
		RenderCopyBufferInfo t_CopyInfo;
		t_CopyInfo.transferCommandHandle = t_TransferCommandList;
		t_CopyInfo.src = s_PerFrameInfo.perFrameTransferBuffer;
		t_CopyInfo.dst = s_PerFrameInfo.perFrameBuffer;
		RenderCopyBufferInfo::CopyRegions t_CopyRegion;
		t_CopyRegion.size = sizeof(CameraBufferInfo) + (sizeof(ModelBufferInfo) * s_RendererInst.modelMatrixMax);
		t_CopyRegion.srcOffset = 0;
		t_CopyRegion.dstOffset = t_CopyRegion.size * s_CurrentFrame;

		t_CopyInfo.copyRegions = &t_CopyRegion;
		t_CopyInfo.CopyRegionCount = 1;

		RenderBackend::CopyBuffer(t_CopyInfo);

		Render::DrawModel(t_Recording, *t_It);
		Render::EndRecordCmds(t_Recording);

		Render::EndFrame();
	}
}

void BB::Render::SetProjection(const glm::mat4& a_Proj)
{
	memcpy(Pointer::Add(s_PerFrameInfo.transferBufferPtr, sizeof(glm::mat4)), &a_Proj, sizeof(glm::mat4));
}

void BB::Render::SetView(const glm::mat4& a_View)
{
	memcpy(s_PerFrameInfo.transferBufferPtr, &a_View, sizeof(glm::mat4));
}

RModelHandle BB::Render::CreateRawModel(const CreateRawModelInfo& a_CreateInfo)
{
	Model t_Model;

	//t_Model.pipelineHandle = a_CreateInfo.pipeline;
	t_Model.pipelineHandle = t_Pipeline;

	{
		RenderBufferCreateInfo t_StagingInfo;
		t_StagingInfo.usage = RENDER_BUFFER_USAGE::STAGING;
		t_StagingInfo.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
		t_StagingInfo.size = a_CreateInfo.vertices.sizeInBytes();
		t_StagingInfo.data = a_CreateInfo.vertices.data();

		RBufferHandle t_StagingBuffer = RenderBackend::CreateBuffer(t_StagingInfo);

		RenderBufferCreateInfo t_VertexInfo;
		t_VertexInfo.usage = RENDER_BUFFER_USAGE::VERTEX;
		t_VertexInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
		t_VertexInfo.size = a_CreateInfo.vertices.sizeInBytes();
		t_VertexInfo.data = nullptr;

		t_Model.vertexBuffer = RenderBackend::CreateBuffer(t_VertexInfo);
		t_Model.vertexBufferView;

		RenderCopyBufferInfo t_CopyInfo;
		t_CopyInfo.transferCommandHandle = t_TransferCommandList;
		t_CopyInfo.src = t_StagingBuffer;
		t_CopyInfo.dst = t_Model.vertexBuffer;
		t_CopyInfo.CopyRegionCount = 1;
		t_CopyInfo.copyRegions = BBnewArr(m_TempAllocator, 1, RenderCopyBufferInfo::CopyRegions);
		t_CopyInfo.copyRegions->srcOffset = 0;
		t_CopyInfo.copyRegions->dstOffset = 0;
		t_CopyInfo.copyRegions->size = a_CreateInfo.vertices.sizeInBytes();

		RenderBackend::CopyBuffer(t_CopyInfo);

		//cleanup staging buffer.
		RenderBackend::DestroyBuffer(t_StagingBuffer);
	}

	{
		RenderBufferCreateInfo t_StagingInfo;
		t_StagingInfo.usage = RENDER_BUFFER_USAGE::STAGING;
		t_StagingInfo.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
		t_StagingInfo.size = a_CreateInfo.indices.sizeInBytes();
		t_StagingInfo.data = a_CreateInfo.indices.data();

		RBufferHandle t_StagingBuffer = RenderBackend::CreateBuffer(t_StagingInfo);

		RenderBufferCreateInfo t_IndexInfo;
		t_IndexInfo.usage = RENDER_BUFFER_USAGE::INDEX;
		t_IndexInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
		t_IndexInfo.size = a_CreateInfo.indices.sizeInBytes();
		t_IndexInfo.data = nullptr;

		t_Model.indexBuffer = RenderBackend::CreateBuffer(t_IndexInfo);
		t_Model.indexBufferView;

		RenderCopyBufferInfo t_CopyInfo;
		t_CopyInfo.transferCommandHandle = t_TransferCommandList;
		t_CopyInfo.src = t_StagingBuffer;
		t_CopyInfo.dst = t_Model.indexBuffer;
		t_CopyInfo.CopyRegionCount = 1;
		t_CopyInfo.copyRegions = BBnewArr(m_TempAllocator, 1, RenderCopyBufferInfo::CopyRegions);
		t_CopyInfo.copyRegions->srcOffset = 0;
		t_CopyInfo.copyRegions->dstOffset = 0;
		t_CopyInfo.copyRegions->size = a_CreateInfo.indices.sizeInBytes();

		RenderBackend::CopyBuffer(t_CopyInfo);

		//cleanup staging buffer.
		RenderBackend::DestroyBuffer(t_StagingBuffer);
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

	return RModelHandle(s_RendererInst.models.insert(t_Model));
}

RecordingCommandListHandle BB::Render::StartRecordCmds()
{
	return RenderBackend::StartCommandList(t_CommandLists[s_CurrentFrame], t_FrameBuffer);
}

void BB::Render::EndRecordCmds(const RecordingCommandListHandle a_Handle)
{
	RenderBackend::EndCommandList(a_Handle);
}

void BB::Render::DrawModel(const RecordingCommandListHandle a_Handle, const Model& a_Model)
{
	RenderBackend::BindPipeline(a_Handle, a_Model.pipelineHandle);
	uint64_t t_BufferOffsets[1]{ 0 };
	RenderBackend::BindVertexBuffers(a_Handle, &a_Model.vertexBuffer, t_BufferOffsets, 1);
	RenderBackend::BindIndexBuffer(a_Handle, a_Model.indexBuffer, 0);
	uint32_t dynOffset = (sizeof(CameraBufferInfo) + sizeof(ModelBufferInfo)) * s_CurrentFrame;
	RenderBackend::BindDescriptorSets(a_Handle, 0, 1, s_PerFrameInfo.perFrameDescriptor, 1, &dynOffset);
	for (uint32_t i = 0; i < a_Model.linearNodeCount; i++)
	{
		for (size_t j = 0; j < a_Model.linearNodes[i].mesh->primitiveCount; j++)
		{
			RenderBackend::DrawIndexed(a_Handle,
				a_Model.linearNodes[i].mesh->primitives[j].indexCount,
				1,
				a_Model.linearNodes[i].mesh->primitives[j].indexStart,
				0,
				0);
		}
	}
}

void BB::Render::StartFrame()
{
	s_CurrentFrame = RenderBackend::StartFrame();
	RenderBackend::ResetCommandList(t_CommandLists[s_CurrentFrame]);
}

void BB::Render::EndFrame()
{
	RenderBackend::RenderFrame(t_CommandLists[s_CurrentFrame], t_FrameBuffer, t_Pipeline);
	RenderBackend::Update();
}

void BB::Render::ResizeWindow(const uint32_t a_X, const uint32_t a_Y)
{
	RenderBackend::ResizeWindow(a_X, a_Y);
}
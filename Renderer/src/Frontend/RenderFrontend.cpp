#include "RenderFrontend.h"
#include "RenderBackend.h"

#include "Storage/Slotmap.h"
#include "OS/OSDevice.h"

using namespace BB;
using namespace BB::Render;

static FreelistAllocator_t m_SystemAllocator{ mbSize * 4 };
static TemporaryAllocator m_TempAllocator{ m_SystemAllocator };

struct RendererInfo
{
	RenderAPI currentAPI;
	bool debug;
};

struct RendererInst
{
	Slotmap<Model> models{ m_SystemAllocator };
};

FrameBufferHandle t_FrameBuffer;
CommandListHandle t_CommandList;
PipelineHandle t_Pipeline;
RBufferHandle t_Buffer;

static RendererInfo s_RendererInfo;
static RendererInst s_RendererInst;

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
	t_BackendCreateInfo.getApiFuncPtr = (PFN_RenderGetAPIFunctions)OS::LibLoadFunc(a_RenderLib, "GetVulkanAPIFunctions");
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

	s_RendererInfo.debug = a_Debug;


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

	ShaderCreateInfo t_ShaderBuffers[2];
	t_ShaderBuffers[0].buffer = OS::ReadFile(m_SystemAllocator, "../Resources/Shaders/Vulkan/debugVert.spv");
	t_ShaderBuffers[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;
	t_ShaderBuffers[1].buffer = OS::ReadFile(m_SystemAllocator, "../Resources/Shaders/Vulkan/debugFrag.spv");
	t_ShaderBuffers[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT;


	const wchar_t* t_DX12ShaderPaths[2];
	t_DX12ShaderPaths[0] = L"../Resources/Shaders/HLSLShaders/DebugVert.hlsl";
	t_DX12ShaderPaths[1] = L"../Resources/Shaders/HLSLShaders/DebugFrag.hlsl";

	RenderPipelineCreateInfo t_PipelineCreateInfo;
	t_PipelineCreateInfo.framebufferHandle = t_FrameBuffer;
	t_PipelineCreateInfo.shaderCreateInfos = BB::Slice(t_ShaderBuffers, 2);
	t_PipelineCreateInfo.shaderPaths = t_DX12ShaderPaths;
	t_PipelineCreateInfo.shaderPathCount = 2;

	t_Pipeline = RenderBackend::CreatePipeline(t_PipelineCreateInfo);

	RenderCommandListCreateInfo t_CmdCreateInfo;
	t_CmdCreateInfo.bufferCount = 5;
	t_CommandList = RenderBackend::CreateCommandList(t_CmdCreateInfo);

	Vertex t_Vertex[3];
	t_Vertex[0] = { {0.0f, -0.5f}, {1.0f, 1.0f, 1.0f} };
	t_Vertex[1] = { {0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} };
	t_Vertex[2] = { {-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f} };

	RenderBufferCreateInfo t_RenderBuffer{};
	t_RenderBuffer.size = sizeof(t_Vertex);
	t_RenderBuffer.data = nullptr; //We will upload with pfn_BufferCopyData.
	t_RenderBuffer.usage = RENDER_BUFFER_USAGE::VERTEX;
	t_RenderBuffer.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
	t_Buffer = RenderBackend::CreateBuffer(t_RenderBuffer);

	RenderBackend::BufferCopyData(t_Buffer, &t_Vertex, sizeof(t_Vertex), 0);

	BBfree(m_SystemAllocator, t_ShaderBuffers[0].buffer.data);
	BBfree(m_SystemAllocator, t_ShaderBuffers[1].buffer.data);
}

void BB::Render::DestroyRenderer()
{
	RenderBackend::WaitGPUReady();
	RenderBackend::DestroyBuffer(t_Buffer);
	RenderBackend::DestroyPipeline(t_Pipeline);
	RenderBackend::DestroyFrameBuffer(t_FrameBuffer);
	RenderBackend::DestroyCommandList(t_CommandList);
	RenderBackend::DestroyBackend();
	s_RendererInfo.currentAPI = RenderAPI::NONE;
	s_RendererInfo.debug = false;
}

RModelHandle CreateRawModel(const CreateRawModelInfo& a_CreateInfo)
{
	Model t_Model;

	RenderBufferCreateInfo t_BufferInfo;
	t_BufferInfo.usage = RENDER_BUFFER_USAGE::VERTEX;
	t_BufferInfo.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
	t_BufferInfo.size = a_CreateInfo.vertices.sizeInBytes();
	t_BufferInfo.data = a_CreateInfo.vertices.data();

	t_Model.vertexBuffer = RenderBackend::CreateBuffer(t_BufferInfo);
	t_Model.vertexBufferView;

	//t_BufferInfo.usage = RENDER_BUFFER_USAGE::INDEX;
	//t_BufferInfo.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
	//t_BufferInfo.size = a_CreateInfo.vertices.sizeInBytes();
	//t_BufferInfo.data = a_CreateInfo.vertices.data();
	
	//t_Model.indexBuffer = RenderBackend::CreateBuffer(t_BufferInfo);
	t_Model.indexBufferView;
	
	Model::Node* t_BaseNode = BBnew(m_SystemAllocator, Model::Node);
	t_BaseNode->mesh = BBnew(m_SystemAllocator, Model::Mesh);
	t_BaseNode->mesh->primitiveCount = 1;
	t_BaseNode->mesh->primitives = BBnew(m_SystemAllocator, Model::Primitive);
	t_BaseNode->mesh->primitives->indexStart = 0;
	t_BaseNode->mesh->primitives->indexCount = t_BufferInfo.size;
	t_Model.linearNodes = BBnewArr(m_SystemAllocator, 1, Model::Node);
	t_Model.nodes = BBnewArr(m_SystemAllocator, 1, Model::Node);

	return RModelHandle(s_RendererInst.models.insert(t_Model));
}

RecordingCommandListHandle BB::Render::StartRecordCmds()
{
	return RenderBackend::StartCommandList(t_CommandList, t_FrameBuffer);
}

void BB::Render::EndRecordCmds(const RecordingCommandListHandle a_Handle)
{
	RenderBackend::EndCommandList(a_Handle);
}

void BB::Render::DrawModel(const RecordingCommandListHandle a_Handle, const RModelHandle a_ModelHandle)
{
	const Model& t_Model = s_RendererInst.models.find(a_ModelHandle.handle);
	
	RenderBackend::DrawBuffers(a_Handle, &t_Model.vertexBuffer, 1);
}

void BB::Render::Update()
{
	auto t_Recording = RenderBackend::StartCommandList(t_CommandList, t_FrameBuffer);
	RenderBackend::BindPipeline(t_Recording, t_Pipeline);
	RenderBackend::DrawBuffers(t_Recording, &t_Buffer, 1);
	RenderBackend::EndCommandList(t_Recording);

	RenderBackend::RenderFrame(t_CommandList, t_FrameBuffer, t_Pipeline);
	RenderBackend::Update();
}

void BB::Render::ResizeWindow(const uint32_t a_X, const uint32_t a_Y)
{
	RenderBackend::ResizeWindow(a_X, a_Y);
}
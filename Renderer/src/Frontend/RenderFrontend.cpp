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

static RendererInfo s_RendererInfo;
static RendererInst s_RendererInst;

void BB::Render::InitRenderer(const WindowHandle a_WindowHandle, const RenderAPI a_RenderAPI, const bool a_Debug)
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
	t_BackendCreateInfo.api = a_RenderAPI;
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

	s_RendererInfo.currentAPI = a_RenderAPI;
	s_RendererInfo.debug = a_Debug;
}

void BB::Render::DestroyRenderer()
{
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

void BB::Render::StartRecordCmds()
{

}

void BB::Render::EndRecordCmds()
{

}

void BB::Render::DrawModel(const RModelHandle a_ModelHandle)
{
	const Model& t_Model = s_RendererInst.models.find(a_ModelHandle.handle);
	
	//RenderBackend::DrawBuffers(t_Model.vertexBuffer, 1);
}

void BB::Render::Update()
{
	RenderBackend::Update();
}

void BB::Render::ResizeWindow(const uint32_t a_X, const uint32_t a_Y)
{
	RenderBackend::ResizeWindow(a_X, a_Y);
}
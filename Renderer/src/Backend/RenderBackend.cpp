#include "RenderBackend.h"
#include "VulkanBackend.h"

#include "Utils/Slice.h"
#include "BBString.h"

#include "OS/OSDevice.h"

using namespace BB;

FrameBufferHandle t_FrameBuffer;
CommandListHandle t_CommandList;
PipelineHandle t_Pipeline;

void RenderBackend::InitBackend(BB::WindowHandle a_WindowHandle, RenderAPI a_RenderAPI, bool a_Debug)
{
	SetFunctions(a_RenderAPI);

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
	OS::GetWindowSize(a_WindowHandle,t_WindowWidth,t_WindowHeight);

	RenderBackendCreateInfo t_BackendCreateInfo;
	t_BackendCreateInfo.extensions = t_Extensions;
	t_BackendCreateInfo.deviceExtensions = t_DeviceExtensions;
	t_BackendCreateInfo.hwnd = reinterpret_cast<HWND>(OS::GetOSWindowHandle(a_WindowHandle));
	t_BackendCreateInfo.version = 1;
	t_BackendCreateInfo.validationLayers = a_Debug;
	t_BackendCreateInfo.appName = "TestName";
	t_BackendCreateInfo.engineName = "TestEngine";
	t_BackendCreateInfo.windowWidth = static_cast<uint32_t>(t_WindowWidth);
	t_BackendCreateInfo.windowHeight = static_cast<uint32_t>(t_WindowHeight);

	m_APIbackend = pfn_CreateBackend(m_TempAllocator, t_BackendCreateInfo);

	RenderFrameBufferCreateInfo t_FrameBufferCreateInfo;
	//VkRenderpass info
	t_FrameBufferCreateInfo.colorLoadOp = RENDER_LOAD_OP::CLEAR;
	t_FrameBufferCreateInfo.colorStoreOp = RENDER_STORE_OP::STORE;
	t_FrameBufferCreateInfo.colorInitialLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
	t_FrameBufferCreateInfo.colorFinalLayout = RENDER_IMAGE_LAYOUT::PRESENT;

	//VkFrameBuffer info
	t_FrameBufferCreateInfo.width = static_cast<uint32_t>(t_WindowWidth);
	t_FrameBufferCreateInfo.height = static_cast<uint32_t>(t_WindowHeight);

	t_FrameBuffer = pfn_CreateFrameBuffer(m_TempAllocator, t_FrameBufferCreateInfo);

	ShaderCreateInfo t_ShaderBuffers[2];
	t_ShaderBuffers[0].buffer = OS::ReadFile(m_SystemAllocator, "../Resources/Shaders/Vulkan/debugVert.spv");
	t_ShaderBuffers[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;
	t_ShaderBuffers[1].buffer = OS::ReadFile(m_SystemAllocator, "../Resources/Shaders/Vulkan/debugFrag.spv");
	t_ShaderBuffers[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT;

	RenderPipelineCreateInfo t_PipelineCreateInfo;
	t_PipelineCreateInfo.framebufferHandle = t_FrameBuffer;
	t_PipelineCreateInfo.shaderCreateInfos = BB::Slice(t_ShaderBuffers, 2);

	t_Pipeline = pfn_CreatePipelineFunc(m_TempAllocator, t_PipelineCreateInfo);

	t_CommandList = pfn_CreateCommandList(m_TempAllocator, 5);

	Vertex t_Vertex[3];
	t_Vertex[0] = { {0.0f, -0.5f}, {1.0f, 1.0f, 1.0f} };
	t_Vertex[1] = { {0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} };
	t_Vertex[2] = { {-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f} };

	RenderBufferCreateInfo t_RenderBuffer;
	t_RenderBuffer.size = sizeof(t_Vertex);
	t_RenderBuffer.usage = RENDER_BUFFER_USAGE::VERTEX;
	t_RenderBuffer.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
	RBufferHandle buffer = pfn_CreateBuffer(t_RenderBuffer);

	RDeviceBufferView t_View;
	t_View.offset = 0;
	t_View.size = sizeof(t_Vertex);

	pfn_BufferCopyData(buffer, &t_Vertex, t_View);

	//VulkanDestroyCommandList(m_SystemAllocator, t_CommandList, *vkBackend);
	//VulkanDestroyFramebuffer(m_SystemAllocator, t_FrameBuffer, *vkBackend);
	//VulkanDestroyPipeline(t_Pipeline, *vkBackend);
	//VulkanDestroyBackend(m_SystemAllocator, *reinterpret_cast<VulkanBackend*>(APIbackend));
	//BBfree<VulkanBackend>(m_SystemAllocator, reinterpret_cast<VulkanBackend*>(APIbackend));
	BBfree(m_SystemAllocator, t_ShaderBuffers[0].buffer.data);
	BBfree(m_SystemAllocator, t_ShaderBuffers[1].buffer.data);
}

void RenderBackend::DestroyBackend()
{
	pfn_WaitDeviceReady();
	pfn_DestroyPipeline(t_Pipeline);
	pfn_DestroyFrameBuffer(t_FrameBuffer);
	pfn_DestroyCommandList(t_CommandList);
	pfn_DestroyBackend(m_APIbackend);
}

void RenderBackend::Update()
{
	pfn_RenderFrame(m_TempAllocator,
		t_CommandList,
		t_FrameBuffer,
		t_Pipeline);
	m_TempAllocator.Clear();
}

void RenderBackend::CreateShader(const ShaderCreateInfo& t_ShaderInfo)
{

}

void RenderBackend::ResizeWindow(uint32_t a_X, uint32_t a_Y)
{
	pfn_ResizeWindow(m_TempAllocator, m_APIbackend, a_X, a_Y);
}

void RenderBackend::SetFunctions(RenderAPI a_RenderAPI)
{
	APIBackendFunctionPointersCreateInfo t_Functions;
	t_Functions.createBackend = &pfn_CreateBackend;
	t_Functions.createFrameBuffer = &pfn_CreateFrameBuffer;
	t_Functions.createPipeline = &pfn_CreatePipelineFunc;
	t_Functions.createCommandList = &pfn_CreateCommandList;
	t_Functions.createBuffer = &pfn_CreateBuffer;

	t_Functions.bufferCopyData = &pfn_BufferCopyData;

	t_Functions.resizeWindow = &pfn_ResizeWindow;
	t_Functions.renderFrame = &pfn_RenderFrame;
	t_Functions.waitDevice = &pfn_WaitDeviceReady;

	t_Functions.destroyBackend = &pfn_DestroyBackend;
	t_Functions.destroyFrameBuffer = &pfn_DestroyFrameBuffer;
	t_Functions.destroyPipeline = &pfn_DestroyPipeline;
	t_Functions.destroyCommandList = &pfn_DestroyCommandList;
	t_Functions.destroyBuffer = &pfn_DestroyBuffer;

	switch (a_RenderAPI)
	{
	case RenderAPI::VULKAN:
		GetVulkanAPIFunctions(t_Functions);
		break;
	case RenderAPI::DX12:
		//GetDX12APIFunctions(t_Functions);
		break;
	default:
		BB_ASSERT(false, "Trying to get functions from an API you don't support.");
		break;
	}
}
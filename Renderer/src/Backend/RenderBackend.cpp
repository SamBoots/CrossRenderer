#include "RenderBackend.h"
#include "VulkanBackend.h"

#include "Utils/Slice.h"
#include "BBString.h"

#include "OS/OSDevice.h"

using namespace BB;

void RenderBackend::InitBackend(BB::WindowHandle a_WindowHandle, RenderAPI a_RenderAPI, bool a_Debug)
{
	currentRenderAPI = a_RenderAPI;
	APIbackend = BBnew<VulkanBackend>(m_SystemAllocator);
	VulkanBackend* vkBackend = reinterpret_cast<VulkanBackend*>(APIbackend);

	BB::Array<const char*> t_Extensions{ m_TempAllocator };
	t_Extensions.emplace_back("VK_KHR_win32_surface");
	t_Extensions.emplace_back("VK_KHR_surface");
	t_Extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	if (a_Debug)
	{
		t_Extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	BB::Array<const char*> t_DeviceExtensions{ m_TempAllocator };
	t_DeviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	t_DeviceExtensions.emplace_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

	int t_WindowWidth;
	int t_WindowHeight;
	AppOSDevice().GetWindowSize(a_WindowHandle,t_WindowWidth,t_WindowHeight);

	VulkanBackendCreateInfo t_BackendCreateInfo;
	t_BackendCreateInfo.extensions = t_Extensions;
	t_BackendCreateInfo.deviceExtensions = t_DeviceExtensions;
	t_BackendCreateInfo.hwnd = reinterpret_cast<HWND>(AppOSDevice().GetOSWindowHandle(a_WindowHandle));
	t_BackendCreateInfo.version = 0;
	t_BackendCreateInfo.validationLayers = true;
	t_BackendCreateInfo.appName = "TestName";
	t_BackendCreateInfo.engineName = "TestEngine";
	t_BackendCreateInfo.windowWidth = static_cast<uint32_t>(t_WindowWidth);
	t_BackendCreateInfo.windowHeight = static_cast<uint32_t>(t_WindowHeight);

	*vkBackend = VKCreateBackend(m_TempAllocator, m_SystemAllocator, t_BackendCreateInfo);

	RenderPassCreateInfo t_FrameBufferCreateInfo;
	t_FrameBufferCreateInfo.swapchainFormat = vkBackend->mainSwapChain.imageFormat;
	t_FrameBufferCreateInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	t_FrameBufferCreateInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	t_FrameBufferCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	t_FrameBufferCreateInfo.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VulkanFrameBuffer t_FrameBuffer = CreateFrameBuffer(m_SystemAllocator,
		m_TempAllocator, *vkBackend, t_FrameBufferCreateInfo);

	ShaderCreateInfo t_ShaderBuffers[2];
	t_ShaderBuffers[0].buffer = AppOSDevice().ReadFile(m_SystemAllocator, "../Resources/Shaders/Vulkan/debugVert.spv");
	t_ShaderBuffers[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;
	t_ShaderBuffers[1].buffer = AppOSDevice().ReadFile(m_SystemAllocator, "../Resources/Shaders/Vulkan/debugFrag.spv");
	t_ShaderBuffers[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT;

	VulkanPipelineCreateInfo t_PipelineCreateInfo;
	t_PipelineCreateInfo.pVulkanFrameBuffer = &t_FrameBuffer;
	t_PipelineCreateInfo.shaderCreateInfos = BB::Slice(t_ShaderBuffers, 2);

	VulkanPipeline t_Pipeline = CreatePipeline(m_TempAllocator, *vkBackend, t_PipelineCreateInfo);

	VkDestroyFramebuffer(m_SystemAllocator, t_FrameBuffer, *vkBackend);
	DestroyPipeline(t_Pipeline, *vkBackend);
	VKDestroyBackend(m_SystemAllocator, *reinterpret_cast<VulkanBackend*>(APIbackend));
	BBfree<VulkanBackend>(m_SystemAllocator, reinterpret_cast<VulkanBackend*>(APIbackend));
	BBfree(m_SystemAllocator, t_ShaderBuffers[0].buffer.data);
	BBfree(m_SystemAllocator, t_ShaderBuffers[1].buffer.data);
}

void RenderBackend::DestroyBackend()
{
	switch (currentRenderAPI)
	{
	case RenderAPI::VULKAN:
		//VKDestroyBackend(m_SystemAllocator, *reinterpret_cast<VulkanBackend*>(APIbackend));
		//BBfree<VulkanBackend>(m_SystemAllocator, reinterpret_cast<VulkanBackend*>(APIbackend));
		break;
	default:
		break;
	}
}

void RenderBackend::Update()
{
	m_TempAllocator.Clear();
}

void RenderBackend::CreateShader(const ShaderCreateInfo& t_ShaderInfo)
{

}
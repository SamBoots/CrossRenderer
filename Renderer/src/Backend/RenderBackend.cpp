#include "RenderBackend.h"
#include "VulkanBackend.h"

#include "Utils/Slice.h"
#include "BBString.h"

#include "OS/OSDevice.h"

using namespace BB;

FrameBufferHandle t_FrameBuffer;
CommandListHandle t_CommandList;
PipelineHandle t_Pipeline;

APIRenderBackendHandle RenderBackend::InitBackend(BB::WindowHandle a_WindowHandle, RenderAPI a_RenderAPI, bool a_Debug)
{
	APIRenderBackendHandle t_ReturnHandle;
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
	AppOSDevice().GetWindowSize(a_WindowHandle,t_WindowWidth,t_WindowHeight);

	RenderBackendCreateInfo t_BackendCreateInfo;
	t_BackendCreateInfo.extensions = t_Extensions;
	t_BackendCreateInfo.deviceExtensions = t_DeviceExtensions;
	t_BackendCreateInfo.hwnd = reinterpret_cast<HWND>(AppOSDevice().GetOSWindowHandle(a_WindowHandle));
	t_BackendCreateInfo.version = 0;
	t_BackendCreateInfo.validationLayers = true;
	t_BackendCreateInfo.appName = "TestName";
	t_BackendCreateInfo.engineName = "TestEngine";
	t_BackendCreateInfo.windowWidth = static_cast<uint32_t>(t_WindowWidth);
	t_BackendCreateInfo.windowHeight = static_cast<uint32_t>(t_WindowHeight);

	t_ReturnHandle = pfn_CreateBackend(m_SystemAllocator, m_TempAllocator, t_BackendCreateInfo);

	RenderFrameBufferCreateInfo t_FrameBufferCreateInfo;
	//VkRenderpass info
	t_FrameBufferCreateInfo.colorLoadOp = RENDER_LOAD_OP::CLEAR;
	t_FrameBufferCreateInfo.colorStoreOp = RENDER_STORE_OP::STORE;
	t_FrameBufferCreateInfo.colorInitialLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
	t_FrameBufferCreateInfo.colorFinalLayout = RENDER_IMAGE_LAYOUT::PRESENT;

	//VkFrameBuffer info
	t_FrameBufferCreateInfo.width = static_cast<uint32_t>(t_WindowWidth);
	t_FrameBufferCreateInfo.height = static_cast<uint32_t>(t_WindowHeight);

	t_FrameBuffer = CreateFrameBuffer(t_FrameBufferCreateInfo);

	ShaderCreateInfo t_ShaderBuffers[2];
	t_ShaderBuffers[0].buffer = AppOSDevice().ReadFile(m_SystemAllocator, "../Resources/Shaders/Vulkan/debugVert.spv");
	t_ShaderBuffers[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;
	t_ShaderBuffers[1].buffer = AppOSDevice().ReadFile(m_SystemAllocator, "../Resources/Shaders/Vulkan/debugFrag.spv");
	t_ShaderBuffers[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT;

	RenderPipelineCreateInfo t_PipelineCreateInfo;
	t_PipelineCreateInfo.framebufferHandle = t_FrameBuffer;
	t_PipelineCreateInfo.shaderCreateInfos = BB::Slice(t_ShaderBuffers, 2);

	t_Pipeline = CreatePipeline(t_PipelineCreateInfo);

	t_CommandList = CreateCommandlist();

	//VulkanDestroyCommandList(m_SystemAllocator, t_CommandList, *vkBackend);
	//VulkanDestroyFramebuffer(m_SystemAllocator, t_FrameBuffer, *vkBackend);
	//VulkanDestroyPipeline(t_Pipeline, *vkBackend);
	//VulkanDestroyBackend(m_SystemAllocator, *reinterpret_cast<VulkanBackend*>(APIbackend));
	//BBfree<VulkanBackend>(m_SystemAllocator, reinterpret_cast<VulkanBackend*>(APIbackend));
	BBfree(m_SystemAllocator, t_ShaderBuffers[0].buffer.data);
	BBfree(m_SystemAllocator, t_ShaderBuffers[1].buffer.data);

	return t_ReturnHandle;
}

FrameBufferHandle RenderBackend::CreateFrameBuffer(const RenderFrameBufferCreateInfo& a_Info)
{
	return pfn_CreateFrameBuffer(m_TempAllocator, a_Info);
}

PipelineHandle RenderBackend::CreatePipeline(const RenderPipelineCreateInfo& a_Info)
{
	return pfn_CreatePipelineFunc(m_TempAllocator, a_Info);
}

CommandListHandle RenderBackend::CreateCommandlist()
{
	return pfn_CreateCommandList(m_TempAllocator, 3);
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

void RenderBackend::SetFunctions(RenderAPI a_RenderAPI)
{
	APIBackendFunctionPointersCreateInfo t_Functions;
	t_Functions.createBackend = &pfn_CreateBackend;
	t_Functions.createFrameBuffer = &pfn_CreateFrameBuffer;
	t_Functions.createPipeline = &pfn_CreatePipelineFunc;
	t_Functions.createCommandList = &pfn_CreateCommandList;

	t_Functions.renderFrame = &pfn_RenderFrame;
	t_Functions.waitDevice = &pfn_WaitDeviceReady;

	t_Functions.destroyBackend = &pfn_DestroyBackend;
	t_Functions.destroyFrameBuffer = &pfn_DestroyFrameBuffer;
	t_Functions.destroyPipeline = &pfn_DestroyPipeline;
	t_Functions.destroyCommandList = &pfn_DestroyCommandList;

	GetVulkanAPIFunctions(t_Functions);
}


void RenderBackend::DestroyBackend(APIRenderBackendHandle a_Handle)
{
	pfn_WaitDeviceReady();
	DestroyCommandList(t_CommandList);
	DestroyPipeline(t_Pipeline);
	DestroyFrameBuffer(t_FrameBuffer);

	pfn_DestroyBackend(a_Handle);
}

void RenderBackend::DestroyFrameBuffer(FrameBufferHandle a_Handle) 
{
	pfn_DestroyFrameBuffer(t_FrameBuffer);
}
void RenderBackend::DestroyPipeline(PipelineHandle a_Handle)
{
	pfn_DestroyPipeline(t_Pipeline);
}
void RenderBackend::DestroyCommandList(CommandListHandle a_Handle)
{
	pfn_DestroyCommandList(t_CommandList);
}
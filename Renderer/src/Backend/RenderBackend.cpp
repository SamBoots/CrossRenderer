#include "RenderBackend.h"
#include "VulkanBackend.h"
#include "DX12Backend.h"
#include "VulkanCommon.h"

#include "Utils/Slice.h"
#include "BBString.h"

#include "OS/OSDevice.h"

using namespace BB;

static FreelistAllocator_t m_SystemAllocator{ mbSize * 4 };
static TemporaryAllocator m_TempAllocator{ m_SystemAllocator };


static RenderAPIFunctions s_ApiFunc;

void SetAPIFunctions(RenderAPI a_RenderAPI)
{
	switch (a_RenderAPI)
	{
	case RenderAPI::VULKAN:
		GetVulkanAPIFunctions(s_ApiFunc);
		break;
	case RenderAPI::DX12:
		GetDX12APIFunctions(s_ApiFunc);
		break;
	default:
		BB_ASSERT(false, "Trying to get functions from an API you don't support.");
		break;
	}
}


FrameBufferHandle t_FrameBuffer;
CommandListHandle t_CommandList;
PipelineHandle t_Pipeline;
RBufferHandle t_Buffer;

void BB::RenderBackend::InitBackend(const RenderBackendCreateInfo& a_CreateInfo)
{
	SetAPIFunctions(a_CreateInfo.api);

	s_ApiFunc.createBackend(m_TempAllocator, a_CreateInfo);

	RenderFrameBufferCreateInfo t_FrameBufferCreateInfo;
	//VkRenderpass info
	t_FrameBufferCreateInfo.colorLoadOp = RENDER_LOAD_OP::CLEAR;
	t_FrameBufferCreateInfo.colorStoreOp = RENDER_STORE_OP::STORE;
	t_FrameBufferCreateInfo.colorInitialLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
	t_FrameBufferCreateInfo.colorFinalLayout = RENDER_IMAGE_LAYOUT::PRESENT;

	//VkFrameBuffer info
	t_FrameBufferCreateInfo.width = static_cast<uint32_t>(a_CreateInfo.windowWidth);
	t_FrameBufferCreateInfo.height = static_cast<uint32_t>(a_CreateInfo.windowHeight);

	t_FrameBuffer = s_ApiFunc.createFrameBuffer(m_TempAllocator, t_FrameBufferCreateInfo);

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

	t_Pipeline = s_ApiFunc.createPipeline(m_TempAllocator, t_PipelineCreateInfo);

	RenderCommandListCreateInfo t_CmdCreateInfo;
	t_CmdCreateInfo.bufferCount = 5;
	t_CommandList = s_ApiFunc.createCommandList(m_TempAllocator, t_CmdCreateInfo);

	Vertex t_Vertex[3];
	t_Vertex[0] = { {0.0f, -0.5f}, {1.0f, 1.0f, 1.0f} };
	t_Vertex[1] = { {0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} };
	t_Vertex[2] = { {-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f} };

	RenderBufferCreateInfo t_RenderBuffer{};
	t_RenderBuffer.size = sizeof(t_Vertex);
	t_RenderBuffer.data = nullptr; //We will upload with pfn_BufferCopyData.
	t_RenderBuffer.usage = RENDER_BUFFER_USAGE::VERTEX;
	t_RenderBuffer.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
	t_Buffer = s_ApiFunc.createBuffer(t_RenderBuffer);

	RDeviceBufferView t_View;
	t_View.offset = 0;
	t_View.size = sizeof(t_Vertex);

	s_ApiFunc.bufferCopyData(t_Buffer, &t_Vertex, t_View);

	BBfree(m_SystemAllocator, t_ShaderBuffers[0].buffer.data);
	BBfree(m_SystemAllocator, t_ShaderBuffers[1].buffer.data);
}

void BB::RenderBackend::DestroyBackend()
{
	s_ApiFunc.waitDevice();
	s_ApiFunc.destroyBuffer(t_Buffer);
	s_ApiFunc.destroyPipeline(t_Pipeline);
	s_ApiFunc.destroyFrameBuffer(t_FrameBuffer);
	s_ApiFunc.destroyCommandList(t_CommandList);
	s_ApiFunc.destroyBackend();
}

FrameBufferHandle BB::RenderBackend::CreateFrameBuffer(const RenderFrameBufferCreateInfo& a_CreateInfo)
{
	return s_ApiFunc.createFrameBuffer(m_TempAllocator, a_CreateInfo);
}

PipelineHandle BB::RenderBackend::CreatePipeline(const RenderPipelineCreateInfo& a_CreateInfo)
{
	return s_ApiFunc.createPipeline(m_TempAllocator, a_CreateInfo);
}

CommandListHandle BB::RenderBackend::CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo)
{
	return s_ApiFunc.createCommandList(m_TempAllocator, a_CreateInfo);
}

RBufferHandle BB::RenderBackend::CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo)
{
	return s_ApiFunc.createBuffer(a_CreateInfo);
}

RecordingCommandListHandle BB::RenderBackend::StartCommandList(const CommandListHandle a_CmdHandle)
{
	//s_ApiFunc.startCommandList(a_Handle, t_FrameBuffer);
	return VulkanStartCommandList(a_CmdHandle, t_FrameBuffer);
}

void BB::RenderBackend::EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle)
{
	//s_ApiFunc.endCommandList(a_Handle);
	return VulkanEndCommandList(a_RecordingCmdHandle);
}

void BB::RenderBackend::DrawBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, RBufferHandle* a_BufferHandles, const size_t a_BufferCount)
{
	
}

void BB::RenderBackend::Update()
{
	auto t_Recording = StartCommandList(t_CommandList);
	VulkanSetPipeline(t_Recording, t_Pipeline);
	VulkanDrawBuffers(t_Recording, &t_Buffer, 1);
	EndCommandList(t_Recording);

	s_ApiFunc.renderFrame(m_TempAllocator,
		t_CommandList,
		t_FrameBuffer,
		t_Pipeline);
	m_TempAllocator.Clear();
}

void BB::RenderBackend::CreateShader(const ShaderCreateInfo& t_ShaderInfo)
{

}

void BB::RenderBackend::ResizeWindow(uint32_t a_X, uint32_t a_Y)
{
	s_ApiFunc.resizeWindow(m_TempAllocator, a_X, a_Y);
}
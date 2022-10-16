#include "RenderBackend.h"
#include "VulkanBackend.h"
#include "DX12Backend.h"

#include "Utils/Slice.h"
#include "BBString.h"

#include "OS/OSDevice.h"

using namespace BB;

static FreelistAllocator_t m_SystemAllocator{ mbSize * 4 };
static TemporaryAllocator m_TempAllocator{ m_SystemAllocator };


static RenderAPIFunctions apiFuncs;

void SetAPIFunctions(RenderAPI a_RenderAPI)
{
	switch (a_RenderAPI)
	{
	case RenderAPI::VULKAN:
		GetVulkanAPIFunctions(apiFuncs);
		break;
	case RenderAPI::DX12:
		GetDX12APIFunctions(apiFuncs);
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

	apiFuncs.createBackend(m_TempAllocator, a_CreateInfo);

	RenderFrameBufferCreateInfo t_FrameBufferCreateInfo;
	//VkRenderpass info
	t_FrameBufferCreateInfo.colorLoadOp = RENDER_LOAD_OP::CLEAR;
	t_FrameBufferCreateInfo.colorStoreOp = RENDER_STORE_OP::STORE;
	t_FrameBufferCreateInfo.colorInitialLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
	t_FrameBufferCreateInfo.colorFinalLayout = RENDER_IMAGE_LAYOUT::PRESENT;

	//VkFrameBuffer info
	t_FrameBufferCreateInfo.width = static_cast<uint32_t>(a_CreateInfo.windowWidth);
	t_FrameBufferCreateInfo.height = static_cast<uint32_t>(a_CreateInfo.windowHeight);

	t_FrameBuffer = apiFuncs.createFrameBuffer(m_TempAllocator, t_FrameBufferCreateInfo);

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

	t_Pipeline = apiFuncs.createPipeline(m_TempAllocator, t_PipelineCreateInfo);

	t_CommandList = apiFuncs.createCommandList(m_TempAllocator, 5);

	Vertex t_Vertex[3];
	t_Vertex[0] = { {0.0f, -0.5f}, {1.0f, 1.0f, 1.0f} };
	t_Vertex[1] = { {0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} };
	t_Vertex[2] = { {-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f} };

	RenderBufferCreateInfo t_RenderBuffer{};
	t_RenderBuffer.size = sizeof(t_Vertex);
	t_RenderBuffer.data = nullptr; //We will upload with pfn_BufferCopyData.
	t_RenderBuffer.usage = RENDER_BUFFER_USAGE::VERTEX;
	t_RenderBuffer.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
	t_Buffer = apiFuncs.createBuffer(t_RenderBuffer);

	RDeviceBufferView t_View;
	t_View.offset = 0;
	t_View.size = sizeof(t_Vertex);

	apiFuncs.bufferCopyData(t_Buffer, &t_Vertex, t_View);

	BBfree(m_SystemAllocator, t_ShaderBuffers[0].buffer.data);
	BBfree(m_SystemAllocator, t_ShaderBuffers[1].buffer.data);
}

void BB::RenderBackend::DestroyBackend()
{
	apiFuncs.waitDevice();
	apiFuncs.destroyBuffer(t_Buffer);
	apiFuncs.destroyPipeline(t_Pipeline);
	apiFuncs.destroyFrameBuffer(t_FrameBuffer);
	apiFuncs.destroyCommandList(t_CommandList);
	apiFuncs.destroyBackend();
}

void BB::RenderBackend::Update()
{
	apiFuncs.renderFrame(m_TempAllocator,
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
	apiFuncs.resizeWindow(m_TempAllocator, a_X, a_Y);
}
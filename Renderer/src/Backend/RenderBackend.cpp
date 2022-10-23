#include "RenderBackend.h"
#include "VulkanBackend.h"

#include "Utils/Slice.h"
#include "BBString.h"

#include "OS/OSDevice.h"

using namespace BB;

static FreelistAllocator_t m_SystemAllocator{ mbSize * 4 };
static TemporaryAllocator m_TempAllocator{ m_SystemAllocator };


static RenderAPIFunctions s_ApiFunc;

void BB::RenderBackend::InitBackend(const RenderBackendCreateInfo& a_CreateInfo)
{
	a_CreateInfo.getApiFuncPtr(s_ApiFunc);

	s_ApiFunc.createBackend(m_TempAllocator, a_CreateInfo);
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

void BB::RenderBackend::BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset)
{
	return s_ApiFunc.bufferCopyData(a_Handle, a_Data, a_Size, a_Offset);
}

RecordingCommandListHandle BB::RenderBackend::StartCommandList(const CommandListHandle a_CmdHandle, const FrameBufferHandle a_FrameHandle)
{
	//s_ApiFunc.startCommandList(a_Handle, t_FrameBuffer);
	return s_ApiFunc.startCommandList(a_CmdHandle, a_FrameHandle);
}

void BB::RenderBackend::EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle)
{
	//s_ApiFunc.endCommandList(a_Handle);
	return s_ApiFunc.endCommandList(a_RecordingCmdHandle);
}

void BB::RenderBackend::BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline)
{
	s_ApiFunc.bindPipeline(a_RecordingCmdHandle, a_Pipeline);
}

void BB::RenderBackend::DrawBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_BufferHandles, const size_t a_BufferCount)
{
	s_ApiFunc.drawBuffers(a_RecordingCmdHandle, a_BufferHandles, a_BufferCount);
}

void BB::RenderBackend::RenderFrame(const CommandListHandle a_CommandHandle, const FrameBufferHandle a_FrameBufferHandle, const PipelineHandle a_PipeHandle)
{
	s_ApiFunc.renderFrame(m_TempAllocator, a_CommandHandle, a_FrameBufferHandle, a_PipeHandle);
}

void BB::RenderBackend::Update()
{

	m_TempAllocator.Clear();
}

void BB::RenderBackend::WaitGPUReady()
{
	s_ApiFunc.waitDevice();
}

void BB::RenderBackend::ResizeWindow(uint32_t a_X, uint32_t a_Y)
{
	s_ApiFunc.resizeWindow(m_TempAllocator, a_X, a_Y);
}

void BB::RenderBackend::CreateShader(const ShaderCreateInfo& t_ShaderInfo)
{

}

void BB::RenderBackend::DestroyBackend()
{
	s_ApiFunc.destroyBackend();
}

void BB::RenderBackend::DestroyFrameBuffer(const FrameBufferHandle a_Handle)
{
	s_ApiFunc.destroyFrameBuffer(a_Handle);
}

void BB::RenderBackend::DestroyPipeline(const PipelineHandle a_Handle)
{
	s_ApiFunc.destroyPipeline(a_Handle);
}

void BB::RenderBackend::DestroyCommandList(const CommandListHandle a_Handle)
{
	s_ApiFunc.destroyCommandList(a_Handle);
}

void BB::RenderBackend::DestroyBuffer(const RBufferHandle a_Handle)
{
	s_ApiFunc.destroyBuffer(a_Handle);
}
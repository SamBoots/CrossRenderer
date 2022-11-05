#include "RenderBackend.h"
#include "VulkanBackend.h"

#include "Utils/Slice.h"
#include "BBString.h"

#include "OS/OSDevice.h"

using namespace BB;

static FreelistAllocator_t m_SystemAllocator{ mbSize * 4 };
static TemporaryAllocator m_TempAllocator{ m_SystemAllocator };

static RenderAPIFunctions s_ApiFunc;

static BackendInfo s_BackendInfo;

const uint32_t BB::RenderBackend::GetFrameBufferAmount()
{
	return s_BackendInfo.framebufferCount;
}

//Preferably use the value that you get from BB::RenderBackend::StartFrame
const FrameIndex BB::RenderBackend::GetCurrentFrameBufferIndex()
{
	return s_BackendInfo.currentFrame;
}

void BB::RenderBackend::InitBackend(const RenderBackendCreateInfo& a_CreateInfo)
{
	a_CreateInfo.getApiFuncPtr(s_ApiFunc);

	s_BackendInfo = s_ApiFunc.createBackend(m_TempAllocator, a_CreateInfo);
}

RDescriptorHandle BB::RenderBackend::CreateDescriptor(RDescriptorLayoutHandle& a_Layout, const RenderDescriptorCreateInfo& a_CreateInfo)
{
	return s_ApiFunc.createDescriptor(m_TempAllocator, a_Layout, a_CreateInfo);
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
	s_ApiFunc.bufferCopyData(a_Handle, a_Data, a_Size, a_Offset);
}

void BB::RenderBackend::CopyBuffer(const RenderCopyBufferInfo& a_CopyInfo)
{
	s_ApiFunc.copyBuffer(m_TempAllocator, a_CopyInfo);
}

RecordingCommandListHandle BB::RenderBackend::StartCommandList(const CommandListHandle a_CmdHandle, const FrameBufferHandle a_FrameHandle)
{
	return s_ApiFunc.startCommandList(a_CmdHandle, a_FrameHandle);
}

void BB::RenderBackend::ResetCommandList(const CommandListHandle a_CmdHandle)
{
	s_ApiFunc.resetCommandList(a_CmdHandle);
}

void BB::RenderBackend::EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle)
{
	s_ApiFunc.endCommandList(a_RecordingCmdHandle);
}

void BB::RenderBackend::BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline)
{
	s_ApiFunc.bindPipeline(a_RecordingCmdHandle, a_Pipeline);
}

void BB::RenderBackend::BindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount)
{
	s_ApiFunc.bindVertBuffers(a_RecordingCmdHandle, a_Buffers, a_BufferOffsets, a_BufferCount);
}

void BB::RenderBackend::BindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset)
{
	s_ApiFunc.bindIndexBuffer(a_RecordingCmdHandle, a_Buffer, a_Offset);
}

void BB::RenderBackend::BindDescriptorSets(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_FirstSet, const uint32_t a_SetCount, const RDescriptorHandle* a_Sets, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets)
{
	s_ApiFunc.bindDescriptor(a_RecordingCmdHandle, a_FirstSet, a_SetCount, a_Sets, a_DynamicOffsetCount, a_DynamicOffsets);
}

void BB::RenderBackend::DrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance)
{
	s_ApiFunc.drawVertex(a_RecordingCmdHandle, a_VertexCount, a_InstanceCount, a_FirstVertex, a_FirstInstance);
}

void BB::RenderBackend::DrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance)
{
	s_ApiFunc.drawIndex(a_RecordingCmdHandle, a_IndexCount, a_InstanceCount, a_FirstIndex, a_VertexOffset, a_FirstInstance);
}

FrameIndex BB::RenderBackend::StartFrame()
{
	return s_BackendInfo.currentFrame = s_ApiFunc.startFrame();
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

void BB::RenderBackend::DestroyDescriptorSetLayout(const RDescriptorLayoutHandle a_Handle)
{
	s_ApiFunc.destroyDescriptorLayout(a_Handle);
}

void BB::RenderBackend::DestroyDescriptorSet(const RDescriptorHandle a_Handle)
{
	s_ApiFunc.destroyDescriptor(a_Handle);
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
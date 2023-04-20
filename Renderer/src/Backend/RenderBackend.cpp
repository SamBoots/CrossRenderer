#include "RenderBackend.h"
#include "VulkanBackend.h"

#include "Utils/Slice.h"
#include "BBString.h"

using namespace BB;

static RenderAPIFunctions s_ApiFunc;

static BackendInfo s_BackendInfo;

PipelineBuilder::PipelineBuilder(const PipelineInitInfo& a_InitInfo)
{
	BB_ASSERT(a_InitInfo.renderTargetBlendCount < 8, "More then 8 blending targets! This will not work with directx12.")
	m_BuilderHandle = s_ApiFunc.pipelineBuilderInit(a_InitInfo);
}

PipelineBuilder::~PipelineBuilder()
{
	BB_ASSERT(m_BuilderHandle.handle == 0, "Unfinished pipeline destructed! Big memory leak and improper graphics API usage.");
}

void PipelineBuilder::BindDescriptor(const RDescriptorHandle a_Handle)
{
	s_ApiFunc.pipelineBuilderBindDescriptor(m_BuilderHandle, a_Handle);
}

void PipelineBuilder::BindShaders(const Slice<BB::ShaderCreateInfo> a_ShaderInfo)
{
	s_ApiFunc.pipelineBuilderBindShaders(m_BuilderHandle, a_ShaderInfo);
}

void PipelineBuilder::BindAttributes(const PipelineAttributes& a_AttributeInfo)
{
	s_ApiFunc.pipelineBuilderBindAttributes(m_BuilderHandle, a_AttributeInfo);
}

PipelineHandle PipelineBuilder::BuildPipeline()
{
	//Buildpipeline will also destroy the builder information. 
	const PipelineHandle t_ReturnHandle = s_ApiFunc.pipelineBuilderBuildPipeline(m_BuilderHandle);
	m_BuilderHandle.handle = 0; //Set the handle to 0 to indicate we can safely destruct the class.
	return t_ReturnHandle;
}

UploadBuffer::UploadBuffer(const uint64_t a_Size)
	: m_Size(a_Size)
{
	RenderBufferCreateInfo t_UploadBufferInfo;
	t_UploadBufferInfo.size = m_Size;
	t_UploadBufferInfo.usage = RENDER_BUFFER_USAGE::STAGING;
	t_UploadBufferInfo.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
	t_UploadBufferInfo.data = nullptr;
	m_Buffer = RenderBackend::CreateBuffer(t_UploadBufferInfo);

	m_Offset = 0;
	m_Start = RenderBackend::MapMemory(m_Buffer);
}

UploadBuffer::~UploadBuffer()
{
	RenderBackend::UnmapMemory(m_Buffer);
	RenderBackend::DestroyBuffer(m_Buffer);
}

UploadBufferChunk UploadBuffer::Alloc(const uint64_t a_Size)
{
	BB_ASSERT(m_Size > m_Offset + a_Size, "Now enough space to alloc in the uploadbuffer.");
	UploadBufferChunk t_Chunk{};
	t_Chunk.memory = Pointer::Add(m_Start, m_Offset);
	t_Chunk.offset = m_Offset;
	m_Offset += a_Size;
	return t_Chunk;
}

void UploadBuffer::Clear()
{
	m_Offset = 0;
	memset(m_Start, 0, m_Size);
}

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

	s_BackendInfo = s_ApiFunc.createBackend(a_CreateInfo);
}

RDescriptorHandle BB::RenderBackend::CreateDescriptor(const RenderDescriptorCreateInfo& a_Info)
{
	return s_ApiFunc.createDescriptor(a_Info);
}

CommandQueueHandle BB::RenderBackend::CreateCommandQueue(const RenderCommandQueueCreateInfo& a_CreateInfo)
{
	return s_ApiFunc.createCommandQueue(a_CreateInfo);
}

CommandAllocatorHandle BB::RenderBackend::CreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo)
{
	return s_ApiFunc.createCommandAllocator(a_CreateInfo);
}

CommandListHandle BB::RenderBackend::CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo)
{
	return s_ApiFunc.createCommandList(a_CreateInfo);
}

RBufferHandle BB::RenderBackend::CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo)
{
	return s_ApiFunc.createBuffer(a_CreateInfo);
}

RImageHandle BB::RenderBackend::CreateImage(const RenderImageCreateInfo& a_CreateInfo)
{
	BB_ASSERT(a_CreateInfo.width != 0, "Image width is 0! Choose a correct width for an image.");
	BB_ASSERT(a_CreateInfo.height != 0, "Image height is 0! Choose a correct height for an image.");
	BB_ASSERT(a_CreateInfo.depth != 0, "Image depth is 0! Standard 2d texture should have a depth of 1.");
	BB_ASSERT(a_CreateInfo.arrayLayers != 0, "Image arrayLayers is 0! Standard should be 1 if you do not do anything special for a 2d image.");
	BB_ASSERT(a_CreateInfo.mipLevels != 0, "Image mipLevels is 0! Standard should be 1 if you do not do mips for an image.");
	return s_ApiFunc.createImage(a_CreateInfo);
}

RFenceHandle BB::RenderBackend::CreateFence(const FenceCreateInfo& a_Info)
{
	return s_ApiFunc.createFence(a_Info);
}

void BB::RenderBackend::UpdateDescriptorBuffer(const UpdateDescriptorBufferInfo& a_Info)
{
	s_ApiFunc.updateDescriptorBuffer(a_Info);
}

void BB::RenderBackend::UpdateDescriptorImage(const UpdateDescriptorImageInfo& a_Info)
{
	s_ApiFunc.updateDescriptorImage(a_Info);
}


void BB::RenderBackend::ResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle)
{
	s_ApiFunc.resetCommandAllocator(a_CmdAllocatorHandle);
}

RecordingCommandListHandle BB::RenderBackend::StartCommandList(const CommandListHandle a_CmdHandle)
{
	return s_ApiFunc.startCommandList(a_CmdHandle);
}

void BB::RenderBackend::EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle)
{
	s_ApiFunc.endCommandList(a_RecordingCmdHandle);
}

void BB::RenderBackend::StartRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_StartInfo)
{
	s_ApiFunc.startRendering(a_RecordingCmdHandle, a_StartInfo);
}

void BB::RenderBackend::SetScissor(const RecordingCommandListHandle a_RecordingCmdHandle, const ScissorInfo& a_ScissorInfo)
{
	s_ApiFunc.setScissor(a_RecordingCmdHandle, a_ScissorInfo);
}

void BB::RenderBackend::EndRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo)
{
	s_ApiFunc.endRendering(a_RecordingCmdHandle, a_EndInfo);
}

ImageReturnInfo BB::RenderBackend::GetImageInfo(const RImageHandle a_Handle)
{
	return s_ApiFunc.getImageInfo(a_Handle);
}

void BB::RenderBackend::CopyBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferInfo& a_CopyInfo)
{
	s_ApiFunc.copyBuffer(a_RecordingCmdHandle, a_CopyInfo);
}

void BB::RenderBackend::CopyBufferImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferImageInfo& a_CopyInfo)
{
	s_ApiFunc.copyBufferImage(a_RecordingCmdHandle, a_CopyInfo);
}

void BB::RenderBackend::TransitionImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderTransitionImageInfo a_TransitionInfo)
{
	s_ApiFunc.transitionImage(a_RecordingCmdHandle, a_TransitionInfo);
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

void BB::RenderBackend::BindDescriptors(const RecordingCommandListHandle a_RecordingCmdHandle, const RDescriptorHandle* a_Sets, const uint32_t a_SetCount, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets)
{
	s_ApiFunc.bindDescriptors(a_RecordingCmdHandle, a_Sets, a_SetCount, a_DynamicOffsetCount, a_DynamicOffsets);
}

void BB::RenderBackend::BindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_Offset, const void* a_Data)
{
	BB_WARNING(a_DwordCount * sizeof(uint32_t) < 128, "Constant size is bigger then 128, this might not work on all hardware for Vulkan!", WarningType::HIGH);
	s_ApiFunc.bindConstant(a_RecordingCmdHandle, a_ConstantIndex, a_DwordCount, a_Offset, a_Data);
}

void BB::RenderBackend::DrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance)
{
	s_ApiFunc.drawVertex(a_RecordingCmdHandle, a_VertexCount, a_InstanceCount, a_FirstVertex, a_FirstInstance);
}

void BB::RenderBackend::DrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance)
{
	s_ApiFunc.drawIndex(a_RecordingCmdHandle, a_IndexCount, a_InstanceCount, a_FirstIndex, a_VertexOffset, a_FirstInstance);
}

void BB::RenderBackend::BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset)
{
	s_ApiFunc.bufferCopyData(a_Handle, a_Data, a_Size, a_Offset);
}

void* BB::RenderBackend::MapMemory(const RBufferHandle a_Handle)
{
	return s_ApiFunc.mapMemory(a_Handle);
}

void BB::RenderBackend::UnmapMemory(const RBufferHandle a_Handle)
{
	s_ApiFunc.unmapMemory(a_Handle);
}

void BB::RenderBackend::StartFrame(const StartFrameInfo& a_StartInfo)
{
	s_ApiFunc.startFrame(a_StartInfo);
}

void BB::RenderBackend::ExecuteCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount)
{
	s_ApiFunc.executeCommands(a_ExecuteQueue, a_ExecuteInfos, a_ExecuteInfoCount);
}

void BB::RenderBackend::ExecutePresentCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo)
{
	s_ApiFunc.executePresentCommands(a_ExecuteQueue, a_ExecuteInfo);
}

FrameIndex BB::RenderBackend::PresentFrame(const PresentFrameInfo& a_PresentInfo)
{
	return s_BackendInfo.currentFrame = s_ApiFunc.presentFrame(a_PresentInfo);
}

void BB::RenderBackend::WaitGPUReady()
{
	s_ApiFunc.waitDevice();
}

void BB::RenderBackend::ResizeWindow(uint32_t a_X, uint32_t a_Y)
{
	s_ApiFunc.resizeWindow(a_X, a_Y);
}

uint64_t BB::RenderBackend::NextQueueFenceValue(const CommandQueueHandle a_Handle)
{
	return s_ApiFunc.nextQueueFenceValue(a_Handle);
}

uint64_t BB::RenderBackend::NextFenceValue(const RFenceHandle a_Handle)
{
	return s_ApiFunc.nextFenceValue(a_Handle);
}

void BB::RenderBackend::DestroyBackend()
{
	s_ApiFunc.destroyBackend();
}

void BB::RenderBackend::DestroyDescriptor(const RDescriptorHandle a_Handle)
{
	s_ApiFunc.destroyDescriptor(a_Handle);
}

void BB::RenderBackend::DestroyPipeline(const PipelineHandle a_Handle)
{
	s_ApiFunc.destroyPipeline(a_Handle);
}

void BB::RenderBackend::DestroyCommandQueue(const CommandQueueHandle a_Handle)
{
	s_ApiFunc.destroyCommandQueue(a_Handle);
}

void BB::RenderBackend::DestroyCommandAllocator(const CommandAllocatorHandle a_Handle)
{
	s_ApiFunc.destroyCommandAllocator(a_Handle);
}

void BB::RenderBackend::DestroyCommandList(const CommandListHandle a_Handle)
{
	s_ApiFunc.destroyCommandList(a_Handle);
}

void BB::RenderBackend::DestroyBuffer(const RBufferHandle a_Handle)
{
	s_ApiFunc.destroyBuffer(a_Handle);
}

void BB::RenderBackend::DestroyImage(const RImageHandle a_Handle)
{
	s_ApiFunc.destroyImage(a_Handle);
}

void BB::RenderBackend::DestroyFence(const RFenceHandle a_Handle)
{
	s_ApiFunc.destroyFence(a_Handle);
}
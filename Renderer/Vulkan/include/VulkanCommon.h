#pragma once
#include "VulkanHelperTypes.h"

namespace BB
{
	BackendInfo VulkanCreateBackend(const RenderBackendCreateInfo& a_CreateInfo);
	RDescriptorHandle VulkanCreateDescriptor(const RenderDescriptorCreateInfo& a_Info);
	CommandQueueHandle VulkanCreateCommandQueue(const RenderCommandQueueCreateInfo& a_Info);
	CommandAllocatorHandle VulkanCreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
	CommandListHandle VulkanCreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo);
	RBufferHandle VulkanCreateBuffer(const RenderBufferCreateInfo& a_Info);
	RImageHandle VulkanCreateImage(const RenderImageCreateInfo& a_CreateInfo);
	RSamplerHandle VulkanCreateSampler(const SamplerCreateInfo& a_Info);
	RFenceHandle VulkanCreateFence(const FenceCreateInfo& a_Info);

	void VulkanUpdateDescriptorBuffer(const UpdateDescriptorBufferInfo& a_Info);
	void VulkanUpdateDescriptorImage(const UpdateDescriptorImageInfo& a_Info);
	void VulkanUpdateDescriptorBuffers(const Slice<UpdateDescriptorBufferInfo> a_Info);
	void VulkanUpdateDescriptorImages(const Slice<UpdateDescriptorImageInfo> a_Info);
	ImageReturnInfo VulkanGetImageInfo(const RImageHandle a_Handle);

	PipelineBuilderHandle VulkanPipelineBuilderInit(const PipelineInitInfo& t_InitInfo);
	void VulkanPipelineBuilderBindDescriptor(const PipelineBuilderHandle a_Handle, const RDescriptorHandle a_Descriptor);
	void VulkanPipelineBuilderBindShaders(const PipelineBuilderHandle a_Handle, const Slice<BB::ShaderCreateInfo> a_ShaderInfo);
	void VulkanPipelineBuilderBindAttributes(const PipelineBuilderHandle a_Handle, const PipelineAttributes& a_AttributeInfo);
	PipelineHandle VulkanPipelineBuildPipeline(const PipelineBuilderHandle a_Handle);
	
	void VulkanResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle);

	RecordingCommandListHandle VulkanStartCommandList(const CommandListHandle a_CmdHandle);
	void VulkanEndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
	void VulkanStartRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_RenderInfo);
	void VulkanSetScissor(const RecordingCommandListHandle a_RecordingCmdHandle, const ScissorInfo& a_ScissorInfo);
	void VulkanEndRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo);

	void VulkanCopyBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferInfo& a_CopyInfo);
	void VulkanCopyBufferImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferImageInfo& a_CopyInfo);
	void VulkanTransitionImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderTransitionImageInfo& a_TransitionInfo);

	void VulkanBindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
	void VulkanBindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
	void VulkanBindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
	void VulkanBindDescriptors(const RecordingCommandListHandle a_RecordingCmdHandle, const RDescriptorHandle* a_Sets, const uint32_t a_SetCount, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets);
	void VulkanBindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_DwordOffset, const void* a_Data);

	void VulkanDrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
	void VulkanDrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);

	void VulkanBufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);
	void* VulkanMapMemory(const RBufferHandle a_Handle);
	void VulkanUnMemory(const RBufferHandle a_Handle);
	
	void VulkanStartFrame(const StartFrameInfo& a_StartInfo);
	void VulkanExecuteCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount);
	//Special execute commands that also signals the binary semaphore for image presentation
	void VulkanExecutePresentCommand(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo);
	FrameIndex VulkanPresentFrame(const PresentFrameInfo& a_PresentInfo);

	uint64_t VulkanNextQueueFenceValue(const CommandQueueHandle a_Handle);
	uint64_t VulkanNextFenceValue(const RFenceHandle a_Handle);

	void VulkanResizeWindow(const uint32_t a_X, const uint32_t a_Y);

	void VulkanWaitCommands(const RenderWaitCommandsInfo& a_WaitInfo);

	void VulkanDestroyFence(const RFenceHandle a_Handle);
	void VulkanDestroySampler(const RSamplerHandle a_Handle);
	void VulkanDestroyImage(const RImageHandle a_Handle);
	void VulkanDestroyBuffer(const RBufferHandle a_Handle);
	void VulkanDestroyCommandQueue(const CommandQueueHandle a_Handle);
	void VulkanDestroyCommandAllocator(const CommandAllocatorHandle a_Handle);
	void VulkanDestroyCommandList(const CommandListHandle a_Handle);
	void VulkanDestroyDescriptor(const RDescriptorHandle a_Handle);
	void VulkanDestroyPipeline(const PipelineHandle a_Handle);
	void VulkanDestroyBackend();
}
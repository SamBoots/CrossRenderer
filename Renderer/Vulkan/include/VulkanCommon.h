#pragma once
#include "VulkanHelperTypes.h"

namespace BB
{
	//Functions
	BackendInfo VulkanCreateBackend(Allocator a_TempAllocator,const RenderBackendCreateInfo& a_CreateInfo);
	FrameBufferHandle VulkanCreateFrameBuffer(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo);
	CommandQueueHandle VulkanCreateCommandQueue(const RenderCommandQueueCreateInfo& a_Info);
	CommandAllocatorHandle VulkanCreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
	CommandListHandle VulkanCreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo);
	RBufferHandle VulkanCreateBuffer(const RenderBufferCreateInfo& a_Info);
	RFenceHandle VulkanCreateFence(const FenceCreateInfo& a_Info);

	//PipelineBuilder
	PipelineBuilderHandle VulkanPipelineBuilderInit(const FrameBufferHandle a_Handle);
	void VulkanPipelineBuilderBindConstants(const PipelineBuilderHandle a_Handle, const BB::Slice<ConstantBind> a_ConstantBinds);
	void VulkanPipelineBuilderBindBuffers(const PipelineBuilderHandle a_Handle, const BB::Slice<BufferBind> a_BufferBinds);
	void VulkanPipelineBuilderBindShaders(const PipelineBuilderHandle a_Handle, const Slice<BB::ShaderCreateInfo> a_ShaderInfo);
	PipelineHandle VulkanPipelineBuildPipeline(const PipelineBuilderHandle a_Handle);
	
	void VulkanResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle);

	RecordingCommandListHandle VulkanStartCommandList(const CommandListHandle a_CmdHandle);
	void VulkanEndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
	void VulkanStartRenderPass(const RecordingCommandListHandle a_RecordingCmdHandle, const FrameBufferHandle a_Framebuffer);
	void VulkanEndRenderPass(const RecordingCommandListHandle a_RecordingCmdHandle);
	void VulkanBindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets);
	void VulkanBindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
	void VulkanBindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
	//void VulkanBindDescriptorSets(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_FirstSet, const uint32_t a_SetCount, const RDescriptorHandle* a_Sets, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets);
	void VulkanBindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const RENDER_SHADER_STAGE a_Stage, const uint32_t a_Offset, const uint32_t a_Size, const void* a_Data);

	void VulkanDrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
	void VulkanDrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);

	void VulkanBufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);
	void VulkanCopyBuffer(Allocator a_TempAllocator, const RenderCopyBufferInfo& a_CopyInfo);
	void* VulkanMapMemory(const RBufferHandle a_Handle);
	void VulkanUnMemory(const RBufferHandle a_Handle);

	void VulkanResizeWindow(Allocator a_TempAllocator, const uint32_t a_X, const uint32_t a_Y);
	
	void VulkanStartFrame(Allocator a_TempAllocator, const StartFrameInfo& a_StartInfo);
	void VulkanExecuteCommands(Allocator a_TempAllocator, CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount);
	//Special execute commands that also signals the binary semaphore for image presentation
	void VulkanExecutePresentCommand(Allocator a_TempAllocator, CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo);
	FrameIndex VulkanPresentFrame(Allocator a_TempAllocator, const PresentFrameInfo& a_PresentInfo);

	uint64_t VulkanNextQueueFenceValue(const CommandQueueHandle a_Handle);
	uint64_t VulkanNextFenceValue(const RFenceHandle a_Handle);

	void VulkanWaitDeviceReady();

	void VulkanDestroyFence(const RFenceHandle a_Handle);
	void VulkanDestroyBuffer(const RBufferHandle a_Handle);
	void VulkanDestroyCommandQueue(const CommandQueueHandle a_Handle);
	void VulkanDestroyCommandAllocator(const CommandAllocatorHandle a_Handle);
	void VulkanDestroyCommandList(const CommandListHandle a_Handle);
	void VulkanDestroyFramebuffer(const FrameBufferHandle a_Handle);
	void VulkanDestroyPipeline(const PipelineHandle a_Handle);
	void VulkanDestroyBackend();
}
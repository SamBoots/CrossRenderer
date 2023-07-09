#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "AgilitySDK/include/d3dx12/d3dx12.h"
#include <dxgi1_6.h>

#include "RenderBackendCommon.h"

namespace BB
{
	BackendInfo DX12CreateBackend(const RenderBackendCreateInfo& a_CreateInfo);
	RDescriptorHeap DX12CreateDescriptorHeap(const DescriptorHeapCreateInfo& a_CreateInfo, const bool a_GpuVisible);
	RDescriptor DX12CreateDescriptor(const RenderDescriptorCreateInfo& a_CreateInfo);
	CommandQueueHandle DX12CreateCommandQueue(const RenderCommandQueueCreateInfo& a_CreateInfo);
	CommandAllocatorHandle DX12CreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
	CommandListHandle DX12CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo);
	RBufferHandle DX12CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo);
	RImageHandle DX12CreateImage(const RenderImageCreateInfo& a_CreateInfo);
	RSamplerHandle DX12CreateSampler(const SamplerCreateInfo& a_CreateInfo);
	RFenceHandle DX12CreateFence(const FenceCreateInfo& a_CreateInfo);

	DescriptorAllocation DX12AllocateDescriptor(const AllocateDescriptorInfo& a_AllocateInfo);
	void DX12CopyDescriptors(const CopyDescriptorsInfo& a_CopyInfo);
	void DX12WriteDescriptors(const WriteDescriptorInfos& a_WriteInfo);
	ImageReturnInfo DX12GetImageInfo(const RImageHandle a_Handle);

	//PipelineBuilder
	PipelineBuilderHandle DX12PipelineBuilderInit(const PipelineInitInfo& t_InitInfo);
	void DX12PipelineBuilderBindDescriptor(const PipelineBuilderHandle a_Handle, const RDescriptor a_Descriptor);
	void DX12PipelineBuilderBindShaders(const PipelineBuilderHandle a_Handle, const Slice<BB::ShaderCreateInfo> a_ShaderInfo);
	void DX12PipelineBuilderBindAttributes(const PipelineBuilderHandle a_Handle, const PipelineAttributes& a_AttributeInfo);
	PipelineHandle DX12PipelineBuildPipeline(const PipelineBuilderHandle a_Handle);

	void DX12ResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle);

	RecordingCommandListHandle DX12StartCommandList(const CommandListHandle a_CmdHandle);
	void DX12EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
	void DX12StartRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_RenderInfo);
	void DX12SetScissor(const RecordingCommandListHandle a_RecordingCmdHandle, const ScissorInfo& a_ScissorInfo);
	void DX12EndRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo);
	
	void DX12CopyBuffer(const RecordingCommandListHandle transferCommandHandle, const RenderCopyBufferInfo& a_CopyInfo);
	void DX12CopyBufferImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferImageInfo& a_CopyInfo);
	void DX12TransitionImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderTransitionImageInfo& a_TransitionInfo);

	void DX12BindDescriptorHeaps(const RecordingCommandListHandle a_RecordingCmdHandle, const RDescriptorHeap a_ResourceHeap, const RDescriptorHeap a_SamplerHeap);
	void DX12BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
	void DX12SetDescriptorHeapOffsets(const RecordingCommandListHandle a_RecordingCmdHandle, const RENDER_DESCRIPTOR_SET a_FirstSet, const uint32_t a_SetCount, const uint32_t* a_HeapIndex, const size_t* a_Offsets);
	void DX12BindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
	void DX12BindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
	void DX12BindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_DwordOffset, const void* a_Data);

	void DX12DrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
	void DX12DrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);
	
	void DX12BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);
	void* DX12MapMemory(const RBufferHandle a_Handle);
	void DX12UnMemory(const RBufferHandle a_Handle);

	void DX12StartFrame(const StartFrameInfo& a_StartInfo);
	void DX12ExecuteCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount);
	//Special execute commands that also signals the binary semaphore for image presentation
	void DX12ExecutePresentCommand(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo);
	FrameIndex DX12PresentFrame(const PresentFrameInfo& a_PresentInfo);

	uint64_t DX12NextQueueFenceValue(const CommandQueueHandle a_Handle);
	uint64_t DX12NextFenceValue(const RFenceHandle a_Handle);

	void DX12WaitCommands(const RenderWaitCommandsInfo& a_WaitInfo);

	void DX12DestroyFence(const RFenceHandle a_Handle);
	void DX12DestroySampler(const RSamplerHandle a_Handle);
	void DX12DestroyImage(const RImageHandle a_Handle);
	void DX12DestroyBuffer(const RBufferHandle a_Handle);
	void DX12DestroyCommandList(const CommandListHandle a_Handle);
	void DX12DestroyCommandAllocator(const CommandAllocatorHandle a_Handle);
	void DX12DestroyCommandQueue(const CommandQueueHandle a_Handle);
	void DX12DestroyPipeline(const PipelineHandle a_Handle);
	void DX12DestroyDescriptor(const RDescriptor a_Handle);
	void DX12DestroyDescriptorHeap(const RDescriptorHeap a_Handle);
	void DX12DestroyBackend();
}
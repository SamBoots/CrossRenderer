#pragma once
#include "TemporaryAllocator.h"
#include "RenderBackendCommon.h"

namespace BB
{
	namespace RenderBackend
	{
		const uint32_t GetFrameBufferAmount();
		const FrameIndex GetCurrentFrameBufferIndex();

		void InitBackend(const RenderBackendCreateInfo& a_CreateInfo);
		RDescriptorHandle CreateDescriptor(const RenderDescriptorCreateInfo& a_CreateInfo);
		FrameBufferHandle CreateFrameBuffer(const RenderFrameBufferCreateInfo& a_CreateInfo);
		PipelineHandle CreatePipeline(const RenderPipelineCreateInfo& a_CreateInfo);
		CommandQueueHandle CreateCommandQueue(const RenderCommandQueueCreateInfo& a_CreateInfo);
		CommandAllocatorHandle CreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
		CommandListHandle CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo);
		RBufferHandle CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo);
		RFenceHandle CreateFence(const FenceCreateInfo& a_Info);
		
		void BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);
		void CopyBuffer(const RenderCopyBufferInfo& a_CopyInfo);
		void* MapMemory(const RBufferHandle a_Handle);
		void UnmapMemory(const RBufferHandle a_Handle);

		RecordingCommandListHandle StartCommandList(const CommandListHandle a_CmdHandle);
		void ResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle);
		void EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
		void StartRenderPass(const RecordingCommandListHandle a_RecordingCmdHandle, const FrameBufferHandle a_Framebuffer);
		void EndRenderPass(const RecordingCommandListHandle a_RecordingCmdHandle);
		void BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
		void BindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
		void BindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
		void BindDescriptorSets(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_FirstSet, const uint32_t a_SetCount, const RDescriptorHandle* a_Sets, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets);
		void BindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const RENDER_SHADER_STAGE a_Stage, const uint32_t a_Offset, const uint32_t a_Size, const void* a_Data);

		void DrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
		void DrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);

		void StartFrame(const StartFrameInfo& a_StartInfo);
		void ExecuteCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount);
		void ExecutePresentCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo);
		FrameIndex PresentFrame(const PresentFrameInfo& a_PresentInfo);

		void Update();
		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);

		uint64_t NextQueueFenceValue(const CommandQueueHandle a_Handle);
		uint64_t NextFenceValue(const RFenceHandle a_Handle);

		void WaitGPUReady();

		void DestroyBackend();
		void DestroyDescriptorSet(const RDescriptorHandle a_Handle);
		void DestroyFrameBuffer(const FrameBufferHandle a_Handle);
		void DestroyPipeline(const PipelineHandle a_Handle);
		void DestroyCommandQueue(const CommandQueueHandle a_Handle);
		void DestroyCommandAllocator(const CommandAllocatorHandle a_Handle);
		void DestroyCommandList(const CommandListHandle a_Handle);
		void DestroyBuffer(const RBufferHandle a_Handle);
		void DestroyFence(const RFenceHandle a_Handle);
	};
}
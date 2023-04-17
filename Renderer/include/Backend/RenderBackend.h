#pragma once
#include "TemporaryAllocator.h"
#include "RenderBackendCommon.h"

namespace BB
{
	class PipelineBuilder
	{
	public:
		PipelineBuilder(const PipelineInitInfo& a_InitInfo);
		~PipelineBuilder();
		
		void BindDescriptor(const RDescriptorHandle a_Handle);
		void BindShaders(const Slice<BB::ShaderCreateInfo> a_ShaderInfo);
		void BindAttributes(const PipelineAttributes& a_AttributeInfo);
		PipelineHandle BuildPipeline();

	private:
		PipelineBuilderHandle m_BuilderHandle;
	};

	struct UploadBufferChunk
	{
		void* memory;
		uint64_t offset;
	};

	class UploadBuffer
	{
	public:
		UploadBuffer(const uint64_t a_Size);
		~UploadBuffer();

		UploadBufferChunk  Alloc(const uint64_t a_Size);
		const uint64_t GetCurrentOffset() const { return m_Offset; }
		void Clear();

		const RBufferHandle Buffer() const { return m_Buffer; }

	private:
		RBufferHandle m_Buffer;
		const uint64_t m_Size;
		uint64_t m_Offset;
		void* m_Start;
	};

	namespace RenderBackend
	{
		const uint32_t GetFrameBufferAmount();
		const FrameIndex GetCurrentFrameBufferIndex();

		void InitBackend(const RenderBackendCreateInfo& a_CreateInfo);
		RDescriptorHandle CreateDescriptor(const RenderDescriptorCreateInfo& a_Info);
		CommandQueueHandle CreateCommandQueue(const RenderCommandQueueCreateInfo& a_CreateInfo);
		CommandAllocatorHandle CreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
		CommandListHandle CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo);
		RBufferHandle CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo);
		RImageHandle CreateImage(const RenderImageCreateInfo& a_CreateInfo);
		RFenceHandle CreateFence(const FenceCreateInfo& a_Info);

		void UpdateDescriptorBuffer(const UpdateDescriptorBufferInfo& a_Info);
		void UpdateDescriptorImage(const UpdateDescriptorImageInfo& a_Info);

		ImageReturnInfo GetImageInfo(const RImageHandle a_Handle);

		void ResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle);

		RecordingCommandListHandle StartCommandList(const CommandListHandle a_CmdHandle);
		void EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
		void StartRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_StartInfo);
		void SetScissor(const RecordingCommandListHandle a_RecordingCmdHandle, const ScissorInfo& a_ScissorInfo);
		void EndRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo);
		
		void CopyBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferInfo& a_CopyInfo);
		void CopyBufferImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferImageInfo& a_CopyInfo);
		void TransitionImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderTransitionImageInfo a_TransitionInfo);

		void BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
		void BindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
		void BindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
		void BindDescriptors(const RecordingCommandListHandle a_RecordingCmdHandle, const RDescriptorHandle* a_Sets, const uint32_t a_SetCount, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets);
		void BindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_Offset, const void* a_Data);

		void DrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
		void DrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);

		void BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);
		void* MapMemory(const RBufferHandle a_Handle);
		void UnmapMemory(const RBufferHandle a_Handle);

		void StartFrame(const StartFrameInfo& a_StartInfo);
		void ExecuteCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount);
		void ExecutePresentCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo);
		FrameIndex PresentFrame(const PresentFrameInfo& a_PresentInfo);

		uint64_t NextQueueFenceValue(const CommandQueueHandle a_Handle);
		uint64_t NextFenceValue(const RFenceHandle a_Handle);

		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);

		void WaitGPUReady();

		void DestroyBackend();
		void DestroyDescriptor(const RDescriptorHandle a_Handle);
		void DestroyPipeline(const PipelineHandle a_Handle);
		void DestroyCommandQueue(const CommandQueueHandle a_Handle);
		void DestroyCommandAllocator(const CommandAllocatorHandle a_Handle);
		void DestroyCommandList(const CommandListHandle a_Handle);
		void DestroyBuffer(const RBufferHandle a_Handle);
		void DestroyImage(const RImageHandle a_Handle);
		void DestroyFence(const RFenceHandle a_Handle);
	};
}
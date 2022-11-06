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
		RDescriptorHandle* CreateDescriptors(Allocator a_SysAllocator, RDescriptorLayoutHandle& a_Layout, const RenderDescriptorCreateInfo& a_CreateInfo);
		FrameBufferHandle CreateFrameBuffer(const RenderFrameBufferCreateInfo& a_CreateInfo);
		PipelineHandle CreatePipeline(const RenderPipelineCreateInfo& a_CreateInfo);
		CommandListHandle CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo);
		
		RBufferHandle CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo);
		void BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);
		void CopyBuffer(const RenderCopyBufferInfo& a_CopyInfo);
		
		RecordingCommandListHandle StartCommandList(const CommandListHandle a_CmdHandle, const FrameBufferHandle a_FrameHandle);
		void ResetCommandList(const CommandListHandle a_CmdHandle);
		void EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
		void BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
		void BindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
		void BindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
		void BindDescriptorSets(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_FirstSet, const uint32_t a_SetCount, const RDescriptorHandle* a_Sets, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets);

		void DrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
		void DrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);

		FrameIndex StartFrame();
		void RenderFrame(const CommandListHandle a_CommandHandle, const FrameBufferHandle a_FrameBufferHandle, const PipelineHandle a_PipeHandle);

		void Update();
		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);

		void WaitGPUReady();

		void CreateShader(const ShaderCreateInfo& t_ShaderInfo);

		void DestroyBackend();
		void DestroyDescriptorSetLayout(const RDescriptorLayoutHandle a_Handle);
		void DestroyDescriptorSet(const RDescriptorHandle a_Handle);
		void DestroyFrameBuffer(const FrameBufferHandle a_Handle);
		void DestroyPipeline(const PipelineHandle a_Handle);
		void DestroyCommandList(const CommandListHandle a_Handle);
		void DestroyBuffer(const RBufferHandle a_Handle);
	};
}
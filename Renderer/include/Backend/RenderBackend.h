#pragma once
#include "TemporaryAllocator.h"
#include "RenderBackendCommon.h"

namespace BB
{
	namespace RenderBackend
	{
		void InitBackend(const RenderBackendCreateInfo& a_CreateInfo);
		FrameBufferHandle CreateFrameBuffer(const RenderFrameBufferCreateInfo& a_CreateInfo);
		PipelineHandle CreatePipeline(const RenderPipelineCreateInfo& a_CreateInfo);
		CommandListHandle CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo);
		
		RBufferHandle CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo);
		void BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);
		void CopyBuffer(const RenderCopyBufferInfo& a_CopyInfo);
		
		RecordingCommandListHandle StartCommandList(const CommandListHandle a_CmdHandle, const FrameBufferHandle a_FrameHandle);
		void EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
		void BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
		void DrawBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_BufferHandles, const size_t a_BufferCount);

		void StartFrame();
		void RenderFrame(const CommandListHandle a_CommandHandle, const FrameBufferHandle a_FrameBufferHandle, const PipelineHandle a_PipeHandle);

		void Update();
		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);

		void WaitGPUReady();

		void CreateShader(const ShaderCreateInfo& t_ShaderInfo);

		void DestroyBackend();
		void DestroyFrameBuffer(const FrameBufferHandle a_Handle);
		void DestroyPipeline(const PipelineHandle a_Handle);
		void DestroyCommandList(const CommandListHandle a_Handle);
		void DestroyBuffer(const RBufferHandle a_Handle);
	};
}
#pragma once
#include "TemporaryAllocator.h"
#include "RenderBackendCommon.h"

namespace BB
{
	namespace RenderBackend
	{
		void InitBackend(const RenderBackendCreateInfo& a_CreateInfo);
		void DestroyBackend();
		
		FrameBufferHandle CreateFrameBuffer(const RenderFrameBufferCreateInfo& a_CreateInfo);
		PipelineHandle CreatePipeline(const RenderPipelineCreateInfo& a_CreateInfo);
		CommandListHandle CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo);
		RBufferHandle CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo);

		RecordingCommandListHandle StartCommandList(const CommandListHandle a_CmdHandle);
		void EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
		void DrawBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, RBufferHandle* a_BufferHandles, const size_t a_BufferCount);

		void Update();
		void ResizeWindow(uint32_t a_X, uint32_t a_Y);

		void CreateShader(const ShaderCreateInfo& t_ShaderInfo);
	};
}
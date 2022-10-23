#pragma once
#include "RenderFrontendCommon.h"

namespace BB
{
	namespace Render
	{
		void InitRenderer(const WindowHandle a_WindowHandle, const LibHandle a_RenderLib, const bool a_Debug);
		void DestroyRenderer();

		RModelHandle CreateRawModel(const CreateRawModelInfo& a_CreateInfo);


		RecordingCommandListHandle StartRecordCmds();
		void EndRecordCmds(const RecordingCommandListHandle a_Handle);
		void DrawModel(const RecordingCommandListHandle a_Handle, const RModelHandle a_ModelHandle);

		void Update();

		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);
	};
}
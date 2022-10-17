#pragma once
#include "RenderFrontendCommon.h"

namespace BB
{
	namespace Render
	{
		void InitRenderer(const WindowHandle a_WindowHandle, const RenderAPI a_RenderAPI, const bool a_Debug);
		void DestroyRenderer();

		RModelHandle CreateRawModel(const CreateRawModelInfo& a_CreateInfo);


		void StartRecordCmds();
		void EndRecordCmds();
		void DrawModel(const RModelHandle a_ModelHandle);

		void Update();

		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);
	};
}
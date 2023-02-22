#pragma once
#include "RenderFrontendCommon.h"

namespace BB
{
	namespace Render
	{
		void InitRenderer(const RenderInitInfo& a_InitInfo);
		void DestroyRenderer();

		void Update(const float a_DeltaTime);
		void SetProjection(const glm::mat4& a_Proj);
		void SetView(const glm::mat4& a_View);
		void* GetMatrixBufferSpace(uint32_t& a_MatrixSpace);

		RModelHandle CreateRawModel(const CreateRawModelInfo& a_CreateInfo);
		RModelHandle LoadModel(const LoadModelInfo& a_LoadInfo);
		DrawObjectHandle CreateDrawObject(const RModelHandle a_Model, const TransformHandle a_TransformHandle);
		void DestroyDrawObject(const DrawObjectHandle a_Handle);

		void StartFrame();
		void EndFrame();

		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);
	};
}
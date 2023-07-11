#pragma once
#include "RenderFrontendCommon.h"
#include "Slice.h"

namespace BB
{
	struct Light;
	struct DrawObject;

	namespace Render
	{
		void InitRenderer(const RenderInitInfo& a_InitInfo);
		void DestroyRenderer();

		RDescriptorHeap GetGPUHeap(const uint32_t a_FrameNum);
		DescriptorAllocation AllocateDescriptor(const RDescriptor a_Descriptor);
		void UploadDescriptorsToGPU(const uint32_t a_FrameNum);
		RenderBufferPart AllocateFromVertexBuffer(const size_t a_Size);
		RenderBufferPart AllocateFromIndexBuffer(const size_t a_Size);

		void SetProjection(const glm::mat4& a_Proj);
		void SetView(const glm::mat4& a_View);
		void* GetMatrixBufferSpace(uint32_t& a_MatrixSpace);
		
		RModelHandle CreateRawModel(const CreateRawModelInfo& a_CreateInfo);
		RModelHandle LoadModel(const LoadModelInfo& a_LoadInfo);

		void StartFrame();
		void Update(const float a_DeltaTime);
		void EndFrame();

		const LightHandle AddLights(const BB::Slice<Light> a_Lights, const LIGHT_TYPE a_LightType);

		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);
	};
}
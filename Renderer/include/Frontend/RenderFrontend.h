#pragma once
#include "RenderFrontendCommon.h"
#include "Slice.h"

namespace BB
{
	constexpr const size_t MAX_TEXTURES = 1024;

	struct Light;
	struct DrawObject;

	struct Render_IO
	{
		uint32_t swapchainWidth = 0;
		uint32_t swapchainHeight = 0;
		uint32_t frameBufferAmount = 0;
		RENDER_API renderAPI = RENDER_API::NONE;

		RDescriptor globalDescriptor;
		DescriptorAllocation globalDescAllocation;
	};

	namespace Render
	{
		Render_IO& GetIO();

		void InitRenderer(const RenderInitInfo& a_InitInfo);
		void DestroyRenderer();

		RDescriptorHeap GetGPUHeap(const uint32_t a_FrameNum);
		DescriptorAllocation AllocateDescriptor(const RDescriptor a_Descriptor);
		void UploadDescriptorsToGPU(const uint32_t a_FrameNum);
		RenderBufferPart AllocateFromVertexBuffer(const size_t a_Size);
		RenderBufferPart AllocateFromIndexBuffer(const size_t a_Size);


		void FreeTextures(const uint32_t* a_TextureIndices, const uint32_t a_Count);
		
		const RDescriptor GetGlobalDescriptorSet();

		RenderQueue& GetGraphicsQueue();
		RenderQueue& GetComputeQueue();
		RenderQueue& GetTransferQueue();

		Model& GetModel(const RModelHandle a_Handle);
		RModelHandle CreateRawModel(const CreateRawModelInfo& a_CreateInfo);
		RModelHandle LoadModel(const LoadModelInfo& a_LoadInfo);

		void StartFrame();
		RecordingCommandListHandle GetRecordingGraphics();
		RecordingCommandListHandle GetRecordingTransfer();
		void Update(const float a_DeltaTime);
		void EndFrame();

		const LightHandle AddLights(const BB::Slice<Light> a_Lights, const LIGHT_TYPE a_LightType);

		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);
	};
}
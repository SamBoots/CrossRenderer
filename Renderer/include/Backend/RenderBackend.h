#pragma once
#include "TemporaryAllocator.h"
#include "RenderBackendCommon.h"

namespace BB
{
	struct ShaderCreateInfo;

	enum class RenderAPI
	{
		VULKAN,
		DX12
	};

	class RenderBackend
	{
	public:
		void InitBackend(WindowHandle a_WindowHandle, RenderAPI a_RenderAPI, bool a_Debug);
		void DestroyBackend();

		void Update();
		void ResizeWindow(uint32_t a_X, uint32_t a_Y);

		void CreateShader(const ShaderCreateInfo& t_ShaderInfo);

	private:
		void SetFunctions(RenderAPI a_RenderAPI);

		FreelistAllocator_t m_SystemAllocator{ mbSize * 4 };
		TemporaryAllocator m_TempAllocator{ m_SystemAllocator };

		APIRenderBackend m_APIbackend;

		//Functions
		PFN_RenderAPICreateBuffer pfn_CreateBuffer;
		PFN_RenderAPIDestroyBuffer pfn_DestroyBuffer;
		PFN_RenderAPIBuffer_CopyData pfn_BufferCopyData;

		PFN_RenderAPICreateBackend pfn_CreateBackend;
		PFN_RenderAPICreateFrameBuffer pfn_CreateFrameBuffer;
		PFN_RenderAPICreatePipeline pfn_CreatePipelineFunc;
		PFN_RenderAPICreateCommandList pfn_CreateCommandList;

		PFN_RenderAPIResizeWindow pfn_ResizeWindow;
		PFN_RenderAPIRenderFrame pfn_RenderFrame;
		PFN_RenderAPIWaitDeviceReady pfn_WaitDeviceReady;

		PFN_RenderAPIDestroyBackend pfn_DestroyBackend;
		PFN_RenderAPIDestroyFrameBuffer pfn_DestroyFrameBuffer;
		PFN_RenderAPIDestroyPipeline pfn_DestroyPipeline;
		PFN_RenderAPIDestroyCommandList pfn_DestroyCommandList;
	};
}
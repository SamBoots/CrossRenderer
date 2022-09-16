#pragma once
#include "AllocTypes.h"
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

		void CreateShader(const ShaderCreateInfo& t_ShaderInfo);

	private:
		void SetFunctions(RenderAPI a_RenderAPI);

		FreeListAllocator_t m_SystemAllocator{ mbSize * 4 };
		LinearAllocator_t m_TempAllocator{ kbSize * 4 };

		APIRenderBackend m_APIbackend;

		//Functions
		PFN_RenderAPICreateBackend pfn_CreateBackend;
		PFN_RenderAPICreateFrameBuffer pfn_CreateFrameBuffer;
		PFN_RenderAPICreatePipeline pfn_CreatePipelineFunc;
		PFN_RenderAPICreateCommandList pfn_CreateCommandList;

		PFN_RenderAPIRenderFrame pfn_RenderFrame;
		PFN_RenderAPIWaitDeviceReady pfn_WaitDeviceReady;

		PFN_RenderAPIDestroyBackend pfn_DestroyBackend;
		PFN_RenderAPIDestroyFrameBuffer pfn_DestroyFrameBuffer;
		PFN_RenderAPIDestroyPipeline pfn_DestroyPipeline;
		PFN_RenderAPIDestroyCommandList pfn_DestroyCommandList;
	};
}
#pragma once
#include "AllocTypes.h"
#include "Common.h"

namespace BB
{
	struct ShaderCreateInfo;

	enum class RenderAPI
	{
		VULKAN
	};

	class RenderBackend
	{
	public:
		void InitBackend(WindowHandle a_WindowHandle, RenderAPI a_RenderAPI, bool a_Debug);
		void DestroyBackend();

		void Update();

		void CreateShader(const ShaderCreateInfo& t_ShaderInfo);

	private:
		FreeListAllocator_t m_SystemAllocator{ mbSize * 4 };
		FreeListAllocator_t m_TempAllocator{ kbSize * 4 };

		void* APIbackend;
		RenderAPI currentRenderAPI;
	};
}
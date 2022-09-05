#pragma once
#include "AllocTypes.h"
#include "Common.h"

enum class RenderAPI
{
	VULKAN
};

class RenderBackend
{
public:
	void InitBackend(BB::WindowHandle& a_WindowHandle, RenderAPI a_RenderAPI, bool a_Debug);
	void DestroyBackend();

	void Update();

private:
	BB::FreeListAllocator_t m_SystemAllocator{ BB::mbSize * 4 };
	BB::FreeListAllocator_t m_TempAllocator{ BB::kbSize * 4 };

	void* APIbackend;
	RenderAPI currentRenderAPI;
};
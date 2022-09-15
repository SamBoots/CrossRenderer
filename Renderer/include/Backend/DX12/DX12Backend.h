#pragma once
#include "AllocTypes.h"
#include "BackendCommands.h"

namespace BB
{
	APIRenderBackend DX12CreateBackend(Allocator a_SysAllocator,
		Allocator a_TempAllocator,
		const RenderBackendCreateInfo& a_CreateInfo);
}
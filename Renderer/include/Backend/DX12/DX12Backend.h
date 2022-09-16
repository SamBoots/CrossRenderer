#pragma once
#include "AllocTypes.h"
#include "RenderBackendCommon.h"

namespace BB
{
	APIRenderBackendHandle DX12CreateBackend(Allocator a_SysAllocator,
		Allocator a_TempAllocator,
		const RenderBackendCreateInfo& a_CreateInfo);
}
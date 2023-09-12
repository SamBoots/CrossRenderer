#pragma once
#include "RenderBackendCommon.h"

namespace BB
{
	//Get the functions needed to run the Vulkan Renderer.
	extern "C" void GetDirectX12APIFunctions(RenderAPIFunctions & a_FuncCreateInfo);
}
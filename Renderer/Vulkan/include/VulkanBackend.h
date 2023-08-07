#pragma once
#include "RenderBackendCommon.h"

namespace BB
{
	//Get the functions needed to run the Vulkan Renderer.
	extern "C" void GetRenderAPIFunctions(RenderAPIFunctions & a_FuncCreateInfo);
}
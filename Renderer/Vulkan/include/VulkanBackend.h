#pragma once
#include "RenderBackendCommon.h"

namespace BB
{
	//Get the functions needed to run the Vulkan Renderer.
	extern "C" void GetVulkanAPIFunctions(RenderAPIFunctions & a_FuncCreateInfo);
}
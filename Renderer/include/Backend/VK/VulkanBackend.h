#pragma once
#include "RenderBackendCommon.h"

namespace BB
{
	//Get the functions needed to run the Vulkan Renderer.
	void GetVulkanAPIFunctions(RenderAPIFunctions& a_FuncCreateInfo);
}
#pragma once
#include "BackendCommands.h"

namespace BB
{
	//Get the functions needed to run the Vulkan Renderer.
	void GetVulkanAPIFunctions(APIBackendFunctionPointersCreateInfo& a_FuncCreateInfo);
}
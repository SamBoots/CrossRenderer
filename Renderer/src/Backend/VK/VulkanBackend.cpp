#pragma once
#include "VulkanBackend.h"
#include "VulkanCommon.h"

using namespace BB;

void BB::GetVulkanAPIFunctions(APIBackendFunctionPointersCreateInfo& a_FuncCreateInfo)
{
	*a_FuncCreateInfo.createBackend = VulkanCreateBackend;
	*a_FuncCreateInfo.createPipeline = VulkanCreatePipeline;
	*a_FuncCreateInfo.createFrameBuffer = VulkanCreateFrameBuffer;
	*a_FuncCreateInfo.createCommandList = VulkanCreateCommandList;
	*a_FuncCreateInfo.createBuffer = VulkanCreateBuffer;

	*a_FuncCreateInfo.bufferCopyData = VulkanBufferCopyData;

	*a_FuncCreateInfo.resizeWindow = ResizeWindow;
	*a_FuncCreateInfo.renderFrame = RenderFrame;
	*a_FuncCreateInfo.waitDevice = VulkanWaitDeviceReady;

	*a_FuncCreateInfo.destroyBackend = VulkanDestroyBackend;
	*a_FuncCreateInfo.destroyFrameBuffer = VulkanDestroyFramebuffer;
	*a_FuncCreateInfo.destroyPipeline = VulkanDestroyPipeline;
	*a_FuncCreateInfo.destroyCommandList = VulkanDestroyCommandList;
	*a_FuncCreateInfo.destroyBuffer = VulkanDestroyBuffer;
}
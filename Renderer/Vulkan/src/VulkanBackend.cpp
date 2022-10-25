#include "VulkanBackend.h"
#include "VulkanCommon.h"

using namespace BB;

void BB::GetRenderAPIFunctions(RenderAPIFunctions& a_FuncCreateInfo)
{
	a_FuncCreateInfo.createBackend = VulkanCreateBackend;
	a_FuncCreateInfo.createPipeline = VulkanCreatePipeline;
	a_FuncCreateInfo.createFrameBuffer = VulkanCreateFrameBuffer;
	a_FuncCreateInfo.createCommandList = VulkanCreateCommandList;
	a_FuncCreateInfo.createBuffer = VulkanCreateBuffer;

	a_FuncCreateInfo.startCommandList = VulkanStartCommandList;
	a_FuncCreateInfo.endCommandList = VulkanEndCommandList;
	a_FuncCreateInfo.bindPipeline = VulkanBindPipeline;
	a_FuncCreateInfo.drawBuffers = VulkanDrawBuffers;

	a_FuncCreateInfo.bufferCopyData = VulkanBufferCopyData;
	a_FuncCreateInfo.copyBuffer = VulkanCopyBuffer;

	a_FuncCreateInfo.resizeWindow = ResizeWindow;

	a_FuncCreateInfo.startFrame = StartFrame;
	a_FuncCreateInfo.renderFrame = RenderFrame;
	a_FuncCreateInfo.waitDevice = VulkanWaitDeviceReady;

	a_FuncCreateInfo.destroyBackend = VulkanDestroyBackend;
	a_FuncCreateInfo.destroyFrameBuffer = VulkanDestroyFramebuffer;
	a_FuncCreateInfo.destroyPipeline = VulkanDestroyPipeline;
	a_FuncCreateInfo.destroyCommandList = VulkanDestroyCommandList;
	a_FuncCreateInfo.destroyBuffer = VulkanDestroyBuffer;
}
#include "VulkanBackend.h"
#include "VulkanCommon.h"

using namespace BB;

void BB::GetRenderAPIFunctions(RenderAPIFunctions& a_FuncCreateInfo)
{
	a_FuncCreateInfo.createBackend = VulkanCreateBackend;
	a_FuncCreateInfo.createDescriptors = VulkanCreateDescriptors;
	a_FuncCreateInfo.createPipeline = VulkanCreatePipeline;
	a_FuncCreateInfo.createFrameBuffer = VulkanCreateFrameBuffer;
	a_FuncCreateInfo.createCommandList = VulkanCreateCommandList;
	a_FuncCreateInfo.createBuffer = VulkanCreateBuffer;

	a_FuncCreateInfo.startCommandList = VulkanStartCommandList;
	a_FuncCreateInfo.resetCommandList = VulkanResetCommandList;
	a_FuncCreateInfo.endCommandList = VulkanEndCommandList;
	a_FuncCreateInfo.bindPipeline = VulkanBindPipeline;
	a_FuncCreateInfo.bindVertBuffers = VulkanBindVertexBuffers;
	a_FuncCreateInfo.bindIndexBuffer = VulkanBindIndexBuffer;
	a_FuncCreateInfo.bindDescriptor = VulkanBindDescriptorSets;

	a_FuncCreateInfo.drawVertex = VulkanDrawVertex;
	a_FuncCreateInfo.drawIndex = VulkanDrawIndexed;

	a_FuncCreateInfo.bufferCopyData = VulkanBufferCopyData;
	a_FuncCreateInfo.copyBuffer = VulkanCopyBuffer;

	a_FuncCreateInfo.resizeWindow = ResizeWindow;

	a_FuncCreateInfo.startFrame = StartFrame;
	a_FuncCreateInfo.renderFrame = RenderFrame;
	a_FuncCreateInfo.waitDevice = VulkanWaitDeviceReady;

	a_FuncCreateInfo.destroyBackend = VulkanDestroyBackend;
	a_FuncCreateInfo.destroyDescriptorLayout = VulkanDestroyDescriptorSetLayout;
	a_FuncCreateInfo.destroyDescriptor = VulkanDestroyDescriptorSet;
	a_FuncCreateInfo.destroyFrameBuffer = VulkanDestroyFramebuffer;
	a_FuncCreateInfo.destroyPipeline = VulkanDestroyPipeline;
	a_FuncCreateInfo.destroyCommandList = VulkanDestroyCommandList;
	a_FuncCreateInfo.destroyBuffer = VulkanDestroyBuffer;
}
#include "VulkanBackend.h"
#include "VulkanCommon.h"

using namespace BB;

void BB::GetRenderAPIFunctions(RenderAPIFunctions& a_FuncCreateInfo)
{
	a_FuncCreateInfo.createBackend = VulkanCreateBackend;
	a_FuncCreateInfo.createDescriptor = VulkanCreateDescriptor;
	a_FuncCreateInfo.createPipeline = VulkanCreatePipeline;
	a_FuncCreateInfo.createFrameBuffer = VulkanCreateFrameBuffer;
	a_FuncCreateInfo.createCommandAllocator = VulkanCreateCommandAllocator;
	a_FuncCreateInfo.createCommandList = VulkanCreateCommandList;
	a_FuncCreateInfo.createBuffer = VulkanCreateBuffer;

	a_FuncCreateInfo.startCommandList = VulkanStartCommandList;
	a_FuncCreateInfo.resetCommandList = VulkanResetCommandList;
	a_FuncCreateInfo.endCommandList = VulkanEndCommandList;
	a_FuncCreateInfo.bindPipeline = VulkanBindPipeline;
	a_FuncCreateInfo.bindVertBuffers = VulkanBindVertexBuffers;
	a_FuncCreateInfo.bindIndexBuffer = VulkanBindIndexBuffer;
	a_FuncCreateInfo.bindDescriptor = VulkanBindDescriptorSets;
	a_FuncCreateInfo.bindConstant = VulkanBindConstant;

	a_FuncCreateInfo.drawVertex = VulkanDrawVertex;
	a_FuncCreateInfo.drawIndex = VulkanDrawIndexed;

	a_FuncCreateInfo.bufferCopyData = VulkanBufferCopyData;
	a_FuncCreateInfo.copyBuffer = VulkanCopyBuffer;
	a_FuncCreateInfo.mapMemory = VulkanMapMemory;
	a_FuncCreateInfo.unmapMemory = VulkanUnMemory;

	a_FuncCreateInfo.resizeWindow = VulkanResizeWindow;

	a_FuncCreateInfo.startFrame = VulkanStartFrame;
	a_FuncCreateInfo.executeCommands = VulkanExecuteCommands;
	a_FuncCreateInfo.waitDevice = VulkanWaitDeviceReady;

	a_FuncCreateInfo.destroyBackend = VulkanDestroyBackend;
	a_FuncCreateInfo.destroyDescriptorLayout = VulkanDestroyDescriptorSetLayout;
	a_FuncCreateInfo.destroyDescriptor = VulkanDestroyDescriptorSet;
	a_FuncCreateInfo.destroyFrameBuffer = VulkanDestroyFramebuffer;
	a_FuncCreateInfo.destroyPipeline = VulkanDestroyPipeline;
	a_FuncCreateInfo.destroyCommandAllocator = VulkanDestroyCommandAllocator;
	a_FuncCreateInfo.destroyCommandList = VulkanDestroyCommandList;
	a_FuncCreateInfo.destroyBuffer = VulkanDestroyBuffer;
}
#include "VulkanBackend.h"
#include "VulkanCommon.h"

using namespace BB;

void BB::GetRenderAPIFunctions(RenderAPIFunctions& a_FuncCreateInfo)
{
	a_FuncCreateInfo.createBackend = VulkanCreateBackend;
	a_FuncCreateInfo.createDescriptor = VulkanCreateDescriptor;
	a_FuncCreateInfo.createCommandQueue = VulkanCreateCommandQueue;
	a_FuncCreateInfo.createCommandAllocator = VulkanCreateCommandAllocator;
	a_FuncCreateInfo.createCommandList = VulkanCreateCommandList;
	a_FuncCreateInfo.createBuffer = VulkanCreateBuffer;
	a_FuncCreateInfo.createImage = VulkanCreateImage;
	a_FuncCreateInfo.createFence = VulkanCreateFence;

	a_FuncCreateInfo.updateDescriptorBuffer = VulkanUpdateDescriptorBuffer;
	a_FuncCreateInfo.updateDescriptorImage = VulkanUpdateDescriptorImage;

	a_FuncCreateInfo.pipelineBuilderInit = VulkanPipelineBuilderInit;
	a_FuncCreateInfo.pipelineBuilderBindDescriptor = VulkanPipelineBuilderBindDescriptor;
	a_FuncCreateInfo.pipelineBuilderBindShaders = VulkanPipelineBuilderBindShaders;
	a_FuncCreateInfo.pipelineBuilderBuildPipeline = VulkanPipelineBuildPipeline;

	a_FuncCreateInfo.resetCommandAllocator = VulkanResetCommandAllocator;

	a_FuncCreateInfo.startCommandList = VulkanStartCommandList;
	a_FuncCreateInfo.endCommandList = VulkanEndCommandList;
	a_FuncCreateInfo.startRendering = VulkanStartRendering;
	a_FuncCreateInfo.endRendering = VulkanEndRendering;

	a_FuncCreateInfo.uploadImage = VulkanUploadImage;
	a_FuncCreateInfo.copyBuffer = VulkanCopyBuffer;
	a_FuncCreateInfo.transitionImage = VulkanTransitionImage;

	a_FuncCreateInfo.bindPipeline = VulkanBindPipeline;
	a_FuncCreateInfo.bindVertBuffers = VulkanBindVertexBuffers;
	a_FuncCreateInfo.bindIndexBuffer = VulkanBindIndexBuffer;
	a_FuncCreateInfo.bindDescriptors = VulkanBindDescriptors;
	a_FuncCreateInfo.bindConstant = VulkanBindConstant;

	a_FuncCreateInfo.drawVertex = VulkanDrawVertex;
	a_FuncCreateInfo.drawIndex = VulkanDrawIndexed;

	a_FuncCreateInfo.bufferCopyData = VulkanBufferCopyData;
	a_FuncCreateInfo.mapMemory = VulkanMapMemory;
	a_FuncCreateInfo.unmapMemory = VulkanUnMemory;

	a_FuncCreateInfo.resizeWindow = VulkanResizeWindow;

	a_FuncCreateInfo.startFrame = VulkanStartFrame;
	a_FuncCreateInfo.executeCommands = VulkanExecuteCommands;
	a_FuncCreateInfo.executePresentCommands = VulkanExecutePresentCommand;
	a_FuncCreateInfo.presentFrame = VulkanPresentFrame;

	a_FuncCreateInfo.nextQueueFenceValue = VulkanNextQueueFenceValue;
	a_FuncCreateInfo.nextFenceValue = VulkanNextFenceValue;

	a_FuncCreateInfo.waitDevice = VulkanWaitDeviceReady;

	a_FuncCreateInfo.destroyBackend = VulkanDestroyBackend;
	a_FuncCreateInfo.destroyDescriptor = VulkanDestroyDescriptor;
	a_FuncCreateInfo.destroyPipeline = VulkanDestroyPipeline;
	a_FuncCreateInfo.destroyCommandQueue = VulkanDestroyCommandQueue;
	a_FuncCreateInfo.destroyCommandAllocator = VulkanDestroyCommandAllocator;
	a_FuncCreateInfo.destroyCommandList = VulkanDestroyCommandList;
	a_FuncCreateInfo.destroyBuffer = VulkanDestroyBuffer;
	a_FuncCreateInfo.destroyImage = VulkanDestroyImage;
	a_FuncCreateInfo.destroyFence = VulkanDestroyFence;
}
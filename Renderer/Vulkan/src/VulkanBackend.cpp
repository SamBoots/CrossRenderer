#include "VulkanBackend.h"
#include "VulkanCommon.h"

using namespace BB;

void BB::GetVulkanAPIFunctions(RenderAPIFunctions& a_FuncCreateInfo)
{
	a_FuncCreateInfo.createBackend = VulkanCreateBackend;
	a_FuncCreateInfo.createDescriptorHeap = VulkanCreateDescriptorHeap;
	a_FuncCreateInfo.createDescriptor = VulkanCreateDescriptor;
	a_FuncCreateInfo.createCommandQueue = VulkanCreateCommandQueue;
	a_FuncCreateInfo.createCommandAllocator = VulkanCreateCommandAllocator;
	a_FuncCreateInfo.createCommandList = VulkanCreateCommandList;
	a_FuncCreateInfo.createBuffer = VulkanCreateBuffer;
	a_FuncCreateInfo.createImage = VulkanCreateImage;
	a_FuncCreateInfo.createSampler = VulkanCreateSampler;
	a_FuncCreateInfo.createFence = VulkanCreateFence;

	a_FuncCreateInfo.setResourceName = VulkanSetResourceName;

	a_FuncCreateInfo.allocateDescriptor = VulkanAllocateDescriptor;
	a_FuncCreateInfo.copyDescriptors = VulkanCopyDescriptors;
	a_FuncCreateInfo.writeDescriptors = VulkanWriteDescriptors;
	a_FuncCreateInfo.getImageInfo = VulkanGetImageInfo;

	a_FuncCreateInfo.pipelineBuilderInit = VulkanPipelineBuilderInit;
	a_FuncCreateInfo.pipelineBuilderBindDescriptor = VulkanPipelineBuilderBindDescriptor;
	a_FuncCreateInfo.pipelineBuilderBindShaders = VulkanPipelineBuilderBindShaders;
	a_FuncCreateInfo.pipelineBuilderBindAttributes = VulkanPipelineBuilderBindAttributes;
	a_FuncCreateInfo.pipelineBuilderBuildPipeline = VulkanPipelineBuildPipeline;

	a_FuncCreateInfo.resetCommandAllocator = VulkanResetCommandAllocator;

	a_FuncCreateInfo.startCommandList = VulkanStartCommandList;
	a_FuncCreateInfo.endCommandList = VulkanEndCommandList;
	a_FuncCreateInfo.startRendering = VulkanStartRendering;
	a_FuncCreateInfo.setScissor = VulkanSetScissor;
	a_FuncCreateInfo.endRendering = VulkanEndRendering;

	a_FuncCreateInfo.copyBuffer = VulkanCopyBuffer;
	a_FuncCreateInfo.copyBufferImage = VulkanCopyBufferImage;
	a_FuncCreateInfo.setPipelineBarriers = VulkanPipelineBarriers;

	a_FuncCreateInfo.bindDescriptorHeaps = VulkanBindDescriptorHeaps;
	a_FuncCreateInfo.bindPipeline = VulkanBindPipeline;
	a_FuncCreateInfo.setDescriptorHeapOffsets = VulkanSetDescriptorHeapOffsets;
	a_FuncCreateInfo.bindVertBuffers = VulkanBindVertexBuffers;
	a_FuncCreateInfo.bindIndexBuffer = VulkanBindIndexBuffer;
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

	a_FuncCreateInfo.waitCommands = VulkanWaitCommands;

	a_FuncCreateInfo.destroyBackend = VulkanDestroyBackend;
	a_FuncCreateInfo.destroyDescriptorHeap = VulkanDestroyDescriptorHeap;
	a_FuncCreateInfo.destroyDescriptor = VulkanDestroyDescriptor;
	a_FuncCreateInfo.destroyPipeline = VulkanDestroyPipeline;
	a_FuncCreateInfo.destroyCommandQueue = VulkanDestroyCommandQueue;
	a_FuncCreateInfo.destroyCommandAllocator = VulkanDestroyCommandAllocator;
	a_FuncCreateInfo.destroyCommandList = VulkanDestroyCommandList;
	a_FuncCreateInfo.destroyBuffer = VulkanDestroyBuffer;
	a_FuncCreateInfo.destroyImage = VulkanDestroyImage;
	a_FuncCreateInfo.destroySampler = VulkanDestroySampler;
	a_FuncCreateInfo.destroyFence = VulkanDestroyFence;
}
#include "DX12Backend.h"
#include "DX12Common.h"

#include "RenderBackendCommon.h"


using namespace BB;

void TempResizeWindow(Allocator a_TempAllocator, const uint32_t a_X, const uint32_t a_Y)
{

}

void BB::GetRenderAPIFunctions(RenderAPIFunctions& a_FuncCreateInfo)
{
	a_FuncCreateInfo.createBackend = DX12CreateBackend;
	a_FuncCreateInfo.createDescriptor = DX12CreateDescriptor;
	a_FuncCreateInfo.createCommandQueue = DX12CreateCommandQueue;
	a_FuncCreateInfo.createCommandAllocator = DX12CreateCommandAllocator;
	a_FuncCreateInfo.createCommandList = DX12CreateCommandList;
	a_FuncCreateInfo.createBuffer = DX12CreateBuffer;
	a_FuncCreateInfo.createImage = DX12CreateImage;
	a_FuncCreateInfo.createSampler = DX12CreateSampler;
	a_FuncCreateInfo.createFence = DX12CreateFence;

	a_FuncCreateInfo.updateDescriptorBuffer = DX12UpdateDescriptorBuffer;
	a_FuncCreateInfo.updateDescriptorImage = DX12UpdateDescriptorImage;
	a_FuncCreateInfo.getImageInfo = DX12GetImageInfo;

	a_FuncCreateInfo.pipelineBuilderInit = DX12PipelineBuilderInit;
	a_FuncCreateInfo.pipelineBuilderBindDescriptor = DX12PipelineBuilderBindDescriptor;
	a_FuncCreateInfo.pipelineBuilderBindShaders = DX12PipelineBuilderBindShaders;
	a_FuncCreateInfo.pipelineBuilderBindAttributes = DX12PipelineBuilderBindAttributes;
	a_FuncCreateInfo.pipelineBuilderBuildPipeline = DX12PipelineBuildPipeline;

	a_FuncCreateInfo.copyBuffer = DX12CopyBuffer;
	a_FuncCreateInfo.copyBufferImage = DX12CopyBufferImage;
	a_FuncCreateInfo.transitionImage = DX12TransitionImage;

	a_FuncCreateInfo.startCommandList = DX12StartCommandList;
	a_FuncCreateInfo.resetCommandAllocator = DX12ResetCommandAllocator;
	a_FuncCreateInfo.endCommandList = DX12EndCommandList;
	a_FuncCreateInfo.startRendering = DX12StartRendering;
	a_FuncCreateInfo.setScissor = DX12SetScissor;
	a_FuncCreateInfo.endRendering = DX12EndRendering;

	a_FuncCreateInfo.bindPipeline = DX12BindPipeline;
	a_FuncCreateInfo.bindVertBuffers = DX12BindVertexBuffers;
	a_FuncCreateInfo.bindIndexBuffer = DX12BindIndexBuffer;
	a_FuncCreateInfo.bindDescriptors = DX12BindDescriptors;
	a_FuncCreateInfo.bindConstant = DX12BindConstant;

	a_FuncCreateInfo.drawVertex = DX12DrawVertex;
	a_FuncCreateInfo.drawIndex = DX12DrawIndexed;

	a_FuncCreateInfo.bufferCopyData = DX12BufferCopyData;
	a_FuncCreateInfo.mapMemory = DX12MapMemory;
	a_FuncCreateInfo.unmapMemory = DX12UnMemory;

	//a_FuncCreateInfo.resizeWindow = TempResizeWindow;

	a_FuncCreateInfo.startFrame = DX12StartFrame;
	a_FuncCreateInfo.executeCommands = DX12ExecuteCommands;
	a_FuncCreateInfo.executePresentCommands = DX12ExecutePresentCommand;
	a_FuncCreateInfo.presentFrame = DX12PresentFrame;

	a_FuncCreateInfo.nextQueueFenceValue = DX12NextQueueFenceValue;
	a_FuncCreateInfo.nextFenceValue = DX12NextFenceValue;

	a_FuncCreateInfo.waitCommands = DX12WaitCommands;

	a_FuncCreateInfo.destroyBackend = DX12DestroyBackend;
	a_FuncCreateInfo.destroyDescriptor = DX12DestroyDescriptor;
	a_FuncCreateInfo.destroyPipeline = DX12DestroyPipeline;
	a_FuncCreateInfo.destroyCommandQueue = DX12DestroyCommandQueue;
	a_FuncCreateInfo.destroyCommandAllocator = DX12DestroyCommandAllocator;
	a_FuncCreateInfo.destroyCommandList = DX12DestroyCommandList;
	a_FuncCreateInfo.destroyBuffer = DX12DestroyBuffer;
	a_FuncCreateInfo.destroyImage = DX12DestroyImage;
	a_FuncCreateInfo.destroySampler = DX12DestroySampler;
	a_FuncCreateInfo.destroyFence = DX12DestroyFence;
}
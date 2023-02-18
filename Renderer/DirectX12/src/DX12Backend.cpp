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
	a_FuncCreateInfo.createFrameBuffer = DX12CreateFrameBuffer;
	a_FuncCreateInfo.createBindingSet = DX12CreateBindingSet;
	a_FuncCreateInfo.createCommandQueue = DX12CreateCommandQueue;
	a_FuncCreateInfo.createCommandAllocator = DX12CreateCommandAllocator;
	a_FuncCreateInfo.createCommandList = DX12CreateCommandList;
	a_FuncCreateInfo.createBuffer = DX12CreateBuffer;
	a_FuncCreateInfo.createFence = DX12CreateFence;

	a_FuncCreateInfo.pipelineBuilderInit = DX12PipelineBuilderInit;
	a_FuncCreateInfo.pipelineBuilderBindBindingSet = DX12PipelineBuilderBindBindingSet;
	a_FuncCreateInfo.pipelineBuilderBindShaders = DX12PipelineBuilderBindShaders;
	a_FuncCreateInfo.pipelineBuilderBuildPipeline = DX12PipelineBuildPipeline;

	a_FuncCreateInfo.startCommandList = DX12StartCommandList;
	a_FuncCreateInfo.resetCommandAllocator = DX12ResetCommandAllocator;
	a_FuncCreateInfo.endCommandList = DX12EndCommandList;
	a_FuncCreateInfo.startRenderPass = DX12StartRenderPass;
	a_FuncCreateInfo.endRenderPass = DX12EndRenderPass;
	a_FuncCreateInfo.bindPipeline = DX12BindPipeline;
	a_FuncCreateInfo.bindVertBuffers = DX12BindVertexBuffers;
	a_FuncCreateInfo.bindIndexBuffer = DX12BindIndexBuffer;
	a_FuncCreateInfo.bindBindingSet = DX12BindBindingSets;
	a_FuncCreateInfo.bindConstant = DX12BindConstant;

	a_FuncCreateInfo.drawVertex = DX12DrawVertex;
	a_FuncCreateInfo.drawIndex = DX12DrawIndexed;

	a_FuncCreateInfo.bufferCopyData = DX12BufferCopyData;
	a_FuncCreateInfo.copyBuffer = DX12CopyBuffer;
	a_FuncCreateInfo.mapMemory = DX12MapMemory;
	a_FuncCreateInfo.unmapMemory = DX12UnMemory;

	a_FuncCreateInfo.resizeWindow = TempResizeWindow;

	a_FuncCreateInfo.startFrame = DX12StartFrame;
	a_FuncCreateInfo.executeCommands = DX12ExecuteCommands;
	a_FuncCreateInfo.executePresentCommands = DX12ExecutePresentCommand;
	a_FuncCreateInfo.presentFrame = DX12PresentFrame;

	a_FuncCreateInfo.nextQueueFenceValue = DX12NextQueueFenceValue;
	a_FuncCreateInfo.nextFenceValue = DX12NextFenceValue;

	a_FuncCreateInfo.waitDevice = DX12WaitDeviceReady;

	a_FuncCreateInfo.destroyBackend = DX12DestroyBackend;
	a_FuncCreateInfo.destroyFrameBuffer = DX12DestroyFramebuffer;
	a_FuncCreateInfo.destroyBindingSet = DX12DestroyBindingSet;
	a_FuncCreateInfo.destroyPipeline = DX12DestroyPipeline;
	a_FuncCreateInfo.destroyCommandQueue = DX12DestroyCommandQueue;
	a_FuncCreateInfo.destroyCommandAllocator = DX12DestroyCommandAllocator;
	a_FuncCreateInfo.destroyCommandList = DX12DestroyCommandList;
	a_FuncCreateInfo.destroyBuffer = DX12DestroyBuffer;
	a_FuncCreateInfo.destroyFence = DX12DestroyFence;
}
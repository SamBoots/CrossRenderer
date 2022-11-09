#include "DX12Backend.h"
#include "DX12Common.h"

using namespace BB;


void BB::GetRenderAPIFunctions(RenderAPIFunctions& a_FuncCreateInfo)
{
	a_FuncCreateInfo.createBackend = DX12CreateBackend;
	//a_FuncCreateInfo.createDescriptor = DX12CreateDescriptor;
	a_FuncCreateInfo.createPipeline = DX12CreatePipeline;
	//a_FuncCreateInfo.createFrameBuffer = DX12CreateFrameBuffer;
	a_FuncCreateInfo.createCommandList = DX12CreateCommandList;
	a_FuncCreateInfo.createBuffer = DX12CreateBuffer;

	//a_FuncCreateInfo.startCommandList = DX12StartCommandList;
	//a_FuncCreateInfo.resetCommandList = DX12ResetCommandList;
	//a_FuncCreateInfo.endCommandList = DX12EndCommandList;
	//a_FuncCreateInfo.bindPipeline = DX12BindPipeline;
	//a_FuncCreateInfo.bindVertBuffers = DX12BindVertexBuffers;
	//a_FuncCreateInfo.bindIndexBuffer = DX12BindIndexBuffer;
	//a_FuncCreateInfo.bindDescriptor = DX12BindDescriptorSets;
	//a_FuncCreateInfo.bindConstant = DX12BindConstant;

	//a_FuncCreateInfo.drawVertex = DX12DrawVertex;
	//a_FuncCreateInfo.drawIndex = DX12DrawIndexed;

	a_FuncCreateInfo.bufferCopyData = DX12BufferCopyData;
	//a_FuncCreateInfo.copyBuffer = DX12CopyBuffer;
	//a_FuncCreateInfo.mapMemory = DX12MapMemory;
	//a_FuncCreateInfo.unmapMemory = DX12UnMemory;

	//a_FuncCreateInfo.resizeWindow = DX12ResizeWindow;

	//a_FuncCreateInfo.startFrame = DX12StartFrame;
	//a_FuncCreateInfo.renderFrame = DX12RenderFrame;
	//a_FuncCreateInfo.waitDevice = DX12WaitDeviceReady;

	a_FuncCreateInfo.destroyBackend = DX12DestroyBackend;
	//a_FuncCreateInfo.destroyDescriptorLayout = DX12DestroyDescriptorSetLayout;
	//a_FuncCreateInfo.destroyDescriptor = DX12DestroyDescriptorSet;
	//a_FuncCreateInfo.destroyFrameBuffer = DX12DestroyFramebuffer;
	a_FuncCreateInfo.destroyPipeline = DX12DestroyPipeline;
	a_FuncCreateInfo.destroyCommandList = DX12DestroyCommandList;
	a_FuncCreateInfo.destroyBuffer = DX12DestroyBuffer;
}
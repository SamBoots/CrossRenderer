#include "DX12Backend.h"
#include "DX12Common.h"

using namespace BB;

FrameBufferHandle TempFrameFuncCreate(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo)
{
	return 1;
}

void TempFrameFunc(FrameBufferHandle a_Handle)
{

}


void BB::GetDX12APIFunctions(RenderAPIFunctions& a_FuncCreateInfo)
{
	a_FuncCreateInfo.createBackend = DX12CreateBackend;
	a_FuncCreateInfo.createPipeline = DX12CreatePipeline;
	a_FuncCreateInfo.createFrameBuffer = TempFrameFuncCreate;
	a_FuncCreateInfo.createCommandList = DX12CreateCommandList;
	a_FuncCreateInfo.createBuffer = DX12CreateBuffer;

	a_FuncCreateInfo.bufferCopyData = DX12BufferCopyData;

	//*a_FuncCreateInfo.resizeWindow = DX12ResizeWindow;
	a_FuncCreateInfo.renderFrame = DX12RenderFrame;
	a_FuncCreateInfo.waitDevice = DX12WaitDeviceReady;

	a_FuncCreateInfo.destroyBackend = DX12DestroyBackend;
	a_FuncCreateInfo.destroyFrameBuffer = TempFrameFunc;
	a_FuncCreateInfo.destroyPipeline = DX12DestroyPipeline;
	a_FuncCreateInfo.destroyCommandList = DX12DestroyCommandList;
	a_FuncCreateInfo.destroyBuffer = DX12DestroyBuffer;
}
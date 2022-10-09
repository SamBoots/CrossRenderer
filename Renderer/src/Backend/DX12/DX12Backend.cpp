#include "DX12Backend.h"
#include "DX12Common.h"

using namespace BB;

void BB::GetDX12APIFunctions(APIBackendFunctionPointersCreateInfo& a_FuncCreateInfo)
{
	*a_FuncCreateInfo.createBackend = DX12CreateBackend;
	//*a_FuncCreateInfo.createPipeline = DX12CreatePipeline;
	//*a_FuncCreateInfo.createFrameBuffer = DX12CreateFrameBuffer;
	//*a_FuncCreateInfo.createCommandList = DX12CreateCommandList;
	//*a_FuncCreateInfo.createBuffer = DX12CreateBuffer;

	//*a_FuncCreateInfo.bufferCopyData = DX12BufferCopyData;

	//*a_FuncCreateInfo.resizeWindow = DX12ResizeWindow;
	//*a_FuncCreateInfo.renderFrame = DX12RenderFrame;
	//*a_FuncCreateInfo.waitDevice = DX12WaitDeviceReady;

	*a_FuncCreateInfo.destroyBackend = DX12DestroyBackend;
	//*a_FuncCreateInfo.destroyFrameBuffer = DX12DestroyFramebuffer;
	//*a_FuncCreateInfo.destroyPipeline = DX12DestroyPipeline;
	//*a_FuncCreateInfo.destroyCommandList = DX12DestroyCommandList;
	//*a_FuncCreateInfo.destroyBuffer = DX12DestroyBuffer;
}
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

//Compile HLSL in runtime for ease of use.
#include <d3dcompiler.h>

#include "RenderBackendCommon.h"

#ifdef _DEBUG
#define DXASSERT(a_HRESULT, a_Msg)\
	if (a_HRESULT != S_OK)\
		BB_ASSERT(false, a_Msg);\

#else
#define DXASSERT(a_HRESULT, a_Msg) a_HRESULT

#endif //_DEBUG

namespace BB
{
	struct DX12Device
	{
		IDXGIAdapter1* adapter;
		ID3D12Device* device;
#ifdef _DEBUG
		ID3D12DebugDevice* debugDevice;
#endif //_DEBUG
	};

	struct DX12CommandList
	{
		ID3D12CommandQueue* commandQueue;
		ID3D12CommandAllocator* commandAllocator;
		ID3D12GraphicsCommandList* commandList;
	};

	struct DX12SwapChain
	{

	};
}
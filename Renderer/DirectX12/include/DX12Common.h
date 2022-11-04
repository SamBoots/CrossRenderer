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
	union DX12BufferView
	{
		D3D12_VERTEX_BUFFER_VIEW vertexView;
		D3D12_INDEX_BUFFER_VIEW indexView;
	};

	namespace DX12Conv
	{

	}

	struct DX12Device
	{
		IDXGIAdapter1* adapter;
		ID3D12Device* logicalDevice;

		ID3D12DebugDevice* debugDevice;
	};

	struct DX12Swapchain
	{
		UINT width;
		UINT height;
		IDXGISwapChain3* swapchain;

		ID3D12DescriptorHeap* rtvHeap;
		ID3D12Resource** renderTargets; //dyn alloc

		D3D12_VIEWPORT viewport;
		D3D12_RECT surfaceRect;
		UINT rtvDescriptorSize;
	};

	//Functions
	BackendInfo DX12CreateBackend(Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo);
	FrameBufferHandle DX12CreateFrameBuffer(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo);
	PipelineHandle DX12CreatePipeline(Allocator a_TempAllocator, const RenderPipelineCreateInfo& a_CreateInfo);
	CommandListHandle DX12CreateCommandList(Allocator a_TempAllocator, const RenderCommandListCreateInfo& a_CreateInfo);
	RBufferHandle DX12CreateBuffer(const RenderBufferCreateInfo& a_Info);
	
	void DX12BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);

	void DX12ResizeWindow(Allocator a_TempAllocator, const uint32_t a_X, const uint32_t a_Y);
	void DX12RenderFrame(Allocator a_TempAllocator, const CommandListHandle a_CommandHandle, const FrameBufferHandle a_FrameBufferHandle, const PipelineHandle a_PipeHandle);

	void DX12WaitDeviceReady();

	void DX12DestroyBuffer(const RBufferHandle a_Handle);
	void DX12DestroyCommandList(const CommandListHandle a_Handle);
	void DX12DestroyFramebuffer(const FrameBufferHandle a_Handle);
	void DX12DestroyPipeline(const PipelineHandle a_Handle);
	void DX12DestroyBackend();
}
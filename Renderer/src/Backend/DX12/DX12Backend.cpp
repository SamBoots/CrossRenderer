#include "DX12Backend.h"
#include "DX12Common.h"

//Tutorial used for this DX12 backend was https://alain.xyz/blog/raw-directx12 

using namespace BB;

static FreelistAllocator_t s_DX12Allocator{ mbSize * 2 };

struct DX12Backend_inst
{
	uint32_t currentFrame = 0;
	uint32_t frameCount = 3; //for now hardcode 3 backbuffers.

	uint32_t swapchainWidth;
	uint32_t swapchainHeight;

	IDXGIFactory4* factory;
	ID3D12Debug1* debugController;
	IDXGISwapChain3* swapchain;
	ID3D12Fence* fence;

	DX12Device device;
};
static DX12Backend_inst s_DXBackendInst;

static DX12Device CreateDevice(IDXGIFactory4* a_Factory)
{
	DX12Device t_Device{};

	IDXGIAdapter1* t_CurrentBestAdapter = nullptr;
	SIZE_T t_BestDedicatedVRAM = 0;

	for (UINT adapterIndex = 0;
		DXGI_ERROR_NOT_FOUND != a_Factory->EnumAdapters1(adapterIndex, &t_Device.adapter);
		++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 t_Desc;
		t_Device.adapter->GetDesc1(&t_Desc);

		//We don't take the software adapter.
		if (t_Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (t_Desc.DedicatedVideoMemory > t_BestDedicatedVRAM)
		{
			t_BestDedicatedVRAM = t_Desc.DedicatedVideoMemory;
			t_CurrentBestAdapter = t_Device.adapter;
		}
	}
	t_Device.adapter = t_CurrentBestAdapter;

	DXASSERT(D3D12CreateDevice(t_Device.adapter,
		D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(&t_Device.device)),
		"DX12: Failed to create logical device.");

#ifdef _DEBUG
	DXASSERT(t_Device.device->QueryInterface(&t_Device.debugDevice),
		"DX12: Failed to query debug device.");
#endif //_DEBUG

	return t_Device;
}

static DX12CommandList CreateCommandList(ID3D12Device* a_Device)
{
	DX12CommandList t_CommandList{};

	D3D12_COMMAND_QUEUE_DESC t_QueueDesc{};
	t_QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	t_QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DXASSERT(a_Device->CreateCommandQueue(&t_QueueDesc,
		IID_PPV_ARGS(&t_CommandList.commandQueue)),
		"DX12: Failed to create commandqueue.");

	DXASSERT(a_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&t_CommandList.commandAllocator)),
		"DX12: Failed to create command allocator.");
}

static DX12SwapChain CreateSwapchain(uint32_t a_Width, uint32_t a_Height)
{
	DX12SwapChain t_SwapChain{};

	D3D12_VIEWPORT t_Viewport;
	D3D12_RECT t_SurfaceRect;

	t_Viewport.TopLeftX = 0.0f;
	t_Viewport.TopLeftY = 0.0f;
	t_Viewport.Width = static_cast<float>(a_Width);
	t_Viewport.Height = static_cast<float>(a_Height);
	t_Viewport.MinDepth = .1f;
	t_Viewport.MaxDepth = 1000.f;

	t_SurfaceRect.left = 0;
	t_SurfaceRect.top = 0;
	t_SurfaceRect.right = static_cast<LONG>(a_Width);
	t_SurfaceRect.bottom = static_cast<LONG>(a_Height);


	return t_SwapChain;
}

APIRenderBackend BB::DX12CreateBackend(Allocator a_SysAllocator, Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo)
{
	UINT t_FactoryFlags = 0;

	if (a_CreateInfo.validationLayers)
	{
		ID3D12Debug* t_DebugController;
		DXASSERT(D3D12GetDebugInterface(IID_PPV_ARGS(&t_DebugController)),
			"DX12: failed to create debuginterface.");
		DXASSERT(t_DebugController->QueryInterface(IID_PPV_ARGS(&s_DXBackendInst.debugController)),
			"DX12: failed to create debuginterface.");
		s_DXBackendInst.debugController->EnableDebugLayer();
		s_DXBackendInst.debugController->SetEnableGPUBasedValidation(true);

		t_FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		t_DebugController->Release();
	}

	DXASSERT(CreateDXGIFactory2(t_FactoryFlags, IID_PPV_ARGS(&s_DXBackendInst.factory)),
		"DX12: failed to create DXGIFactory2.");
	

	//The handle doesn't matter, we only have one backend anyway. But it's nice for API clarity.
	return APIRenderBackend(1);
}
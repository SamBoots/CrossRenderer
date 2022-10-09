#include "DX12Backend.h"
#include "DX12Common.h"
#include "D3D12MemAlloc.h"

//Tutorial used for this DX12 backend was https://alain.xyz/blog/raw-directx12 

using namespace BB;

static FreelistAllocator_t s_DX12Allocator{ mbSize * 2 };

struct DX12Backend_inst
{
	UINT currentFrame = 0;
	UINT backBufferCount = 3; //for now hardcode 3 backbuffers.

	IDXGIFactory4* factory{};
	ID3D12Debug1* debugController{};

	ID3D12CommandQueue* directQueue{};
	ID3D12CommandAllocator* commandAllocator{};

	ID3D12Fence* fence{};

	DX12Device device{};
	DX12Swapchain swapchain{};

	D3D12MA::Allocator* DXMA;
};
static DX12Backend_inst s_DX12BackendInst;


static void SetupBackendSwapChain(UINT a_Width, UINT a_Height, HWND a_WindowHandle)
{
	s_DX12BackendInst.swapchain.width = a_Width;
	s_DX12BackendInst.swapchain.height = a_Height;

	//Just do a resize if a swapchain already exists.
	if (s_DX12BackendInst.swapchain.swapchain != nullptr)
	{
		s_DX12BackendInst.swapchain.swapchain->ResizeBuffers(s_DX12BackendInst.backBufferCount,
			a_Width,
			a_Height,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0);
		return;
	}

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

	DXGI_SWAP_CHAIN_DESC1 t_SwapchainDesc = {};
	t_SwapchainDesc.BufferCount = s_DX12BackendInst.backBufferCount;
	t_SwapchainDesc.Width = a_Width;
	t_SwapchainDesc.Height = a_Height;
	t_SwapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	t_SwapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	t_SwapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	t_SwapchainDesc.SampleDesc.Count = 1;


	IDXGISwapChain1* t_NewSwapchain;
	DXASSERT(s_DX12BackendInst.factory->CreateSwapChainForHwnd(
		s_DX12BackendInst.directQueue,
		a_WindowHandle,
		&t_SwapchainDesc,
		nullptr,
		nullptr,
		&t_NewSwapchain), 
		"DX12: Failed to create swapchain1");

	DXASSERT(s_DX12BackendInst.factory->MakeWindowAssociation(a_WindowHandle,
		DXGI_MWA_NO_ALT_ENTER),
		"DX12: Failed to add DXGI_MWA_NO_ALT_ENTER to window.");

	DXASSERT(s_DX12BackendInst.swapchain.swapchain->QueryInterface(
		__uuidof(IDXGISwapChain3), (void**)&t_NewSwapchain),
		"DX12: Failed to get support for a IDXGISwapchain3.");

	s_DX12BackendInst.swapchain.swapchain = (IDXGISwapChain3*)t_NewSwapchain;

	s_DX12BackendInst.currentFrame = s_DX12BackendInst.swapchain.swapchain->GetCurrentBackBufferIndex();

	D3D12_DESCRIPTOR_HEAP_DESC t_RtvHeapDesc = {};
	t_RtvHeapDesc.NumDescriptors = s_DX12BackendInst.backBufferCount;
	t_RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	t_RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateDescriptorHeap(
		&t_RtvHeapDesc, IID_PPV_ARGS(&s_DX12BackendInst.swapchain.rtvHeap)),
		"DX12: Failed to create descriptor heap for swapchain.");


	UINT rtvDescriptorSize =
		s_DX12BackendInst.device.logicalDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = 
		s_DX12BackendInst.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart();

	// Create a RTV for each frame.
	for (UINT i = 0; i < s_DX12BackendInst.backBufferCount; i++)
	{
		DXASSERT(s_DX12BackendInst.swapchain.swapchain->GetBuffer(i,
			IID_PPV_ARGS(&s_DX12BackendInst.swapchain.renderTargets[i])),
			"DX12: Failed to get swapchain buffer.");

		s_DX12BackendInst.device.logicalDevice->CreateRenderTargetView(
			s_DX12BackendInst.swapchain.renderTargets[i], 
			nullptr, 
			rtvHandle);
		rtvHandle.ptr += (1 * rtvDescriptorSize);
	}
}


APIRenderBackend BB::DX12CreateBackend(Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo)
{
	UINT t_FactoryFlags = 0;

	if (a_CreateInfo.validationLayers)
	{
		ID3D12Debug* t_DebugController;
		DXASSERT(D3D12GetDebugInterface(IID_PPV_ARGS(&t_DebugController)),
			"DX12: failed to create debuginterface.");
		DXASSERT(t_DebugController->QueryInterface(IID_PPV_ARGS(&s_DX12BackendInst.debugController)),
			"DX12: failed to create debuginterface.");
		s_DX12BackendInst.debugController->EnableDebugLayer();
		s_DX12BackendInst.debugController->SetEnableGPUBasedValidation(true);

		t_FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		t_DebugController->Release();
	}
	else
	{
		s_DX12BackendInst.debugController = nullptr;
	}

	DXASSERT(CreateDXGIFactory2(t_FactoryFlags, IID_PPV_ARGS(&s_DX12BackendInst.factory)),
		"DX12: failed to create DXGIFactory2.");

#pragma region DEVICE_CREATION
	IDXGIAdapter1* t_CurrentBestAdapter = nullptr;
	SIZE_T t_BestDedicatedVRAM = 0;

	for (UINT adapterIndex = 0;
		DXGI_ERROR_NOT_FOUND != s_DX12BackendInst.factory->EnumAdapters1(adapterIndex, &s_DX12BackendInst.device.adapter);
		++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 t_Desc;
		s_DX12BackendInst.device.adapter->GetDesc1(&t_Desc);

		//We don't take the software adapter.
		if (t_Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (t_Desc.DedicatedVideoMemory > t_BestDedicatedVRAM)
		{
			t_BestDedicatedVRAM = t_Desc.DedicatedVideoMemory;
			t_CurrentBestAdapter = s_DX12BackendInst.device.adapter;
		}
	}
	s_DX12BackendInst.device.adapter = t_CurrentBestAdapter;

	DXASSERT(D3D12CreateDevice(s_DX12BackendInst.device.adapter,
		D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(&s_DX12BackendInst.device.logicalDevice)),
		"DX12: Failed to create logical device.");

	if (a_CreateInfo.validationLayers)
	{
		DXASSERT(s_DX12BackendInst.device.logicalDevice->QueryInterface(&s_DX12BackendInst.device.debugDevice),
			"DX12: Failed to query debug device.");
	}
	else
	{
		s_DX12BackendInst.device.debugDevice = nullptr;
	}
#pragma endregion //DEVICE_CREATION

	D3D12MA::ALLOCATOR_DESC t_AllocatorDesc = {};
	t_AllocatorDesc.pDevice = s_DX12BackendInst.device.logicalDevice;
	t_AllocatorDesc.pAdapter = s_DX12BackendInst.device.adapter;

	DXASSERT(D3D12MA::CreateAllocator(&t_AllocatorDesc, &s_DX12BackendInst.DXMA),
		"DX12: Failed to create DX12 memory allocator.");

	D3D12_COMMAND_QUEUE_DESC t_QueueDesc{};
	t_QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	t_QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateCommandQueue(&t_QueueDesc,
		IID_PPV_ARGS(&s_DX12BackendInst.directQueue)),
		"DX12: Failed to create direct command queue.");

	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&s_DX12BackendInst.commandAllocator)),
		"DX12: Failed to create command allocator");

	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&s_DX12BackendInst.fence)),
		"DX12: failed to create fence.");

	SetupBackendSwapChain(a_CreateInfo.windowWidth, a_CreateInfo.windowHeight, a_CreateInfo.hwnd);

	//The handle doesn't matter, we only have one backend anyway. But it's nice for API clarity.
	return APIRenderBackend(1);
}

void BB::DX12DestroyBackend(APIRenderBackend)
{
	for (size_t i = 0; i < s_DX12BackendInst.backBufferCount; i++)
	{
		s_DX12BackendInst.swapchain.renderTargets[i]->Release();
	}
	BBfreeArr(s_DX12Allocator, s_DX12BackendInst.swapchain.renderTargets);
	s_DX12BackendInst.swapchain.rtvHeap->Release();
	s_DX12BackendInst.swapchain.swapchain->Release();

	s_DX12BackendInst.commandAllocator->Release();
	s_DX12BackendInst.directQueue->Release();
	s_DX12BackendInst.fence->Release();

	s_DX12BackendInst.DXMA->Release();
	if (s_DX12BackendInst.device.debugDevice)
		s_DX12BackendInst.device.debugDevice->Release();
	s_DX12BackendInst.device.logicalDevice->Release();
	s_DX12BackendInst.device.adapter->Release();
	if (s_DX12BackendInst.debugController)
		s_DX12BackendInst.debugController->Release();
	s_DX12BackendInst.factory->Release();
}
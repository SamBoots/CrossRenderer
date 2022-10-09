#include "DX12Backend.h"
#include "DX12Common.h"
#include "D3D12MemAlloc.h"

#include "Slotmap.h"

//Tutorial used for this DX12 backend was https://alain.xyz/blog/raw-directx12 

using namespace BB;

static FreelistAllocator_t s_DX12Allocator{ mbSize * 2 };

struct DXMAResource
{
	D3D12MA::Allocation* allocation;
	ID3D12Resource* resource;
};

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

	Slotmap<ID3D12PipelineState*> pipelines{ s_DX12Allocator };
};
static DX12Backend_inst s_DX12BackendInst;

static DXMAResource CreateResource()
{
	DXMAResource t_Resource;

	D3D12_RESOURCE_DESC t_ResourceDesc = {}; 
	t_ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	t_ResourceDesc.Alignment = 0;
	t_ResourceDesc.Width = s_DX12BackendInst.swapchain.width;
	t_ResourceDesc.Height = s_DX12BackendInst.swapchain.height;
	t_ResourceDesc.DepthOrArraySize = 1;
	t_ResourceDesc.MipLevels = 1;
	t_ResourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	t_ResourceDesc.SampleDesc.Count = 1;
	t_ResourceDesc.SampleDesc.Quality = 0;
	t_ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	t_ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12MA::ALLOCATION_DESC t_AllocationDesc = {};
	t_AllocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	DXASSERT(s_DX12BackendInst.DXMA->CreateResource(
		&t_AllocationDesc,
		&t_ResourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		NULL,
		&t_Resource.allocation,
		IID_PPV_ARGS(&t_Resource.resource)),
		"DX12: Failed to create resource using D3D12 Memory Allocator");

	return t_Resource;
}

static void SetResource(DXMAResource a_Resource, BB::Buffer a_Buffer)
{
	void* t_MapPtr;

	DXASSERT(a_Resource.resource->Map(0, NULL, &t_MapPtr),
		"DX12: Failed to map resource.");

	memcpy(t_MapPtr, a_Buffer.data, a_Buffer.size);

	a_Resource.resource->Unmap(0, NULL);
}

static void DestroyResource(DXMAResource a_Resource)
{
	a_Resource.resource->Release();
	a_Resource.allocation->Release();
}

static ID3D12RootSignature* CreateRootSignature()
{
	ID3D12RootSignature* t_RootSignature = nullptr;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE t_FeatureData = {};
	t_FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(s_DX12BackendInst.device.logicalDevice->CheckFeatureSupport(
		D3D12_FEATURE_ROOT_SIGNATURE,
		&t_FeatureData, sizeof(t_FeatureData))))
	{
		t_FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	//Individual GPU Resources
	D3D12_DESCRIPTOR_RANGE1 t_Ranges[1];
	t_Ranges[0].BaseShaderRegister = 0;
	t_Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	t_Ranges[0].NumDescriptors = 1;
	t_Ranges[0].RegisterSpace = 0;
	t_Ranges[0].OffsetInDescriptorsFromTableStart = 0;
	t_Ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

	//Groups of GPU Resources
	D3D12_ROOT_PARAMETER1 t_RootParameters[1];
	t_RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	t_RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	t_RootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	t_RootParameters[0].DescriptorTable.pDescriptorRanges = t_Ranges;

	//Overall Layout
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC t_RootSignatureDesc;
	t_RootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	t_RootSignatureDesc.Desc_1_1.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	t_RootSignatureDesc.Desc_1_1.NumParameters = 1;
	t_RootSignatureDesc.Desc_1_1.pParameters = t_RootParameters;
	t_RootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
	t_RootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

	ID3DBlob* t_Signature;
	ID3DBlob* t_Error;

	DXASSERT(D3D12SerializeVersionedRootSignature(&t_RootSignatureDesc,
		&t_Signature, &t_Error),
		"DX12: Failed to serialize root signature.");

	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateRootSignature(0, 
		t_Signature->GetBufferPointer(),
		t_Signature->GetBufferSize(),
		IID_PPV_ARGS(&t_RootSignature)),
		"DX12: Failed to create root signature.");

	t_RootSignature->SetName(L"Hello Triangle Root Signature");

	t_Signature->Release();
	t_Error->Release();

	return t_RootSignature;
}

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

PipelineHandle BB::DX12CreatePipeline(Allocator a_TempAllocator, const RenderPipelineCreateInfo& a_CreateInfo)
{
	ID3D12PipelineState* t_PipelineState;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC t_PsoDesc = {};

	D3D12_INPUT_ELEMENT_DESC t_InputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} };

	t_PsoDesc.InputLayout = { t_InputElementDescs, _countof(t_InputElementDescs) };

	t_PsoDesc.pRootSignature = CreateRootSignature();

	//All the vertex stuff

	return PipelineHandle(s_DX12BackendInst.pipelines.emplace(t_PipelineState));
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
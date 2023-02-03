#include "DX12Common.h"
#include "DX12HelperTypes.h"
#include "D3D12MemAlloc.h"

#include "Slotmap.h"
#include "Pool.h"
#include "BBString.h"

//Tutorial used for this DX12 backend was https://alain.xyz/blog/raw-directx12 

using namespace BB;

struct DXMAResource
{
	D3D12MA::Allocation* allocation;
	ID3D12Resource* resource;
	DX12BufferView view;
};

struct DX12Backend_inst
{
	FrameIndex currentFrame = 0;
	UINT backBufferCount = 3; //for now hardcode 3 backbuffers.

	IDXGIFactory4* factory{};
	ID3D12Debug1* debugController{};

	DescriptorHeap* defaultHeap;
	DescriptorHeap* uploadHeap;

	DX12Device device{};
	DX12Swapchain swapchain{};

	D3D12MA::Allocator* DXMA;
	ID3D12CommandQueue* directpresentqueue;

	Slotmap<ID3D12RootSignature*> rootSignatures{ s_DX12Allocator };
	Slotmap<ID3D12PipelineState*> pipelines{ s_DX12Allocator };
	Slotmap<DX12FrameBuffer> frameBuffers{ s_DX12Allocator };

	Pool<DescriptorHeap> Descriptorheaps;
	Pool<DXCommandQueue> cmdQueues;
	Pool<DXCommandAllocator> cmdAllocators;
	Pool<DXMAResource> renderResources;
	Pool<ID3D12Fence> fencePool;

	void CreatePools()
	{
		Descriptorheaps.CreatePool(s_DX12Allocator, 16);
		cmdQueues.CreatePool(s_DX12Allocator, 4);
		cmdAllocators.CreatePool(s_DX12Allocator, 16);
		renderResources.CreatePool(s_DX12Allocator, 8);
		fencePool.CreatePool(s_DX12Allocator, 16);
	}

	void DestroyPools()
	{
		Descriptorheaps.DestroyPool(s_DX12Allocator);
		cmdQueues.DestroyPool(s_DX12Allocator);
		cmdAllocators.DestroyPool(s_DX12Allocator);
		renderResources.DestroyPool(s_DX12Allocator);
		fencePool.DestroyPool(s_DX12Allocator);
	}
};

static DX12Backend_inst s_DX12BackendInst;

constexpr uint64_t COMMAND_BUFFER_STANDARD_COUNT = 32;

enum class ShaderType
{
	VERTEX,
	PIXEL
};

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
		s_DX12BackendInst.directpresentqueue,
		a_WindowHandle,
		&t_SwapchainDesc,
		nullptr,
		nullptr,
		&t_NewSwapchain),
		"DX12: Failed to create swapchain1");

	DXASSERT(s_DX12BackendInst.factory->MakeWindowAssociation(a_WindowHandle,
		DXGI_MWA_NO_ALT_ENTER),
		"DX12: Failed to add DXGI_MWA_NO_ALT_ENTER to window.");

	DXASSERT(t_NewSwapchain->QueryInterface(
		__uuidof(IDXGISwapChain3), (void**)&t_NewSwapchain),
		"DX12: Failed to get support for a IDXGISwapchain3.");

	s_DX12BackendInst.swapchain.swapchain = (IDXGISwapChain3*)t_NewSwapchain;

	s_DX12BackendInst.currentFrame = s_DX12BackendInst.swapchain.swapchain->GetCurrentBackBufferIndex();

	//D3D12_DESCRIPTOR_HEAP_DESC t_RtvHeapDesc = {};
	//t_RtvHeapDesc.NumDescriptors = s_DX12BackendInst.backBufferCount;

	//t_RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	//t_RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	//DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateDescriptorHeap(
	//	&t_RtvHeapDesc, IID_PPV_ARGS(&s_DX12BackendInst.swapchain.rtvHeap)),
	//	"DX12: Failed to create descriptor heap for swapchain.");


	//D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = 
	//	s_DX12BackendInst.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart();

	//s_DX12BackendInst.swapchain.renderTargets = BBnewArr(s_DX12Allocator, s_DX12BackendInst.backBufferCount, ID3D12Resource*);

	//// Create a RTV for each frame.
	//for (UINT i = 0; i < s_DX12BackendInst.backBufferCount; i++)
	//{
	//	DXASSERT(s_DX12BackendInst.swapchain.swapchain->GetBuffer(i,
	//		IID_PPV_ARGS(&s_DX12BackendInst.swapchain.renderTargets[i])),
	//		"DX12: Failed to get swapchain buffer.");

	//	s_DX12BackendInst.device.logicalDevice->CreateRenderTargetView(
	//		s_DX12BackendInst.swapchain.renderTargets[i], 
	//		nullptr, 
	//		rtvHandle);
	//	rtvHandle.ptr += (1 * s_DX12BackendInst.swapchain.rtvDescriptorSize);
	//}
}


BackendInfo BB::DX12CreateBackend(Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo)
{
	UINT t_FactoryFlags = 0;
	s_DX12BackendInst.CreatePools();

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
		"DX12: Failed to create DX12 memory allocator");

	D3D12_COMMAND_QUEUE_DESC t_QueueDesc{};
	t_QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	t_QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateCommandQueue(&t_QueueDesc,
		IID_PPV_ARGS(&s_DX12BackendInst.directpresentqueue)),
		"DX12: Failed to create direct command queue");

	SetupBackendSwapChain(a_CreateInfo.windowWidth, a_CreateInfo.windowHeight, a_CreateInfo.hwnd);

	//Create the two main heaps.
	s_DX12BackendInst.defaultHeap = 
		new (s_DX12BackendInst.Descriptorheaps.Get())DescriptorHeap(
			s_DX12BackendInst.device.logicalDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			4096,
			true);

	s_DX12BackendInst.uploadHeap = 
		new (s_DX12BackendInst.Descriptorheaps.Get())DescriptorHeap(
			s_DX12BackendInst.device.logicalDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			4096,
			false);


	//Returns some info to the global backend that is important.
	BackendInfo t_BackendInfo;
	t_BackendInfo.currentFrame = s_DX12BackendInst.currentFrame;
	t_BackendInfo.framebufferCount = s_DX12BackendInst.backBufferCount;

	return t_BackendInfo;
}

FrameBufferHandle BB::DX12CreateFrameBuffer(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo)
{
	DX12FrameBuffer frameBuffer{};

	D3D12_VIEWPORT t_Viewport{};
	D3D12_RECT t_SurfaceRect{};

	t_Viewport.TopLeftX = 0.0f;
	t_Viewport.TopLeftY = 0.0f;
	t_Viewport.Width = static_cast<float>(a_FramebufferCreateInfo.width);
	t_Viewport.Height = static_cast<float>(a_FramebufferCreateInfo.height);
	t_Viewport.MinDepth = .1f;
	t_Viewport.MaxDepth = 1000.f;

	t_SurfaceRect.left = 0;
	t_SurfaceRect.top = 0;
	t_SurfaceRect.right = static_cast<LONG>(a_FramebufferCreateInfo.width);
	t_SurfaceRect.bottom = static_cast<LONG>(a_FramebufferCreateInfo.height);

	frameBuffer.viewport = t_Viewport;
	frameBuffer.surfaceRect = t_SurfaceRect;

	const UINT t_IncrementSize = s_DX12BackendInst.device.logicalDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_DESCRIPTOR_HEAP_DESC t_RtvHeapDesc = {};
	t_RtvHeapDesc.NumDescriptors = s_DX12BackendInst.backBufferCount;
	t_RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; //RTV heaps are CPU only so the cost is not high.
	t_RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateDescriptorHeap(
		&t_RtvHeapDesc, IID_PPV_ARGS(&frameBuffer.rtvHeap)),
		"DX12: Failed to create descriptor heap for swapchain.");

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
		frameBuffer.rtvHeap->GetCPUDescriptorHandleForHeapStart();

	frameBuffer.renderTargets = BBnewArr(s_DX12Allocator, s_DX12BackendInst.backBufferCount, ID3D12Resource*);

	// Create a RTV for each frame.
	for (UINT i = 0; i < s_DX12BackendInst.backBufferCount; i++)
	{
		DXASSERT(s_DX12BackendInst.swapchain.swapchain->GetBuffer(i,
			IID_PPV_ARGS(&frameBuffer.renderTargets[i])),
			"DX12: Failed to get swapchain buffer.");

		s_DX12BackendInst.device.logicalDevice->CreateRenderTargetView(
			frameBuffer.renderTargets[i],
			nullptr,
			rtvHandle);
		rtvHandle.ptr += (1 * t_IncrementSize);
	}

	return FrameBufferHandle(s_DX12BackendInst.frameBuffers.insert(frameBuffer).handle);
}

RDescriptorHandle BB::DX12CreateDescriptor(Allocator a_TempAllocator, RDescriptorLayoutHandle& a_Layout, const RenderDescriptorCreateInfo& a_CreateInfo)
{
	if (a_Layout.ptrHandle != nullptr)
	{


		return RDescriptorHandle(a_Layout.ptrHandle);
	}

	ID3D12RootSignature* t_RootSignature = nullptr;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE t_FeatureData = {};
	t_FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(s_DX12BackendInst.device.logicalDevice->CheckFeatureSupport(
		D3D12_FEATURE_ROOT_SIGNATURE,
		&t_FeatureData, sizeof(t_FeatureData))))
	{
		t_FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	D3D12_ROOT_CONSTANTS* t_RootConstants = BBnewArr(
		a_TempAllocator,
		a_CreateInfo.constantBinds.size(),
		D3D12_ROOT_CONSTANTS);

	UINT t_RegisterSpace = 0;
	for (size_t i = 0; i < a_CreateInfo.constantBinds.size(); i++)
	{
		BB_ASSERT(a_CreateInfo.constantBinds[i].size % sizeof(uint32_t) == 0, "DX12: BindConstant a_size is not a multiple of 32!");
		const UINT t_Dwords = a_CreateInfo.constantBinds[i].size / sizeof(uint32_t);
		t_RootConstants[i].Num32BitValues = t_Dwords;
		t_RootConstants[i].ShaderRegister = t_RegisterSpace++;
		t_RootConstants[i].RegisterSpace = 0; //We will just keep this 0 for now.
	}


	D3D12_DESCRIPTOR_RANGE1* t_DescRanges = BBnewArr(
		a_TempAllocator,
		a_CreateInfo.bufferBinds.size(),
		D3D12_DESCRIPTOR_RANGE1);

	t_RegisterSpace = 0;
	for (size_t i = 0; i < a_CreateInfo.bufferBinds.size(); i++)
	{
		t_DescRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		t_DescRanges[i].RegisterSpace = 0; //We will keep this 0 for now.
		t_DescRanges[i].NumDescriptors = 1;
		t_DescRanges[i].OffsetInDescriptorsFromTableStart = i; //Set the descriptor handles to here!
		t_DescRanges[i].BaseShaderRegister = t_RegisterSpace++;
		t_DescRanges[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
	}

	//Groups of GPU Resources
	D3D12_ROOT_PARAMETER1 t_RootParameters[2]{};
	t_RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	t_RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; //This is for the indices so make it visible to all.
	t_RootParameters[0].DescriptorTable.NumDescriptorRanges = a_CreateInfo.bufferBinds.size();
	t_RootParameters[0].DescriptorTable.pDescriptorRanges = t_DescRanges;

	t_RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	t_RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; //This is for the indices so make it visible to all.
	t_RootParameters[1].Constants = t_RootConstants[0];

	//Overall Layout
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC t_RootSignatureDesc;
	t_RootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	t_RootSignatureDesc.Desc_1_1.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	t_RootSignatureDesc.Desc_1_1.NumParameters = 2;
	t_RootSignatureDesc.Desc_1_1.pParameters = t_RootParameters;
	t_RootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
	t_RootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

	ID3DBlob* t_Signature;
	ID3DBlob* t_Error;

	D3D12SerializeVersionedRootSignature(&t_RootSignatureDesc,
		&t_Signature, &t_Error);

	if (t_Error != nullptr)
	{
		BB_LOG((const char*)t_Error->GetBufferPointer());
		BB_ASSERT(false, "DX12: error creating root signature, details are above.");
		t_Error->Release();
	}

	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateRootSignature(0,
		t_Signature->GetBufferPointer(),
		t_Signature->GetBufferSize(),
		IID_PPV_ARGS(&t_RootSignature)),
		"DX12: Failed to create root signature.");

	t_RootSignature->SetName(L"Hello Triangle Root Signature");

	if (t_Signature != nullptr)
		t_Signature->Release();

	a_Layout.ptrHandle = t_RootSignature;
	return RDescriptorHandle(t_RootSignature);
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

	t_PsoDesc.pRootSignature = reinterpret_cast<ID3D12RootSignature*>(a_CreateInfo.descLayoutHandles[0].ptrHandle);

	for (size_t i = 0; i < a_CreateInfo.shaderCreateInfos.size(); i++)
	{
		switch (a_CreateInfo.shaderCreateInfos[i].shaderStage)
		{
		case RENDER_SHADER_STAGE::VERTEX:
			t_PsoDesc.VS.BytecodeLength = a_CreateInfo.shaderCreateInfos[i].buffer.size;
			t_PsoDesc.VS.pShaderBytecode = a_CreateInfo.shaderCreateInfos[i].buffer.data;
			break;
		case RENDER_SHADER_STAGE::FRAGMENT_PIXEL:
			t_PsoDesc.PS.BytecodeLength = a_CreateInfo.shaderCreateInfos[i].buffer.size;
			t_PsoDesc.PS.pShaderBytecode = a_CreateInfo.shaderCreateInfos[i].buffer.data;
			break;
		default:
			BB_ASSERT(false, "DX12: unsupported shaderstage.")
			break;
		}
	}

	D3D12_RASTERIZER_DESC t_RasterDesc;
	t_RasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
	t_RasterDesc.CullMode = D3D12_CULL_MODE_NONE;
	t_RasterDesc.FrontCounterClockwise = FALSE;
	t_RasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	t_RasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	t_RasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	t_RasterDesc.DepthClipEnable = TRUE;
	t_RasterDesc.MultisampleEnable = FALSE;
	t_RasterDesc.AntialiasedLineEnable = FALSE;
	t_RasterDesc.ForcedSampleCount = 0;
	t_RasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	t_PsoDesc.RasterizerState = t_RasterDesc;
	t_PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	D3D12_BLEND_DESC t_BlendDesc;
	t_BlendDesc.AlphaToCoverageEnable = FALSE;
	t_BlendDesc.IndependentBlendEnable = FALSE;
	const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
		FALSE,
		FALSE,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		t_BlendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
	t_PsoDesc.BlendState = t_BlendDesc;

	t_PsoDesc.DepthStencilState.DepthEnable = FALSE;
	t_PsoDesc.DepthStencilState.StencilEnable = FALSE;
	t_PsoDesc.SampleMask = UINT_MAX;

	t_PsoDesc.NumRenderTargets = 1;
	t_PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	t_PsoDesc.SampleDesc.Count = 1;

	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateGraphicsPipelineState(
		&t_PsoDesc, IID_PPV_ARGS(&t_PipelineState)),
		"DX12: Failed to create graphics pipeline");

	return PipelineHandle(s_DX12BackendInst.pipelines.emplace(t_PipelineState).handle);
}

CommandQueueHandle BB::DX12CreateCommandQueue(const RenderCommandQueueCreateInfo& a_Info)
{
	switch (a_Info.queue)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		return CommandQueueHandle(new (s_DX12BackendInst.cmdQueues.Get())
			DXCommandQueue(s_DX12BackendInst.device.logicalDevice, s_DX12BackendInst.directpresentqueue));
		break;
	case RENDER_QUEUE_TYPE::TRANSFER_COPY:
		return CommandQueueHandle(new (s_DX12BackendInst.cmdQueues.Get())
			DXCommandQueue(s_DX12BackendInst.device.logicalDevice, D3D12_COMMAND_LIST_TYPE_COPY));
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
		return CommandQueueHandle(new (s_DX12BackendInst.cmdQueues.Get())
			DXCommandQueue(s_DX12BackendInst.device.logicalDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE));
		break;
	default:
		BB_ASSERT(false, "DX12: Tried to make a command queue with a queue type that does not exist.");
		return CommandQueueHandle(new (s_DX12BackendInst.cmdQueues.Get())
			DXCommandQueue(s_DX12BackendInst.device.logicalDevice, s_DX12BackendInst.directpresentqueue));
		break;
	}
}

CommandAllocatorHandle BB::DX12CreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo)
{
	//Create the command allocator and it's command lists.
	DXCommandAllocator* t_CmdAllocator = new (s_DX12BackendInst.cmdAllocators.Get())
		DXCommandAllocator(s_DX12BackendInst.device.logicalDevice,
			DXConv::CommandListType(a_CreateInfo.queueType),
			a_CreateInfo.commandListCount);
	return CommandAllocatorHandle(t_CmdAllocator);
}

CommandListHandle BB::DX12CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo)
{
	return CommandListHandle(reinterpret_cast<DXCommandAllocator*>(a_CreateInfo.commandAllocator.ptrHandle)->GetCommandList());
}

RBufferHandle BB::DX12CreateBuffer(const RenderBufferCreateInfo& a_Info)
{
	DXMAResource* t_Resource = s_DX12BackendInst.renderResources.Get();

	D3D12_RESOURCE_DESC t_ResourceDesc = {};
	t_ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	t_ResourceDesc.Alignment = 0;
	t_ResourceDesc.Width = a_Info.size;
	t_ResourceDesc.Height = 1;
	t_ResourceDesc.DepthOrArraySize = 1;
	t_ResourceDesc.MipLevels = 1;
	t_ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	t_ResourceDesc.SampleDesc.Count = 1;
	t_ResourceDesc.SampleDesc.Quality = 0;
	t_ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	t_ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	D3D12MA::ALLOCATION_DESC t_AllocationDesc = {};
	//t_AllocationDesc.HeapType = DXConv::HeapType(a_Info.memProperties);
	t_AllocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_STATES t_States = D3D12_RESOURCE_STATE_COPY_DEST;
	//if (a_Info.usage == RENDER_BUFFER_USAGE::STAGING)
		t_States = D3D12_RESOURCE_STATE_GENERIC_READ;
	DXASSERT(s_DX12BackendInst.DXMA->CreateResource(
		&t_AllocationDesc,
		&t_ResourceDesc,
		t_States,
		NULL,
		&t_Resource->allocation,
		IID_PPV_ARGS(&t_Resource->resource)),
		"DX12: Failed to create resource using D3D12 Memory Allocator");

	switch (a_Info.usage)
	{
	case RENDER_BUFFER_USAGE::VERTEX:
		t_Resource->view.vertexView.BufferLocation = t_Resource->resource->GetGPUVirtualAddress();
		t_Resource->view.vertexView.StrideInBytes = sizeof(Vertex);
		t_Resource->view.vertexView.SizeInBytes = static_cast<UINT>(a_Info.size);
		break;
	case RENDER_BUFFER_USAGE::INDEX:
		t_Resource->view.indexView.BufferLocation = t_Resource->resource->GetGPUVirtualAddress();
		t_Resource->view.indexView.Format = DXGI_FORMAT_R32_UINT;
		t_Resource->view.indexView.SizeInBytes = static_cast<UINT>(a_Info.size);
		break;
	default:
		t_Resource->view.constantView.BufferLocation = t_Resource->resource->GetGPUVirtualAddress();
		t_Resource->view.constantView.SizeInBytes = static_cast<UINT>(a_Info.size);
		break;
	}

	if (a_Info.data != nullptr)
	{
		void* t_MappedPtr;
		D3D12_RANGE t_ReadRange;
		t_ReadRange.Begin = 0;
		t_ReadRange.End = 0;

		DXASSERT(t_Resource->resource->Map(0, nullptr, &t_MappedPtr),
			"DX12: failed to map memory to resource.");
		memcpy(t_MappedPtr, a_Info.data, a_Info.size);
		t_Resource->resource->Unmap(0, nullptr);
	}

	return RBufferHandle(t_Resource);
}

RFenceHandle BB::DX12CreateFence(const FenceCreateInfo& a_Info)
{
	ID3D12Fence* t_Fence = s_DX12BackendInst.fencePool.Get();
	s_DX12BackendInst.device.logicalDevice->CreateFence(0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&t_Fence));
	return RFenceHandle(t_Fence);
}


void BB::DX12ResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle)
{
	reinterpret_cast<DXCommandAllocator*>(a_CmdAllocatorHandle.ptrHandle)->ResetCommandAllocator();
}

RecordingCommandListHandle BB::DX12StartCommandList(const CommandListHandle a_CmdHandle)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_CmdHandle.ptrHandle);
	t_CommandList->Reset();
	return RecordingCommandListHandle(t_CommandList);
}

void BB::DX12EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	t_CommandList->rtv = nullptr;
	//Close it for now
	t_CommandList->Close();
}

void BB::DX12StartRenderPass(const RecordingCommandListHandle a_RecordingCmdHandle, const FrameBufferHandle a_Framebuffer)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	DX12FrameBuffer& t_Framebuffer = s_DX12BackendInst.frameBuffers.find(a_Framebuffer.handle);

	t_CommandList->rtv = t_Framebuffer.renderTargets[s_DX12BackendInst.currentFrame];

	D3D12_RESOURCE_BARRIER t_RenderTargetBarrier;
	t_RenderTargetBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	t_RenderTargetBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	t_RenderTargetBarrier.Transition.pResource = t_CommandList->rtv;
	t_RenderTargetBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	t_RenderTargetBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	t_RenderTargetBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	t_CommandList->List()->ResourceBarrier(1, &t_RenderTargetBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE
		rtvHandle(t_Framebuffer.rtvHeap->GetCPUDescriptorHandleForHeapStart());
	rtvHandle.ptr = rtvHandle.ptr + (s_DX12BackendInst.currentFrame * 
		s_DX12BackendInst.device.logicalDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	
	t_CommandList->List()->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
	const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };

	t_CommandList->List()->RSSetViewports(1, &t_Framebuffer.viewport);
	t_CommandList->List()->RSSetScissorRects(1, &t_Framebuffer.surfaceRect);
	t_CommandList->List()->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

void BB::DX12EndRenderPass(const RecordingCommandListHandle a_RecordingCmdHandle)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	//// Indicate that the back buffer will now be used to present.
	D3D12_RESOURCE_BARRIER presentBarrier;
	presentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	presentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	presentBarrier.Transition.pResource = t_CommandList->rtv;
	presentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	presentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	presentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	t_CommandList->List()->ResourceBarrier(1, &presentBarrier);
}

void BB::DX12BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	t_CommandList->List()->SetPipelineState(s_DX12BackendInst.pipelines.find(a_Pipeline.handle));
}

void BB::DX12BindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	D3D12_VERTEX_BUFFER_VIEW t_Views[12]{};
	for (size_t i = 0; i < a_BufferCount; i++)
	{
		t_Views[i] = reinterpret_cast<DXMAResource*>(a_Buffers[i].ptrHandle)->view.vertexView;
	}

	t_CommandList->List()->IASetVertexBuffers(0, static_cast<uint32_t>(a_BufferCount), t_Views);
}

void BB::DX12BindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	t_CommandList->List()->IASetIndexBuffer(&reinterpret_cast<DXMAResource*>(a_Buffer.ptrHandle)->view.indexView);
}


void BB::DX12BindDescriptorSets(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_FirstSet, const uint32_t a_SetCount, const RDescriptorHandle* a_Sets, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	
	t_CommandList->List()->SetGraphicsRootSignature(reinterpret_cast<ID3D12RootSignature*>(a_Sets[0].ptrHandle));
}

void BB::DX12BindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const RENDER_SHADER_STAGE a_Stage, const uint32_t a_Offset, const uint32_t a_Size, const void* a_Data)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	BB_ASSERT(a_Size % sizeof(uint32_t) == 0, "DX12: BindConstant a_size is not a multiple of 32!");
	const UINT t_Dwords = a_Size / sizeof(uint32_t);
	t_CommandList->List()->SetGraphicsRoot32BitConstants(0, t_Dwords, a_Data, a_Offset);
}

void BB::DX12DrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	t_CommandList->List()->DrawInstanced(a_VertexCount, a_InstanceCount, a_FirstInstance, a_InstanceCount);
}

void BB::DX12DrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	t_CommandList->List()->DrawIndexedInstanced(a_IndexCount, a_InstanceCount, a_FirstIndex, a_VertexOffset, a_FirstInstance);
}


void BB::DX12BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset)
{
	DXMAResource* t_Resource = reinterpret_cast<DXMAResource*>(a_Handle.ptrHandle);
	void* t_MapData;

	DXASSERT(t_Resource->resource->Map(0, NULL, &t_MapData),
		"DX12: Failed to map resource.");

	memcpy(Pointer::Add(t_MapData, a_Offset), a_Data, a_Size);

	t_Resource->resource->Unmap(0, NULL);
}

void BB::DX12CopyBuffer(Allocator a_TempAllocator, const RenderCopyBufferInfo& a_CopyInfo)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_CopyInfo.transferCommandHandle.ptrHandle);

	ID3D12Resource* t_DestResource = reinterpret_cast<DXMAResource*>(a_CopyInfo.dst.ptrHandle)->resource;

	for (size_t i = 0; i < a_CopyInfo.CopyRegionCount; i++)
	{
		t_CommandList->List()->CopyBufferRegion(
			t_DestResource,
			a_CopyInfo.copyRegions[i].dstOffset,
			reinterpret_cast<DXMAResource*>(a_CopyInfo.src.handle)->resource,
			a_CopyInfo.copyRegions[i].srcOffset,
			a_CopyInfo.copyRegions[i].size);
	}
}

struct RenderBarriersInfo
{
	struct ResourceBarriers
	{
		RENDER_BUFFER_USAGE previous;
		RENDER_BUFFER_USAGE next;
		RBufferHandle buffer;
		uint32_t subresource;
	};
	ResourceBarriers* barriers;
	uint32_t barrierCount;
};

void DX12ResourceBarrier(Allocator a_TempAllocator, const RecordingCommandListHandle a_RecordingCmdHandle, const RenderBarriersInfo& a_Info)
{
	D3D12_RESOURCE_BARRIER* t_ResourceBarriers = BBnewArr(a_TempAllocator, 
		a_Info.barrierCount,
		D3D12_RESOURCE_BARRIER);

	for (size_t i = 0; i < a_Info.barrierCount; i++)
	{
		t_ResourceBarriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		t_ResourceBarriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		t_ResourceBarriers[i].Transition.pResource = reinterpret_cast<DXMAResource*>(a_Info.barriers[i].buffer.ptrHandle)->resource;
		t_ResourceBarriers[i].Transition.StateBefore = DXConv::ResourceStates(a_Info.barriers[i].previous);
		t_ResourceBarriers[i].Transition.StateAfter = DXConv::ResourceStates(a_Info.barriers[i].next);
		t_ResourceBarriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	}

	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	t_CommandList->List()->ResourceBarrier(a_Info.barrierCount, t_ResourceBarriers);
}


void* BB::DX12MapMemory(const RBufferHandle a_Handle)
{
	DXMAResource* t_Resource = reinterpret_cast<DXMAResource*>(a_Handle.ptrHandle);
	void* t_MapData;
	DXASSERT(t_Resource->resource->Map(0, NULL, &t_MapData),
		"DX12: Failed to map resource.");
	return t_MapData;
}

void BB::DX12UnMemory(const RBufferHandle a_Handle)
{
	DXMAResource* t_Resource = reinterpret_cast<DXMAResource*>(a_Handle.ptrHandle);
	t_Resource->resource->Unmap(0, NULL);
}

void BB::DX12StartFrame(Allocator a_TempAllocator, const StartFrameInfo& a_StartInfo)
{

}

void BB::DX12ExecuteCommands(Allocator a_TempAllocator, CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount)
{
	for (size_t i = 0; i < a_ExecuteInfoCount; i++)
	{
		ID3D12CommandList** t_CommandLists = BBnewArr(
			a_TempAllocator,
			a_ExecuteInfos[i].commandCount,
			ID3D12CommandList*
		);

		for (size_t j = 0; j < a_ExecuteInfos[i].commandCount; j++)
		{
			t_CommandLists[j] = reinterpret_cast<DXCommandList*>(
				a_ExecuteInfos[i].commands[j].ptrHandle)->List();
		}

		reinterpret_cast<DXCommandQueue*>(a_ExecuteQueue.ptrHandle)->ExecuteCommandlist(
			t_CommandLists,
			a_ExecuteInfos[i].commandCount);

		//Now reset the command lists.
		for (size_t j = 0; j < a_ExecuteInfos[i].commandCount; j++)
		{
			reinterpret_cast<DXCommandList*>(a_ExecuteInfos[i].commands[j].ptrHandle)->Reset();
		}
	}
}

//Special execute commands, not sure if DX12 needs anything special yet.
void BB::DX12ExecutePresentCommand(Allocator a_TempAllocator, CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo)
{
	ID3D12CommandList** t_CommandLists = BBnewArr(
		a_TempAllocator,
		a_ExecuteInfo.commandCount,
		ID3D12CommandList*
	);

	for (size_t j = 0; j < a_ExecuteInfo.commandCount; j++)
	{
		t_CommandLists[j] = reinterpret_cast<DXCommandList*>(
			a_ExecuteInfo.commands[j].ptrHandle)->List();
	}

	reinterpret_cast<DXCommandQueue*>(a_ExecuteQueue.ptrHandle)->ExecuteCommandlist(
		t_CommandLists,
		a_ExecuteInfo.commandCount);

	//Now reset the command lists.
	for (size_t j = 0; j < a_ExecuteInfo.commandCount; j++)
	{
		reinterpret_cast<DXCommandList*>(a_ExecuteInfo.commands[j].ptrHandle)->Reset();
	}
}

FrameIndex BB::DX12PresentFrame(Allocator a_TempAllocator, const PresentFrameInfo& a_PresentInfo)
{
	s_DX12BackendInst.swapchain.swapchain->Present(1, 0);
	s_DX12BackendInst.currentFrame = s_DX12BackendInst.swapchain.swapchain->GetCurrentBackBufferIndex();

	return s_DX12BackendInst.currentFrame;
}

void BB::DX12WaitDeviceReady()
{
	//const UINT64 fenceV = s_DX12BackendInst.fenceValue;
	BB_WARNING(false, "DX12: DX12WaitDeviceReady function is unfinished, using it is dangerous.", WarningType::MEDIUM);
	//if (s_DX12BackendInst.fence->GetCompletedValue() < fenceV)
	//{
	//	DXASSERT(s_DX12BackendInst.fence->SetEventOnCompletion(fenceV, s_DX12BackendInst.fenceEvent),
	//		"DX12: Failed to wait for event complection on fence.");
	//	WaitForSingleObject(s_DX12BackendInst.fenceEvent, INFINITE);
	//}
}

void BB::DX12DestroyFence(const RFenceHandle a_Handle)
{
	DXRelease(reinterpret_cast<ID3D12Fence*>(a_Handle.ptrHandle));
	s_DX12BackendInst.fencePool.Free(reinterpret_cast<ID3D12Fence*>(a_Handle.ptrHandle));
}

void BB::DX12DestroyBuffer(const RBufferHandle a_Handle)
{
	DXMAResource* t_Resource = reinterpret_cast<DXMAResource*>(a_Handle.ptrHandle);
	t_Resource->resource->Release();
	t_Resource->allocation->Release();
	s_DX12BackendInst.renderResources.Free(t_Resource);
}

void BB::DX12DestroyCommandQueue(const CommandQueueHandle a_Handle)
{
	reinterpret_cast<DXCommandQueue*>(a_Handle.ptrHandle)->~DXCommandQueue();
}

void BB::DX12DestroyCommandAllocator(const CommandAllocatorHandle a_Handle)
{
	//Deconstructing the command allocator.
	reinterpret_cast<DXCommandAllocator*>(a_Handle.ptrHandle)->~DXCommandAllocator();
}

void BB::DX12DestroyCommandList(const CommandListHandle a_Handle)
{
	reinterpret_cast<DXCommandList*>(a_Handle.ptrHandle)->Free();
}

void BB::DX12DestroyPipeline(const PipelineHandle a_Handle)
{
	s_DX12BackendInst.pipelines.find(a_Handle.handle)->Release();
	s_DX12BackendInst.pipelines.erase(a_Handle.handle);
}

void BB::DX12DestroyFramebuffer(const FrameBufferHandle a_Handle)
{
	DX12FrameBuffer t_FrameBuffer = s_DX12BackendInst.frameBuffers.find(a_Handle.handle);
	
	for (size_t i = 0; i < s_DX12BackendInst.backBufferCount; i++)
	{
		t_FrameBuffer.renderTargets[i]->Release();
	}
	BBfreeArr(s_DX12Allocator, t_FrameBuffer.renderTargets);
	t_FrameBuffer.rtvHeap->Release();

	s_DX12BackendInst.frameBuffers.erase(a_Handle.handle);
}

void BB::DX12DestroyDescriptorSetLayout(const RDescriptorLayoutHandle a_Handle)
{
	reinterpret_cast<ID3D12RootSignature*>(a_Handle.ptrHandle)->Release();
}

void BB::DX12DestroyDescriptorSet(const RDescriptorHandle a_Handle)
{

}

void BB::DX12DestroyBackend()
{
	s_DX12BackendInst.swapchain.swapchain->SetFullscreenState(false, NULL);
	s_DX12BackendInst.swapchain.swapchain->Release();
	s_DX12BackendInst.swapchain.swapchain = nullptr;

	s_DX12BackendInst.DXMA->Release();
	if (s_DX12BackendInst.device.debugDevice)
	{
		s_DX12BackendInst.device.debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_NONE);
		s_DX12BackendInst.device.debugDevice->Release();
	}


	s_DX12BackendInst.device.logicalDevice->Release();
	s_DX12BackendInst.device.adapter->Release();
	if (s_DX12BackendInst.debugController)
		s_DX12BackendInst.debugController->Release();

	s_DX12BackendInst.factory->Release();
}
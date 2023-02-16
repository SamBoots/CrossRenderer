#include "DX12Common.h"
#include "DX12HelperTypes.h"

#include "Slotmap.h"
#include "Pool.h"
#include "BBString.h"

#include "TemporaryAllocator.h"

//Tutorial used for this DX12 backend was https://alain.xyz/blog/raw-directx12 

using namespace BB;

struct PipelineBuildInfo
{
	//temporary allocator, this gets removed when we are finished building.
	TemporaryAllocator buildAllocator{ s_DX12Allocator };
	DXPipeline buildPipeline;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSOdesc{};
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc{};

	UINT regCBV = 0;
	UINT regSRV = 0;
	UINT regUAV = 0;

	uint32_t rootParameterCount = 0;
	//Use this as if it always picks constants
	D3D12_ROOT_PARAMETER1* rootConstants{};
	uint32_t rootConstantCount = 0;

	//Use this as if it always picks root descriptors
	D3D12_ROOT_PARAMETER1* rootDescriptors{};
	uint32_t rootDescriptorCount = 0;
};

struct DX12Backend_inst
{
	FrameIndex currentFrame = 0;
	UINT backBufferCount = 3; //for now hardcode 3 backbuffers.
	DXFence* frameFences; //Equal amount of fences to backBufferCount.

	IDXGIFactory4* factory{};
	ID3D12Debug1* debugController{};

	DescriptorHeap* defaultHeap;
	DescriptorHeap* uploadHeap;

	DX12Device device{};
	DX12Swapchain swapchain{};

	D3D12MA::Allocator* DXMA;
	ID3D12CommandQueue* directpresentqueue;

	Slotmap<DX12FrameBuffer> frameBuffers{ s_DX12Allocator };

	Pool<DescriptorHeap> Descriptorheaps;
	Pool<DXPipeline> pipelinePool;
	Pool<DXCommandQueue> cmdQueues;
	Pool<DXCommandAllocator> cmdAllocators;
	Pool<DXResource> renderResources;
	Pool<DXFence> fencePool;

	void CreatePools()
	{
		Descriptorheaps.CreatePool(s_DX12Allocator, 16);
		pipelinePool.CreatePool(s_DX12Allocator, 4);
		cmdQueues.CreatePool(s_DX12Allocator, 4);
		cmdAllocators.CreatePool(s_DX12Allocator, 16);
		renderResources.CreatePool(s_DX12Allocator, 8);
		fencePool.CreatePool(s_DX12Allocator, 16);
	}

	void DestroyPools()
	{
		Descriptorheaps.DestroyPool(s_DX12Allocator);
		pipelinePool.DestroyPool(s_DX12Allocator);
		cmdQueues.DestroyPool(s_DX12Allocator);
		cmdAllocators.DestroyPool(s_DX12Allocator);
		renderResources.DestroyPool(s_DX12Allocator);
		fencePool.DestroyPool(s_DX12Allocator);
	}
};

static DX12Backend_inst s_DX12B;

constexpr uint64_t COMMAND_BUFFER_STANDARD_COUNT = 32;

enum class ShaderType
{
	VERTEX,
	PIXEL
};

static void SetupBackendSwapChain(UINT a_Width, UINT a_Height, HWND a_WindowHandle)
{
	s_DX12B.swapchain.width = a_Width;
	s_DX12B.swapchain.height = a_Height;

	//Just do a resize if a swapchain already exists.
	if (s_DX12B.swapchain.swapchain != nullptr)
	{
		s_DX12B.swapchain.swapchain->ResizeBuffers(s_DX12B.backBufferCount,
			a_Width,
			a_Height,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0);
		return;
	}

	DXGI_SWAP_CHAIN_DESC1 t_SwapchainDesc = {};
	t_SwapchainDesc.BufferCount = s_DX12B.backBufferCount;
	t_SwapchainDesc.Width = a_Width;
	t_SwapchainDesc.Height = a_Height;
	t_SwapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	t_SwapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	t_SwapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	t_SwapchainDesc.SampleDesc.Count = 1;

	IDXGISwapChain1* t_NewSwapchain;
	DXASSERT(s_DX12B.factory->CreateSwapChainForHwnd(
		s_DX12B.directpresentqueue,
		a_WindowHandle,
		&t_SwapchainDesc,
		nullptr,
		nullptr,
		&t_NewSwapchain),
		"DX12: Failed to create swapchain1");

	DXASSERT(s_DX12B.factory->MakeWindowAssociation(a_WindowHandle,
		DXGI_MWA_NO_ALT_ENTER),
		"DX12: Failed to add DXGI_MWA_NO_ALT_ENTER to window.");

	DXASSERT(t_NewSwapchain->QueryInterface(
		__uuidof(IDXGISwapChain3), (void**)&t_NewSwapchain),
		"DX12: Failed to get support for a IDXGISwapchain3.");

	s_DX12B.swapchain.swapchain = (IDXGISwapChain3*)t_NewSwapchain;

	s_DX12B.currentFrame = s_DX12B.swapchain.swapchain->GetCurrentBackBufferIndex();
}


BackendInfo BB::DX12CreateBackend(Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo)
{
	UINT t_FactoryFlags = 0;
	s_DX12B.CreatePools();

	if (a_CreateInfo.validationLayers)
	{
		ID3D12Debug* t_DebugController;
		DXASSERT(D3D12GetDebugInterface(IID_PPV_ARGS(&t_DebugController)),
			"DX12: failed to create debuginterface.");
		DXASSERT(t_DebugController->QueryInterface(IID_PPV_ARGS(&s_DX12B.debugController)),
			"DX12: failed to create debuginterface.");
		s_DX12B.debugController->EnableDebugLayer();
		s_DX12B.debugController->SetEnableGPUBasedValidation(true);

		t_FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		t_DebugController->Release();
	}
	else
	{
		s_DX12B.debugController = nullptr;
	}

	DXASSERT(CreateDXGIFactory2(t_FactoryFlags, IID_PPV_ARGS(&s_DX12B.factory)),
		"DX12: failed to create DXGIFactory2.");

#pragma region DEVICE_CREATION
	IDXGIAdapter1* t_CurrentBestAdapter = nullptr;
	SIZE_T t_BestDedicatedVRAM = 0;

	for (UINT adapterIndex = 0;
		DXGI_ERROR_NOT_FOUND != s_DX12B.factory->EnumAdapters1(adapterIndex, &s_DX12B.device.adapter);
		++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 t_Desc;
		s_DX12B.device.adapter->GetDesc1(&t_Desc);

		//We don't take the software adapter.
		if (t_Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (t_Desc.DedicatedVideoMemory > t_BestDedicatedVRAM)
		{
			t_BestDedicatedVRAM = t_Desc.DedicatedVideoMemory;
			t_CurrentBestAdapter = s_DX12B.device.adapter;
		}
	}
	s_DX12B.device.adapter = t_CurrentBestAdapter;

	DXASSERT(D3D12CreateDevice(s_DX12B.device.adapter,
		D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(&s_DX12B.device.logicalDevice)),
		"DX12: Failed to create logical device.");

	if (a_CreateInfo.validationLayers)
	{
		DXASSERT(s_DX12B.device.logicalDevice->QueryInterface(&s_DX12B.device.debugDevice),
			"DX12: Failed to query debug device.");
	}
	else
	{
		s_DX12B.device.debugDevice = nullptr;
	}
#pragma endregion //DEVICE_CREATION

	D3D12MA::ALLOCATOR_DESC t_AllocatorDesc = {};
	t_AllocatorDesc.pDevice = s_DX12B.device.logicalDevice;
	t_AllocatorDesc.pAdapter = s_DX12B.device.adapter;

	DXASSERT(D3D12MA::CreateAllocator(&t_AllocatorDesc, &s_DX12B.DXMA),
		"DX12: Failed to create DX12 memory allocator");

	D3D12_COMMAND_QUEUE_DESC t_QueueDesc{};
	t_QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	t_QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DXASSERT(s_DX12B.device.logicalDevice->CreateCommandQueue(&t_QueueDesc,
		IID_PPV_ARGS(&s_DX12B.directpresentqueue)),
		"DX12: Failed to create direct command queue");

	SetupBackendSwapChain(a_CreateInfo.windowWidth, a_CreateInfo.windowHeight, a_CreateInfo.hwnd);

	//Create the two main heaps.
	s_DX12B.defaultHeap = 
		new (s_DX12B.Descriptorheaps.Get())DescriptorHeap(
			s_DX12B.device.logicalDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			4096,
			true);

	s_DX12B.uploadHeap = 
		new (s_DX12B.Descriptorheaps.Get())DescriptorHeap(
			s_DX12B.device.logicalDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			4096,
			false);


	s_DX12B.frameFences = BBnewArr(s_DX12Allocator,
		s_DX12B.backBufferCount,
		DXFence);

	for (size_t i = 0; i < s_DX12B.backBufferCount; i++)
	{
		new (&s_DX12B.frameFences[i]) DXFence(s_DX12B.device.logicalDevice);
	}

	//Returns some info to the global backend that is important.
	BackendInfo t_BackendInfo;
	t_BackendInfo.currentFrame = s_DX12B.currentFrame;
	t_BackendInfo.framebufferCount = s_DX12B.backBufferCount;

	return t_BackendInfo;
}

FrameBufferHandle BB::DX12CreateFrameBuffer(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo)
{
	DX12FrameBuffer frameBuffer{};

	D3D12_VIEWPORT t_Viewport{};
	D3D12_RECT t_SurfaceRect{};

	for (size_t i = 0; i < 4; i++)
	{
		frameBuffer.clearColor[i] = a_FramebufferCreateInfo.clearColor[i];
	}

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

	const UINT t_IncrementSize = s_DX12B.device.logicalDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_DESCRIPTOR_HEAP_DESC t_RtvHeapDesc = {};
	t_RtvHeapDesc.NumDescriptors = s_DX12B.backBufferCount;
	t_RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; //RTV heaps are CPU only so the cost is not high.
	t_RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DXASSERT(s_DX12B.device.logicalDevice->CreateDescriptorHeap(
		&t_RtvHeapDesc, IID_PPV_ARGS(&frameBuffer.rtvHeap)),
		"DX12: Failed to create descriptor heap for swapchain.");

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
		frameBuffer.rtvHeap->GetCPUDescriptorHandleForHeapStart();

	frameBuffer.renderTargets = BBnewArr(s_DX12Allocator, s_DX12B.backBufferCount, ID3D12Resource*);

	// Create a RTV for each frame.
	for (UINT i = 0; i < s_DX12B.backBufferCount; i++)
	{
		DXASSERT(s_DX12B.swapchain.swapchain->GetBuffer(i,
			IID_PPV_ARGS(&frameBuffer.renderTargets[i])),
			"DX12: Failed to get swapchain buffer.");

		s_DX12B.device.logicalDevice->CreateRenderTargetView(
			frameBuffer.renderTargets[i],
			nullptr,
			rtvHandle);
		rtvHandle.ptr += static_cast<uintptr_t>(1 * t_IncrementSize);
	}

	return FrameBufferHandle(s_DX12B.frameBuffers.insert(frameBuffer).handle);
}

CommandQueueHandle BB::DX12CreateCommandQueue(const RenderCommandQueueCreateInfo& a_Info)
{
	switch (a_Info.queue)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		return CommandQueueHandle(new (s_DX12B.cmdQueues.Get())
			DXCommandQueue(s_DX12B.device.logicalDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, s_DX12B.directpresentqueue));
		break;
	case RENDER_QUEUE_TYPE::TRANSFER_COPY:
		return CommandQueueHandle(new (s_DX12B.cmdQueues.Get())
			DXCommandQueue(s_DX12B.device.logicalDevice, D3D12_COMMAND_LIST_TYPE_COPY));
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
		return CommandQueueHandle(new (s_DX12B.cmdQueues.Get())
			DXCommandQueue(s_DX12B.device.logicalDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE));
		break;
	default:
		BB_ASSERT(false, "DX12: Tried to make a command queue with a queue type that does not exist.");
		return CommandQueueHandle(new (s_DX12B.cmdQueues.Get())
			DXCommandQueue(s_DX12B.device.logicalDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, s_DX12B.directpresentqueue));
		break;
	}
}

CommandAllocatorHandle BB::DX12CreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo)
{
	//Create the command allocator and it's command lists.
	DXCommandAllocator* t_CmdAllocator = new (s_DX12B.cmdAllocators.Get())
		DXCommandAllocator(s_DX12B.device.logicalDevice,
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
	DXResource* t_Resource = new (s_DX12B.renderResources.Get())
		DXResource(s_DX12B.DXMA, a_Info.usage, a_Info.memProperties, a_Info.size);

	if (a_Info.data != nullptr)
	{
		void* t_MappedPtr;
		D3D12_RANGE t_ReadRange{};
		t_ReadRange.Begin = 0;
		t_ReadRange.End = 0;

		DXASSERT(t_Resource->GetResource()->Map(0, nullptr, &t_MappedPtr),
			"DX12: failed to map memory to resource.");
		memcpy(t_MappedPtr, a_Info.data, a_Info.size);
		t_Resource->GetResource()->Unmap(0, nullptr);
	}

	return RBufferHandle(t_Resource);
}

RFenceHandle BB::DX12CreateFence(const FenceCreateInfo& a_Info)
{
	return RFenceHandle(new (s_DX12B.fencePool.Get()) DXFence(s_DX12B.device.logicalDevice));
}

//PipelineBuilder
PipelineBuilderHandle BB::DX12PipelineBuilderInit(const FrameBufferHandle a_Handle)
{
	PipelineBuildInfo* t_BuildInfo = BBnew(s_DX12Allocator, PipelineBuildInfo);

	D3D12_FEATURE_DATA_ROOT_SIGNATURE t_FeatureData = {};
	t_FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(s_DX12B.device.logicalDevice->CheckFeatureSupport(
		D3D12_FEATURE_ROOT_SIGNATURE,
		&t_FeatureData, sizeof(t_FeatureData))))
	{
		BB_ASSERT(false, "DX12, root signature version 1.1 not supported! We do not currently support this.")
	}
	t_BuildInfo->rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	t_BuildInfo->rootSigDesc.Desc_1_1.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	t_BuildInfo->rootSigDesc.Desc_1_1.NumStaticSamplers = 0;
	t_BuildInfo->rootSigDesc.Desc_1_1.pStaticSamplers = nullptr;

	return PipelineBuilderHandle(t_BuildInfo);
}

void BB::DX12PipelineBuilderBindConstants(const PipelineBuilderHandle a_Handle, const BB::Slice<ConstantBind> a_ConstantBinds)
{
	PipelineBuildInfo* t_BuildInfo = reinterpret_cast<PipelineBuildInfo*>(a_Handle.ptrHandle);
	t_BuildInfo->rootConstantCount = static_cast<uint32_t>(a_ConstantBinds.size());
	t_BuildInfo->rootConstants = BBnewArr(
		t_BuildInfo->buildAllocator,
		t_BuildInfo->rootConstantCount,
		D3D12_ROOT_PARAMETER1);

	for (size_t i = 0; i < a_ConstantBinds.size(); i++)
	{
		size_t t_ParamIndex = t_BuildInfo->rootParameterCount + i;
		BB_ASSERT(a_ConstantBinds[i].size % sizeof(uint32_t) == 0, "DX12: BindConstant a_size is not a multiple of 32!");
		const UINT t_Dwords = a_ConstantBinds[i].size / sizeof(uint32_t);

		t_BuildInfo->rootConstants[t_ParamIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		t_BuildInfo->rootConstants[t_ParamIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; //This is for the indices so make it visible to all.

		t_BuildInfo->rootConstants[t_ParamIndex].Constants.Num32BitValues = t_Dwords;
		t_BuildInfo->rootConstants[t_ParamIndex].Constants.ShaderRegister = t_BuildInfo->regCBV++;
		t_BuildInfo->rootConstants[t_ParamIndex].Constants.RegisterSpace = 0; //We will just keep this 0 for now.
	}

	//not sure if this raises the root? Test first!
	t_BuildInfo->rootParameterCount += static_cast<uint32_t>(a_ConstantBinds.size());
}

void BB::DX12PipelineBuilderBindBuffers(const PipelineBuilderHandle a_Handle, const BB::Slice<BufferBind> a_BufferBinds)
{
	PipelineBuildInfo* t_BuildInfo = reinterpret_cast<PipelineBuildInfo*>(a_Handle.ptrHandle);
	t_BuildInfo->rootDescriptorCount = static_cast<uint32_t>(a_BufferBinds.size());
	t_BuildInfo->rootDescriptors = BBnewArr(
		t_BuildInfo->buildAllocator,
		t_BuildInfo->rootDescriptorCount,
		D3D12_ROOT_PARAMETER1);

	for (size_t i = 0; i < a_BufferBinds.size(); i++)
	{
		size_t t_ParamIndex = t_BuildInfo->rootParameterCount + i;
		switch (a_BufferBinds[i].type)
		{
		case DESCRIPTOR_BUFFER_TYPE::READONLY_CONSTANT:
			t_BuildInfo->rootDescriptors[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			t_BuildInfo->rootDescriptors[i].Descriptor.ShaderRegister = t_BuildInfo->regCBV;
			t_BuildInfo->rootDescriptors[i].Descriptor.RegisterSpace = 0;

			t_BuildInfo->buildPipeline.rootCBV[t_BuildInfo->buildPipeline.rootCBVCount].rootIndex = t_ParamIndex;
			t_BuildInfo->buildPipeline.rootCBV[t_BuildInfo->buildPipeline.rootCBVCount].virtAddress =
				reinterpret_cast<DXResource*>(a_BufferBinds[i].buffer.ptrHandle)->GetResource()->GetGPUVirtualAddress();

			//We do not increment the register since the root constants are also CBV.
			++t_BuildInfo->buildPipeline.rootCBVCount;
			++t_BuildInfo->regCBV;
			break;
		case DESCRIPTOR_BUFFER_TYPE::READONLY_BUFFER:
			t_BuildInfo->rootDescriptors[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			t_BuildInfo->rootDescriptors[i].Descriptor.ShaderRegister = a_BufferBinds[i].binding;
			t_BuildInfo->rootDescriptors[i].Descriptor.RegisterSpace = 0;

			t_BuildInfo->buildPipeline.rootSRV[t_BuildInfo->regSRV].rootIndex = t_ParamIndex;
			t_BuildInfo->buildPipeline.rootSRV[t_BuildInfo->regSRV].virtAddress =
				reinterpret_cast<DXResource*>(a_BufferBinds[i].buffer.ptrHandle)->GetResource()->GetGPUVirtualAddress();

			++t_BuildInfo->buildPipeline.rootSRVCount;
			++t_BuildInfo->regSRV;
			break;
		case DESCRIPTOR_BUFFER_TYPE::READWRITE:
			t_BuildInfo->rootDescriptors[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			t_BuildInfo->rootDescriptors[i].Descriptor.ShaderRegister = t_BuildInfo->regUAV;
			t_BuildInfo->rootDescriptors[i].Descriptor.RegisterSpace = a_BufferBinds[i].binding;

			t_BuildInfo->buildPipeline.rootUAV[t_BuildInfo->regUAV].rootIndex = t_ParamIndex;
			t_BuildInfo->buildPipeline.rootUAV[t_BuildInfo->regUAV].virtAddress =
				reinterpret_cast<DXResource*>(a_BufferBinds[i].buffer.ptrHandle)->GetResource()->GetGPUVirtualAddress();

			++t_BuildInfo->buildPipeline.rootUAVCount;
			++t_BuildInfo->regUAV;
			break;
		}
	}

	t_BuildInfo->rootParameterCount += static_cast<uint32_t>(a_BufferBinds.size());
}

void BB::DX12PipelineBuilderBindShaders(const PipelineBuilderHandle a_Handle, const Slice<BB::ShaderCreateInfo> a_ShaderInfo)
{
	PipelineBuildInfo* t_BuildInfo = reinterpret_cast<PipelineBuildInfo*>(a_Handle.ptrHandle);

	for (size_t i = 0; i < a_ShaderInfo.size(); i++)
	{
		switch (a_ShaderInfo[i].shaderStage)
		{
		case RENDER_SHADER_STAGE::VERTEX:
			t_BuildInfo->PSOdesc.VS.BytecodeLength = a_ShaderInfo[i].buffer.size;
			t_BuildInfo->PSOdesc.VS.pShaderBytecode = a_ShaderInfo[i].buffer.data;
			break;
		case RENDER_SHADER_STAGE::FRAGMENT_PIXEL:
			t_BuildInfo->PSOdesc.PS.BytecodeLength = a_ShaderInfo[i].buffer.size;
			t_BuildInfo->PSOdesc.PS.pShaderBytecode = a_ShaderInfo[i].buffer.data;
			break;
		default:
			BB_ASSERT(false, "DX12: unsupported shaderstage.")
				break;
		}
	}
}

PipelineHandle BB::DX12PipelineBuildPipeline(const PipelineBuilderHandle a_Handle)
{
	PipelineBuildInfo* t_BuildInfo = reinterpret_cast<PipelineBuildInfo*>(a_Handle.ptrHandle);

	{
		D3D12_ROOT_PARAMETER1* t_Parameters = BBnewArr(
			t_BuildInfo->buildAllocator,
			t_BuildInfo->rootParameterCount,
			D3D12_ROOT_PARAMETER1);


		for (size_t i = 0; i < t_BuildInfo->rootConstantCount; i++)
		{
			t_Parameters[i] = t_BuildInfo->rootConstants[i];
		}

		uint32_t t_ParameterOffset = t_BuildInfo->rootConstantCount;

		for (size_t i = 0; i < t_BuildInfo->rootDescriptorCount; i++)
		{
			t_Parameters[i + t_ParameterOffset] = t_BuildInfo->rootDescriptors[i];
		}

		t_ParameterOffset += t_BuildInfo->rootDescriptorCount;

		t_BuildInfo->rootSigDesc.Desc_1_1.NumParameters = t_BuildInfo->rootParameterCount;
		t_BuildInfo->rootSigDesc.Desc_1_1.pParameters = t_Parameters;

		ID3DBlob* t_Signature;
		ID3DBlob* t_Error;

		D3D12SerializeVersionedRootSignature(&t_BuildInfo->rootSigDesc,
			&t_Signature, &t_Error);

		if (t_Error != nullptr)
		{
			BB_LOG((const char*)t_Error->GetBufferPointer());
			BB_ASSERT(false, "DX12: error creating root signature, details are above.");
			t_Error->Release();
		}

		DXASSERT(s_DX12B.device.logicalDevice->CreateRootSignature(0,
			t_Signature->GetBufferPointer(),
			t_Signature->GetBufferSize(),
			IID_PPV_ARGS(&t_BuildInfo->buildPipeline.rootSig)),
			"DX12: Failed to create root signature.");

		t_BuildInfo->buildPipeline.rootSig->SetName(L"Hello Triangle Root Signature");

		if (t_Signature != nullptr)
			t_Signature->Release();
	}

	t_BuildInfo->PSOdesc.pRootSignature = t_BuildInfo->buildPipeline.rootSig;

	D3D12_INPUT_ELEMENT_DESC t_InputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} };

	t_BuildInfo->PSOdesc.InputLayout = { t_InputElementDescs, _countof(t_InputElementDescs) };


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
	t_BuildInfo->PSOdesc.BlendState = t_BlendDesc;


	D3D12_RASTERIZER_DESC t_RasterDesc{};
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

	t_BuildInfo->PSOdesc.RasterizerState = t_RasterDesc;
	t_BuildInfo->PSOdesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	t_BuildInfo->PSOdesc.DepthStencilState.DepthEnable = FALSE;
	t_BuildInfo->PSOdesc.DepthStencilState.StencilEnable = FALSE;
	t_BuildInfo->PSOdesc.SampleMask = UINT_MAX;
	t_BuildInfo->PSOdesc.NumRenderTargets = 1;
	t_BuildInfo->PSOdesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	t_BuildInfo->PSOdesc.SampleDesc.Count = 1;


	DXASSERT(s_DX12B.device.logicalDevice->CreateGraphicsPipelineState(
		&t_BuildInfo->PSOdesc, IID_PPV_ARGS(&t_BuildInfo->buildPipeline.pipelineState)),
		"DX12: Failed to create graphics pipeline");


	DXPipeline* t_ReturnPipeline = s_DX12B.pipelinePool.Get();
	*t_ReturnPipeline = t_BuildInfo->buildPipeline;

	return PipelineHandle(t_ReturnPipeline);
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
	DX12FrameBuffer& t_Framebuffer = s_DX12B.frameBuffers.find(a_Framebuffer.handle);

	t_CommandList->rtv = t_Framebuffer.renderTargets[s_DX12B.currentFrame];

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
	rtvHandle.ptr += static_cast<size_t>(s_DX12B.currentFrame * 
		s_DX12B.device.logicalDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	
	t_CommandList->List()->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	t_CommandList->List()->RSSetViewports(1, &t_Framebuffer.viewport);
	t_CommandList->List()->RSSetScissorRects(1, &t_Framebuffer.surfaceRect);
	t_CommandList->List()->ClearRenderTargetView(rtvHandle, t_Framebuffer.clearColor, 0, nullptr);
}

void BB::DX12EndRenderPass(const RecordingCommandListHandle a_RecordingCmdHandle)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	//// Indicate that the back buffer will now be used to present.
	D3D12_RESOURCE_BARRIER t_PresentBarrier;
	t_PresentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	t_PresentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	t_PresentBarrier.Transition.pResource = t_CommandList->rtv;
	t_PresentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	t_PresentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	t_PresentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	t_CommandList->List()->ResourceBarrier(1, &t_PresentBarrier);
}

void BB::DX12BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	DXPipeline* t_Pipeline = reinterpret_cast<DXPipeline*>(a_Pipeline.ptrHandle);

	t_CommandList->List()->SetPipelineState(t_Pipeline->pipelineState);
	t_CommandList->List()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	t_CommandList->List()->SetGraphicsRootSignature(t_Pipeline->rootSig);

	size_t t_OffsetCount = 0;
	for (size_t i = 0; i < t_Pipeline->rootCBVCount; i++)
	{
		t_CommandList->List()->SetGraphicsRootConstantBufferView(
			t_Pipeline->rootCBV[i].rootIndex,
			t_Pipeline->rootCBV[i].virtAddress + a_DynamicOffsets[t_OffsetCount++]);
	}
	for (size_t i = 0; i < t_Pipeline->rootSRVCount; i++)
	{
		t_CommandList->List()->SetGraphicsRootShaderResourceView(
			t_Pipeline->rootSRV[i].rootIndex,
			t_Pipeline->rootSRV[i].virtAddress + a_DynamicOffsets[t_OffsetCount++]);
	}
	for (size_t i = 0; i < t_Pipeline->rootUAVCount; i++)
	{
		t_CommandList->List()->SetGraphicsRootUnorderedAccessView(
			t_Pipeline->rootUAV[i].rootIndex,
			t_Pipeline->rootUAV[i].virtAddress + a_DynamicOffsets[t_OffsetCount++]);
	}
}

void BB::DX12BindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	D3D12_VERTEX_BUFFER_VIEW t_Views[12]{};
	for (size_t i = 0; i < a_BufferCount; i++)
	{
		t_Views[i] = reinterpret_cast<DXResource*>(a_Buffers[i].ptrHandle)->GetView().vertexView;
	}
	
	t_CommandList->List()->IASetVertexBuffers(0, static_cast<uint32_t>(a_BufferCount), t_Views);
}

void BB::DX12BindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	t_CommandList->List()->IASetIndexBuffer(&reinterpret_cast<DXResource*>(a_Buffer.ptrHandle)->GetView().indexView);
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
	DXResource* t_Resource = reinterpret_cast<DXResource*>(a_Handle.ptrHandle);
	void* t_MapData;

	DXASSERT(t_Resource->GetResource()->Map(0, NULL, &t_MapData),
		"DX12: Failed to map resource.");

	memcpy(Pointer::Add(t_MapData, a_Offset), a_Data, a_Size);

	t_Resource->GetResource()->Unmap(0, NULL);
}

void BB::DX12CopyBuffer(Allocator a_TempAllocator, const RenderCopyBufferInfo& a_CopyInfo)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_CopyInfo.transferCommandHandle.ptrHandle);

	ID3D12Resource* t_DestResource = reinterpret_cast<DXResource*>(a_CopyInfo.dst.ptrHandle)->GetResource();

	for (size_t i = 0; i < a_CopyInfo.CopyRegionCount; i++)
	{
		t_CommandList->List()->CopyBufferRegion(
			t_DestResource,
			a_CopyInfo.copyRegions[i].dstOffset,
			reinterpret_cast<DXResource*>(a_CopyInfo.src.handle)->GetResource(),
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

void* BB::DX12MapMemory(const RBufferHandle a_Handle)
{
	DXResource* t_Resource = reinterpret_cast<DXResource*>(a_Handle.ptrHandle);
	void* t_MapData;
	DXASSERT(t_Resource->GetResource()->Map(0, NULL, &t_MapData),
		"DX12: Failed to map resource.");
	return t_MapData;
}

void BB::DX12UnMemory(const RBufferHandle a_Handle)
{
	DXResource* t_Resource = reinterpret_cast<DXResource*>(a_Handle.ptrHandle);
	t_Resource->GetResource()->Unmap(0, NULL);
}

void BB::DX12StartFrame(Allocator a_TempAllocator, const StartFrameInfo& a_StartInfo)
{
	s_DX12B.frameFences[s_DX12B.currentFrame].WaitIdle();
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

		for (size_t queueIndex = 0; queueIndex < a_ExecuteInfos[i].waitQueueCount; queueIndex++)
		{
			DXCommandQueue* t_WaitQueue = reinterpret_cast<DXCommandQueue*>(a_ExecuteInfos[i].waitQueues[queueIndex].ptrHandle);
			reinterpret_cast<DXCommandQueue*>(a_ExecuteQueue.ptrHandle)->InsertWaitQueueFence(
				*t_WaitQueue, a_ExecuteInfos[i].waitValues[queueIndex]);
		}

		for (size_t j = 0; j < a_ExecuteInfos[i].commandCount; j++)
		{
			t_CommandLists[j] = reinterpret_cast<DXCommandList*>(
				a_ExecuteInfos[i].commands[j].ptrHandle)->List();
		}

		reinterpret_cast<DXCommandQueue*>(a_ExecuteQueue.ptrHandle)->ExecuteCommandlist(
			t_CommandLists,
			a_ExecuteInfos[i].commandCount);

		for (size_t queueIndex = 0; queueIndex < a_ExecuteInfos[i].signalQueueCount; queueIndex++)
		{
			reinterpret_cast<DXCommandQueue*>(a_ExecuteInfos[i].signalQueues[queueIndex].ptrHandle)->SignalQueue();
		}

		//Now reset the command lists.
		for (size_t j = 0; j < a_ExecuteInfos[i].commandCount; j++)
		{
			//reinterpret_cast<DXCommandList*>(a_ExecuteInfos[i].commands[j].ptrHandle)->Reset();
			//reinterpret_cast<DXCommandList*>(a_ExecuteInfos[i].commands[j].ptrHandle)->Close();
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

	for (size_t queueIndex = 0; queueIndex < a_ExecuteInfo.waitQueueCount; queueIndex++)
	{
		DXCommandQueue* t_WaitQueue = reinterpret_cast<DXCommandQueue*>(a_ExecuteInfo.waitQueues[queueIndex].ptrHandle);
		reinterpret_cast<DXCommandQueue*>(a_ExecuteQueue.ptrHandle)->InsertWaitQueueFence(
			*t_WaitQueue, a_ExecuteInfo.waitValues[queueIndex]);
	}

	for (size_t j = 0; j < a_ExecuteInfo.commandCount; j++)
	{
		t_CommandLists[j] = reinterpret_cast<DXCommandList*>(
			a_ExecuteInfo.commands[j].ptrHandle)->List();
	}

	reinterpret_cast<DXCommandQueue*>(a_ExecuteQueue.ptrHandle)->ExecuteCommandlist(
		t_CommandLists,
		a_ExecuteInfo.commandCount);

	for (size_t queueIndex = 0; queueIndex < a_ExecuteInfo.signalQueueCount; queueIndex++)
	{
		reinterpret_cast<DXCommandQueue*>(a_ExecuteInfo.signalQueues[queueIndex].ptrHandle)->SignalQueue();
	}
	reinterpret_cast<DXCommandQueue*>(a_ExecuteQueue.ptrHandle)->SignalQueue(
		s_DX12B.frameFences[s_DX12B.currentFrame]);

	//Now reset the command lists.
	for (size_t j = 0; j < a_ExecuteInfo.commandCount; j++)
	{
		//reinterpret_cast<DXCommandList*>(a_ExecuteInfo.commands[j].ptrHandle)->Reset();
	}
}

FrameIndex BB::DX12PresentFrame(Allocator a_TempAllocator, const PresentFrameInfo& a_PresentInfo)
{
	s_DX12B.swapchain.swapchain->Present(1, 0);
	s_DX12B.currentFrame = s_DX12B.swapchain.swapchain->GetCurrentBackBufferIndex();

	return s_DX12B.currentFrame;
}

uint64_t BB::DX12NextQueueFenceValue(const CommandQueueHandle a_Handle)
{
	return reinterpret_cast<DXCommandQueue*>(a_Handle.ptrHandle)->GetNextFenceValue();
}

//TO BE IMPLEMENTED.
uint64_t BB::DX12NextFenceValue(const RFenceHandle a_Handle)
{
	return reinterpret_cast<DXFence*>(a_Handle.ptrHandle)->GetNextFenceValue();
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
	DXFence* t_Fence = reinterpret_cast<DXFence*>(a_Handle.ptrHandle);
	s_DX12B.fencePool.Free(t_Fence);
	t_Fence->~DXFence();
}

void BB::DX12DestroyBuffer(const RBufferHandle a_Handle)
{
	DXResource* t_Resource = reinterpret_cast<DXResource*>(a_Handle.ptrHandle);
	s_DX12B.renderResources.Free(t_Resource);
	t_Resource->~DXResource();
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
	DXPipeline* t_Pipeline = reinterpret_cast<DXPipeline*>(a_Handle.ptrHandle);
	DXRelease(t_Pipeline->pipelineState);
	DXRelease(t_Pipeline->rootSig);
}

void BB::DX12DestroyFramebuffer(const FrameBufferHandle a_Handle)
{
	DX12FrameBuffer t_FrameBuffer = s_DX12B.frameBuffers.find(a_Handle.handle);
	
	for (size_t i = 0; i < s_DX12B.backBufferCount; i++)
	{
		t_FrameBuffer.renderTargets[i]->Release();
	}
	BBfreeArr(s_DX12Allocator, t_FrameBuffer.renderTargets);
	t_FrameBuffer.rtvHeap->Release();

	s_DX12B.frameBuffers.erase(a_Handle.handle);
}

void BB::DX12DestroyBackend()
{
	s_DX12B.swapchain.swapchain->SetFullscreenState(false, NULL);
	s_DX12B.swapchain.swapchain->Release();
	s_DX12B.swapchain.swapchain = nullptr;

	s_DX12B.DXMA->Release();
	if (s_DX12B.device.debugDevice)
	{
		s_DX12B.device.debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_NONE);
		s_DX12B.device.debugDevice->Release();
	}


	s_DX12B.device.logicalDevice->Release();
	s_DX12B.device.adapter->Release();
	if (s_DX12B.debugController)
		s_DX12B.debugController->Release();

	s_DX12B.factory->Release();
}
#include "DX12Common.h"
#include "DX12HelperTypes.h"

#include "Slotmap.h"
#include "Pool.h"
#include "BBString.h"

#include "TemporaryAllocator.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 610; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

//Tutorial used for this DX12 backend was https://alain.xyz/blog/raw-directx12 

using namespace BB;

struct DXPipelineBuildInfo
{
	//temporary allocator, this gets removed when we are finished building.
	TemporaryAllocator buildAllocator{ s_DX12Allocator };
	DXPipeline buildPipeline;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSOdesc{};
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc{};

	uint32_t descriptorSetCount;
	DXDescriptor descriptorSets[4]{};

	D3D12_ROOT_CONSTANTS rootConstant{};
};

enum class ShaderType
{
	VERTEX,
	PIXEL
};

static void CheckFeatureSupport()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS12 t_DeviceOptions{};

	s_DX12B.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &t_DeviceOptions, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS12));

	//BB_ASSERT(t_DeviceOptions.EnhancedBarriersSupported, "DX12, enhanced barriers not supported!");
}

static void SetupBackendSwapChain(UINT a_Width, UINT a_Height, HWND a_WindowHandle)
{
	s_DX12B.swapWidth = a_Width;
	s_DX12B.swapHeight = a_Height;
	
	//Just do a resize if a swapchain already exists.
	if (s_DX12B.swapchain != nullptr)
	{
		s_DX12B.swapchain->ResizeBuffers(s_DX12B.backBufferCount,
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

	s_DX12B.swapchain = (IDXGISwapChain3*)t_NewSwapchain;

	s_DX12B.currentFrame = s_DX12B.swapchain->GetCurrentBackBufferIndex();

	{ //crea
		const UINT t_IncrementSize = s_DX12B.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_DESCRIPTOR_HEAP_DESC t_RtvHeapDesc = {};
		t_RtvHeapDesc.NumDescriptors = s_DX12B.backBufferCount;
		t_RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; //RTV heaps are CPU only so the cost is not high.
		t_RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DXASSERT(s_DX12B.device->CreateDescriptorHeap(
			&t_RtvHeapDesc, IID_PPV_ARGS(&s_DX12B.swapchainRTVHeap)),
			"DX12: Failed to create descriptor heap for swapchain.");

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = s_DX12B.swapchainRTVHeap->GetCPUDescriptorHandleForHeapStart();

		s_DX12B.swapchainRenderTargets = BBnewArr(s_DX12Allocator, s_DX12B.backBufferCount, ID3D12Resource*);
		// Create a RTV for each frame.
		for (UINT i = 0; i < s_DX12B.backBufferCount; i++)
		{
			DXASSERT(s_DX12B.swapchain->GetBuffer(i,
				IID_PPV_ARGS(&s_DX12B.swapchainRenderTargets[i])),
				"DX12: Failed to get swapchain buffer.");

			s_DX12B.device->CreateRenderTargetView(
				s_DX12B.swapchainRenderTargets[i],
				nullptr,
				rtvHandle);
			rtvHandle.ptr += static_cast<uintptr_t>(1 * t_IncrementSize);
		}
	}
}

BackendInfo BB::DX12CreateBackend(const RenderBackendCreateInfo& a_CreateInfo)
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
		DXGI_ERROR_NOT_FOUND != s_DX12B.factory->EnumAdapters1(adapterIndex, &s_DX12B.adapter);
		++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 t_Desc;
		s_DX12B.adapter->GetDesc1(&t_Desc);

		//We don't take the software adapter.
		if (t_Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (t_Desc.DedicatedVideoMemory > t_BestDedicatedVRAM)
		{
			t_BestDedicatedVRAM = t_Desc.DedicatedVideoMemory;
			t_CurrentBestAdapter = s_DX12B.adapter;
		}
	}
	s_DX12B.adapter = t_CurrentBestAdapter;

	DXASSERT(D3D12CreateDevice(s_DX12B.adapter,
		D3D_FEATURE_LEVEL_12_2,
		IID_PPV_ARGS(&s_DX12B.device)),
		"DX12: Failed to create logical device.");

	if (a_CreateInfo.validationLayers)
	{
		DXASSERT(s_DX12B.device->QueryInterface(&s_DX12B.debugDevice),
			"DX12: Failed to query debug device.");
	}
	else
	{
		s_DX12B.debugDevice = nullptr;
	}
#pragma endregion //DEVICE_CREATION

	D3D12MA::ALLOCATOR_DESC t_AllocatorDesc = {};
	t_AllocatorDesc.pDevice = s_DX12B.device;
	t_AllocatorDesc.pAdapter = s_DX12B.adapter;

	DXASSERT(D3D12MA::CreateAllocator(&t_AllocatorDesc, &s_DX12B.DXMA),
		"DX12: Failed to create DX12 memory allocator");

	D3D12_COMMAND_QUEUE_DESC t_QueueDesc{};
	t_QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	t_QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DXASSERT(s_DX12B.device->CreateCommandQueue(&t_QueueDesc,
		IID_PPV_ARGS(&s_DX12B.directpresentqueue)),
		"DX12: Failed to create direct command queue");

	s_DX12B.directpresentqueue->SetName(L"Graphics&Present Queue");

	SetupBackendSwapChain(a_CreateInfo.windowWidth, 
		a_CreateInfo.windowHeight, 
		reinterpret_cast<HWND>(a_CreateInfo.windowHandle.ptrHandle));

	{
		D3D12_DESCRIPTOR_HEAP_DESC t_DsvHeap{};
		t_DsvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		t_DsvHeap.NumDescriptors = 32;
		t_DsvHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		DXASSERT(s_DX12B.device->CreateDescriptorHeap(&t_DsvHeap, IID_PPV_ARGS(&s_DX12B.dsvHeap.heap)),
			"DX12, Failed to create descriptor heap.");
		s_DX12B.dsvHeap.max = 32;
		s_DX12B.dsvHeap.pos = 0;
	}

	s_DX12B.frameFences = BBnewArr(s_DX12Allocator,
		s_DX12B.backBufferCount,
		DXFence);

	for (size_t i = 0; i < s_DX12B.backBufferCount; i++)
	{
		new (&s_DX12B.frameFences[i]) DXFence("DLL INTERNAL BACKBUFFER FENCES");
	}

	{//get heap increment sizes;
		s_DX12B.heap_cbv_srv_uav_increment_size = s_DX12B.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		s_DX12B.heap_sampler_size = s_DX12B.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	}

	CheckFeatureSupport();

	//Returns some info to the global backend that is important.
	BackendInfo t_BackendInfo;
	t_BackendInfo.currentFrame = s_DX12B.currentFrame;
	t_BackendInfo.framebufferCount = s_DX12B.backBufferCount;

	return t_BackendInfo;
}

RDescriptorHeap BB::DX12CreateDescriptorHeap(const DescriptorHeapCreateInfo& a_CreateInfo, const bool a_GpuVisible)
{
	D3D12_DESCRIPTOR_HEAP_TYPE t_HeapType;
	if (a_CreateInfo.isSampler)
		t_HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	else
		t_HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	DXDescriptorHeap* t_Heap = BBnew(s_DX12Allocator,
		DXDescriptorHeap)(
			t_HeapType,
			a_CreateInfo.descriptorCount,
			a_GpuVisible, 
			a_CreateInfo.name);
	return t_Heap;
}

RDescriptor BB::DX12CreateDescriptor(const RenderDescriptorCreateInfo& a_Info)
{
	D3D12_DESCRIPTOR_RANGE1* t_TableRanges = BBnewArr(
		s_DX12Allocator,
		a_Info.bindings.size(),
		D3D12_DESCRIPTOR_RANGE1);

	uint32_t t_DescriptorCount = 0;
	const UINT t_RegisterSpace = static_cast<const UINT>(a_Info.bindingSet);

	for (size_t i = 0; i < a_Info.bindings.size(); i++)
	{
		const DescriptorBinding& t_Binding = a_Info.bindings[i];
		t_DescriptorCount += t_Binding.descriptorCount;

		t_TableRanges[i].RangeType = DXConv::DescriptorRangeType(t_Binding.type);
		t_TableRanges[i].NumDescriptors = t_Binding.descriptorCount;
		t_TableRanges[i].BaseShaderRegister = t_Binding.binding;
		t_TableRanges[i].RegisterSpace = t_RegisterSpace;
	switch (t_Binding.flags)
	{
	case RENDER_DESCRIPTOR_FLAG::BINDLESS:
		t_TableRanges[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		break;
	case RENDER_DESCRIPTOR_FLAG::NONE:
		t_TableRanges[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
		break;
	default:
		break;
	}
		t_TableRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	DXDescriptor* t_Descriptor = s_DX12B.descriptorPool.Get();
	t_Descriptor->tableEntryCount = a_Info.bindings.size();
	t_Descriptor->tableEntries = t_TableRanges;
	t_Descriptor->descriptorCount = t_DescriptorCount;
	return RDescriptor(t_Descriptor);
}

CommandQueueHandle BB::DX12CreateCommandQueue(const RenderCommandQueueCreateInfo& a_Info)
{
	if (a_Info.queue == RENDER_QUEUE_TYPE::GRAPHICS)
		return CommandQueueHandle(new (s_DX12B.cmdQueues.Get())
			DXCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, s_DX12B.directpresentqueue));
	else
		return CommandQueueHandle(new (s_DX12B.cmdQueues.Get())DXCommandQueue(a_Info));
}

CommandAllocatorHandle BB::DX12CreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo)
{
	//Create the command allocator and it's command lists.
	DXCommandAllocator* t_CmdAllocator = new (s_DX12B.cmdAllocators.Get())
		DXCommandAllocator(a_CreateInfo);

	return CommandAllocatorHandle(t_CmdAllocator);
}

CommandListHandle BB::DX12CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo)
{
	DXCommandList* t_Cmdlist = reinterpret_cast<DXCommandAllocator*>(a_CreateInfo.commandAllocator.ptrHandle)->GetCommandList();
#ifdef _DEBUG
	t_Cmdlist->List()->SetName(UTF8ToUnicodeString(s_DX12TempAllocator, a_CreateInfo.name));
#endif //_DEBUG
	return CommandListHandle(t_Cmdlist);
}

RBufferHandle BB::DX12CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo)
{
	DXResource* t_Resource = new (s_DX12B.renderResources.Get())
		DXResource(a_CreateInfo);

	return RBufferHandle(t_Resource);
}

RImageHandle BB::DX12CreateImage(const RenderImageCreateInfo& a_CreateInfo)
{
	return RImageHandle(new (s_DX12B.renderImages.Get()) DXImage(a_CreateInfo));
}

RSamplerHandle BB::DX12CreateSampler(const SamplerCreateInfo& a_Info)
{
	return RSamplerHandle(new (s_DX12B.samplerPool.Get()) DXSampler(a_Info));
}

RFenceHandle BB::DX12CreateFence(const FenceCreateInfo& a_Info)
{
	return RFenceHandle(new (s_DX12B.fencePool.Get()) DXFence(a_Info.name));
}

DescriptorAllocation BB::DX12AllocateDescriptor(const AllocateDescriptorInfo& a_AllocateInfo)
{
	return reinterpret_cast<DXDescriptorHeap*>(a_AllocateInfo.heap.ptrHandle)->Allocate(a_AllocateInfo.descriptor, a_AllocateInfo.heapOffset);
}

void BB::DX12CopyDescriptors(const CopyDescriptorsInfo& a_CopyInfo)
{
	const DXDescriptorHeap* t_DstHeap = reinterpret_cast<DXDescriptorHeap*>(a_CopyInfo.dstHeap.ptrHandle);
	const DXDescriptorHeap* t_SrcHeap = reinterpret_cast<DXDescriptorHeap*>(a_CopyInfo.srcHeap.ptrHandle);

	const D3D12_CPU_DESCRIPTOR_HANDLE t_DstHandle{ t_DstHeap->GetCPUStartPtr().ptr + a_CopyInfo.dstOffset };
	const D3D12_CPU_DESCRIPTOR_HANDLE t_SrcHandle{ t_SrcHeap->GetCPUStartPtr().ptr + a_CopyInfo.srcOffset };

	const D3D12_DESCRIPTOR_HEAP_TYPE t_DstHeapType = t_DstHeap->GetHeapType();
	BB_ASSERT(t_DstHeapType == t_SrcHeap->GetHeapType(), "DX12, Heaptypes are not the same!");

	s_DX12B.device->CopyDescriptorsSimple(a_CopyInfo.descriptorCount,
		t_DstHandle, t_SrcHandle, t_DstHeapType);
}

void BB::DX12WriteDescriptors(const WriteDescriptorInfos& a_WriteInfo)
{
	UINT t_DescriptorIncrementSize;
	if (a_WriteInfo.data[0].type == RENDER_DESCRIPTOR_TYPE::SAMPLER)
		t_DescriptorIncrementSize = s_DX12B.heap_sampler_size;
	else
		t_DescriptorIncrementSize = s_DX12B.heap_cbv_srv_uav_increment_size;

	D3D12_CPU_DESCRIPTOR_HANDLE t_BaseHandle{ 0 };
	t_BaseHandle.ptr = reinterpret_cast<SIZE_T>(a_WriteInfo.allocation.bufferStart) + 
		(static_cast<SIZE_T>(a_WriteInfo.allocation.offset) * t_DescriptorIncrementSize);

	for (size_t i = 0; i < a_WriteInfo.data.size(); i++)
	{
		const WriteDescriptorData& t_WriteData = a_WriteInfo.data[i];

		D3D12_CPU_DESCRIPTOR_HANDLE t_DescHandle{ t_BaseHandle.ptr + ((t_WriteData.binding + t_WriteData.descriptorIndex) * t_DescriptorIncrementSize) };

		switch (t_WriteData.type)
		{
		case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:
		{
			DXResource* t_Resource = reinterpret_cast<DXResource*>(t_WriteData.buffer.buffer.ptrHandle);
			BB_ASSERT(t_Resource->GetResourceSize() >= t_WriteData.buffer.range + t_WriteData.buffer.offset,
				"DX12, trying to create descriptor that reads over bounds of a resource!");
			D3D12_CONSTANT_BUFFER_VIEW_DESC t_View{};
			t_View.BufferLocation = t_Resource->GetResource()->GetGPUVirtualAddress() + t_WriteData.buffer.offset;
			t_View.SizeInBytes = t_WriteData.buffer.range;
			s_DX12B.device->CreateConstantBufferView(&t_View, t_DescHandle);
		}
		break;
		case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:
		{
			DXResource* t_Resource = reinterpret_cast<DXResource*>(t_WriteData.buffer.buffer.ptrHandle);
			BB_ASSERT(t_Resource->GetResourceSize() >= t_WriteData.buffer.range + t_WriteData.buffer.offset,
				"DX12, trying to create descriptor that reads over bounds of a resource!");
			D3D12_SHADER_RESOURCE_VIEW_DESC t_View{};
			t_View.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			t_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			t_View.Buffer.NumElements = t_WriteData.buffer.range / 4;
			t_View.Buffer.FirstElement = t_WriteData.buffer.offset / 4; //does this work? Maybe
			t_View.Buffer.StructureByteStride = 0;
			t_View.Format = DXGI_FORMAT_R32_TYPELESS;
			t_View.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

			s_DX12B.device->CreateShaderResourceView(
				t_Resource->GetResource(),
				&t_View, t_DescHandle);
		}
		break;
		case RENDER_DESCRIPTOR_TYPE::READWRITE:
		{
			DXResource* t_Resource = reinterpret_cast<DXResource*>(t_WriteData.buffer.buffer.ptrHandle);
			BB_ASSERT(t_Resource->GetResourceSize() >= t_WriteData.buffer.range + t_WriteData.buffer.offset,
				"DX12, trying to create descriptor that reads over bounds of a resource!");
			D3D12_UNORDERED_ACCESS_VIEW_DESC t_View{};
			t_View.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			t_View.Buffer.NumElements = t_WriteData.buffer.range / 4;
			t_View.Buffer.FirstElement = t_WriteData.buffer.offset / 4; //does this work? Maybe
			t_View.Buffer.StructureByteStride = 0;
			t_View.Buffer.CounterOffsetInBytes = 0;
			t_View.Format = DXGI_FORMAT_R32_TYPELESS;
			t_View.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

			s_DX12B.device->CreateUnorderedAccessView(
				t_Resource->GetResource(),
				nullptr, &t_View, t_DescHandle);
		}
		break;
		case RENDER_DESCRIPTOR_TYPE::IMAGE:
		{
			DXImage* t_Image = reinterpret_cast<DXImage*>(t_WriteData.image.image.ptrHandle);

			D3D12_SHADER_RESOURCE_VIEW_DESC t_View = {};
			t_View.Format = t_Image->GetTextureData().format;
			t_View.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			t_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			t_View.Texture2D.MipLevels = 1;
			t_View.Texture2D.MostDetailedMip = 0;
			s_DX12B.device->CreateShaderResourceView(t_Image->GetResource(), &t_View, t_DescHandle);
		}
		break;
		case RENDER_DESCRIPTOR_TYPE::SAMPLER:
		{
			BB_ASSERT(false, "DX12, Not supporting creating non-immutable samplers yet.");
			//DXSampler* t_Sampler = reinterpret_cast<DXSampler*>(a_Info.sampler.ptrHandle);

			//s_DX12B.device->CreateSampler(t_Sampler->GetDesc(), t_DescHandle);
		}
		break;
		default:
			BB_ASSERT(false, "DX12, Trying to update a buffer descriptor with an invalid type.");
			break;
		}
	}
}

ImageReturnInfo BB::DX12GetImageInfo(const RImageHandle a_Handle)
{
	ImageReturnInfo t_ReturnInfo{};

	DXImage* t_Image = reinterpret_cast<DXImage*>(a_Handle.ptrHandle);
	D3D12_RESOURCE_DESC t_Desc = t_Image->GetResource()->GetDesc();

	const uint32_t subresourceNum = t_Desc.DepthOrArraySize + t_Desc.MipLevels;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* t_Layouts = BBnewArr(
		s_DX12TempAllocator,
		subresourceNum,
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT);

	UINT64 t_TotalByteSize = 0;

	s_DX12B.device->GetCopyableFootprints(&t_Desc, 0,
		1, 0,
		t_Layouts,
		nullptr,
		nullptr,
		&t_TotalByteSize);


	t_ReturnInfo.allocInfo.imageAllocByteSize = t_TotalByteSize;
	t_ReturnInfo.allocInfo.footRowPitch = t_Layouts->Footprint.RowPitch;
	t_ReturnInfo.allocInfo.footHeight = t_Layouts->Footprint.Height;

	t_ReturnInfo.width = static_cast<uint32_t>(t_Desc.Width);
	t_ReturnInfo.height = t_Desc.Height;
	t_ReturnInfo.depth = t_Desc.DepthOrArraySize;
	t_ReturnInfo.arrayLayers = t_Desc.DepthOrArraySize;
	t_ReturnInfo.mips = t_Desc.MipLevels;

	return t_ReturnInfo;
}

PipelineBuilderHandle BB::DX12PipelineBuilderInit(const PipelineInitInfo& a_InitInfo)
{
	DXPipelineBuildInfo* t_BuildInfo = BBnew(s_DX12Allocator, DXPipelineBuildInfo)();

	D3D12_FEATURE_DATA_ROOT_SIGNATURE t_FeatureData = {};
	t_FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(s_DX12B.device->CheckFeatureSupport(
		D3D12_FEATURE_ROOT_SIGNATURE,
		&t_FeatureData, sizeof(t_FeatureData))))
	{
		BB_ASSERT(false, "DX12, root signature version 1.1 not supported! We do not currently support this.")
	}
	t_BuildInfo->rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	t_BuildInfo->rootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	{
		D3D12_RASTERIZER_DESC t_RasterDesc{};
		t_RasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		switch (a_InitInfo.rasterizerState.cullMode)
		{
		case RENDER_CULL_MODE::NONE: t_RasterDesc.CullMode = D3D12_CULL_MODE_NONE;
			break;
		case RENDER_CULL_MODE::BACK: t_RasterDesc.CullMode = D3D12_CULL_MODE_BACK;
			break;
		case RENDER_CULL_MODE::FRONT: t_RasterDesc.CullMode = D3D12_CULL_MODE_FRONT;
			break;
		default:
			BB_ASSERT(false, "DX12, cull mode not supported for the rasterizer state!");
			break;
		}
		t_RasterDesc.FrontCounterClockwise = a_InitInfo.rasterizerState.frontCounterClockwise;
		t_RasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		t_RasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		t_RasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		t_RasterDesc.DepthClipEnable = TRUE;
		t_RasterDesc.MultisampleEnable = FALSE;
		t_RasterDesc.AntialiasedLineEnable = FALSE;
		t_RasterDesc.ForcedSampleCount = 0;
		t_RasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		t_BuildInfo->PSOdesc.RasterizerState = t_RasterDesc;
	}

	{
		D3D12_BLEND_DESC t_BlendDesc{};

		t_BlendDesc.AlphaToCoverageEnable = FALSE;
		t_BlendDesc.IndependentBlendEnable = FALSE;
		for (size_t i = 0; i < a_InitInfo.renderTargetBlendCount; i++)
		{
			t_BlendDesc.RenderTarget[i].LogicOp = DXConv::LogicOp(a_InitInfo.blendLogicOp);
			t_BlendDesc.RenderTarget[i].LogicOpEnable = a_InitInfo.blendLogicOpEnable;

			const PipelineRenderTargetBlend& t_Bi = a_InitInfo.renderTargetBlends[i];
			t_BlendDesc.RenderTarget[i].BlendEnable = t_Bi.blendEnable;
			t_BlendDesc.RenderTarget[i].BlendOp = DXConv::BlendOp(t_Bi.blendOp);
			t_BlendDesc.RenderTarget[i].SrcBlend = DXConv::Blend(t_Bi.srcBlend);
			t_BlendDesc.RenderTarget[i].DestBlend = DXConv::Blend(t_Bi.dstBlend);
			t_BlendDesc.RenderTarget[i].BlendOpAlpha = DXConv::BlendOp(t_Bi.blendOpAlpha);
			t_BlendDesc.RenderTarget[i].SrcBlendAlpha = DXConv::Blend(t_Bi.srcBlendAlpha);
			t_BlendDesc.RenderTarget[i].DestBlendAlpha = DXConv::Blend(t_Bi.dstBlendAlpha);
		}

		t_BuildInfo->PSOdesc.BlendState = t_BlendDesc;
	}

	if (a_InitInfo.constantData.dwordSize > 0)
	{
		//Reserve one space for the root constants
		t_BuildInfo->rootConstant.Num32BitValues = a_InitInfo.constantData.dwordSize;
		t_BuildInfo->rootConstant.ShaderRegister = 0;
		t_BuildInfo->rootConstant.RegisterSpace = 0;
	}

	if (a_InitInfo.enableDepthTest)
	{
		constexpr D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
		{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		D3D12_DEPTH_STENCIL_DESC t_DepthStencil{};
		t_DepthStencil.DepthEnable = TRUE;
		t_DepthStencil.StencilEnable = FALSE;
		t_DepthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		t_DepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		t_DepthStencil.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		t_DepthStencil.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		t_DepthStencil.FrontFace = defaultStencilOp;
		t_DepthStencil.BackFace = defaultStencilOp;
		t_BuildInfo->PSOdesc.DepthStencilState = t_DepthStencil;
	}
	else
	{
		t_BuildInfo->PSOdesc.DepthStencilState.DepthEnable = FALSE;
		t_BuildInfo->PSOdesc.DepthStencilState.StencilEnable = FALSE;
	}


	//immutable samplers
	if (a_InitInfo.immutableSamplers.size())
	{
		D3D12_STATIC_SAMPLER_DESC* t_Desc = BBnewArr(t_BuildInfo->buildAllocator,
			a_InitInfo.immutableSamplers.size(), 
			D3D12_STATIC_SAMPLER_DESC);

		Memory::Set(t_Desc, 0, a_InitInfo.immutableSamplers.size());

		for (size_t i = 0; i < a_InitInfo.immutableSamplers.size(); i++)
		{
			t_Desc[i].ShaderRegister = i;
			t_Desc[i].RegisterSpace = 0;
			t_Desc[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			t_Desc[i].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

			const SamplerCreateInfo& t_Samp = a_InitInfo.immutableSamplers[i];
			t_Desc[i].AddressU = DXConv::AddressMode(t_Samp.addressModeU);
			t_Desc[i].AddressV = DXConv::AddressMode(t_Samp.addressModeV);
			t_Desc[i].AddressW = DXConv::AddressMode(t_Samp.addressModeW);
			switch (t_Samp.filter)
			{
			case SAMPLER_FILTER::NEAREST:
				t_Desc[i].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
				break;
			case SAMPLER_FILTER::LINEAR:
				t_Desc[i].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
				break;
			default:
				BB_ASSERT(false, "DX12, does not support this type of sampler filter!");
				break;
			}
			t_Desc[i].MinLOD = t_Samp.minLod;
			t_Desc[i].MaxLOD = t_Samp.maxLod;
			t_Desc[i].MaxAnisotropy = static_cast<UINT>(t_Samp.maxAnistoropy);
		}

		t_BuildInfo->rootSigDesc.Desc_1_1.NumStaticSamplers = a_InitInfo.immutableSamplers.size();
		t_BuildInfo->rootSigDesc.Desc_1_1.pStaticSamplers = t_Desc;
	}
	else
	{
		t_BuildInfo->rootSigDesc.Desc_1_1.NumStaticSamplers = 0;
		t_BuildInfo->rootSigDesc.Desc_1_1.pStaticSamplers = nullptr;
	}


	return PipelineBuilderHandle(t_BuildInfo);
}

void BB::DX12PipelineBuilderBindDescriptor(const PipelineBuilderHandle a_Handle, const RDescriptor a_Descriptor)
{
	DXPipelineBuildInfo* t_BuildInfo = reinterpret_cast<DXPipelineBuildInfo*>(a_Handle.ptrHandle);
	const DXDescriptor* t_Descriptor = reinterpret_cast<DXDescriptor*>(a_Descriptor.ptrHandle);

	t_BuildInfo->descriptorSets[t_BuildInfo->descriptorSetCount++] = *t_Descriptor;
}

void BB::DX12PipelineBuilderBindShaders(const PipelineBuilderHandle a_Handle, const Slice<BB::ShaderCreateInfo> a_ShaderInfo)
{
	DXPipelineBuildInfo* t_BuildInfo = reinterpret_cast<DXPipelineBuildInfo*>(a_Handle.ptrHandle);

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

void BB::DX12PipelineBuilderBindAttributes(const PipelineBuilderHandle a_Handle, const PipelineAttributes& a_AttributeInfo)
{
	DXPipelineBuildInfo* t_BuildInfo = reinterpret_cast<DXPipelineBuildInfo*>(a_Handle.ptrHandle);
	BB_ASSERT(t_BuildInfo->PSOdesc.InputLayout.NumElements == 0, "DX12: Already bound attributes to this pipeline builder!");

	D3D12_INPUT_ELEMENT_DESC* t_InputDesc = BBnewArr(
		t_BuildInfo->buildAllocator,
		a_AttributeInfo.attributes.size(),
		D3D12_INPUT_ELEMENT_DESC);

	for (size_t i = 0; i < a_AttributeInfo.attributes.size(); i++)
	{
		t_InputDesc[i] = {};
		t_InputDesc[i].SemanticName = a_AttributeInfo.attributes[i].semanticName;
		t_InputDesc[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		t_InputDesc[i].AlignedByteOffset = a_AttributeInfo.attributes[i].offset;
		switch(a_AttributeInfo.attributes[i].format)
		{
		case RENDER_INPUT_FORMAT::R32:
			t_InputDesc[i].Format = DXGI_FORMAT_R32_FLOAT;
			break;
		case RENDER_INPUT_FORMAT::RG32:
			t_InputDesc[i].Format = DXGI_FORMAT_R32G32_FLOAT;
			break;
		case RENDER_INPUT_FORMAT::RGB32:
			t_InputDesc[i].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			break;
		case RENDER_INPUT_FORMAT::RGBA32:
			t_InputDesc[i].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			break;
		case RENDER_INPUT_FORMAT::RG8:
			t_InputDesc[i].Format = DXGI_FORMAT_R8G8_UNORM;
			break;
		case RENDER_INPUT_FORMAT::RGBA8:
			t_InputDesc[i].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		default:
			BB_ASSERT(false, "Vulkan: Input format not supported!");
			break;
		}
	}

	t_BuildInfo->PSOdesc.InputLayout.NumElements = static_cast<UINT>(a_AttributeInfo.attributes.size());
	t_BuildInfo->PSOdesc.InputLayout.pInputElementDescs = t_InputDesc;
}

PipelineHandle BB::DX12PipelineBuildPipeline(const PipelineBuilderHandle a_Handle)
{
	DXPipelineBuildInfo* t_BuildInfo = reinterpret_cast<DXPipelineBuildInfo*>(a_Handle.ptrHandle);

	uint32_t t_RootParamCount = t_BuildInfo->descriptorSetCount;

	if (t_BuildInfo->rootConstant.Num32BitValues > 0)
		++t_RootParamCount;

	D3D12_ROOT_PARAMETER1* t_Prams = BBnewArr(t_BuildInfo->buildAllocator, t_RootParamCount, D3D12_ROOT_PARAMETER1);

	{
		uint32_t t_StartIndex = 0;
		uint32_t t_EndIndex = t_BuildInfo->descriptorSetCount;
		if (t_BuildInfo->rootConstant.Num32BitValues > 0)
		{
			++t_StartIndex;
			++t_EndIndex;
			t_Prams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			t_Prams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			t_Prams[0].Constants = t_BuildInfo->rootConstant;
		}

		uint32_t t_Index = 0;
		for (size_t i = t_StartIndex; i < t_EndIndex; i++)
		{
			t_Prams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			t_Prams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			t_Prams[i].DescriptorTable.pDescriptorRanges = t_BuildInfo->descriptorSets[t_Index].tableEntries;
			t_Prams[i].DescriptorTable.NumDescriptorRanges = t_BuildInfo->descriptorSets[t_Index].tableEntryCount;
			t_BuildInfo->buildPipeline.rootParamBindingOffset[t_Index] = i;
			++t_Index;
		}
	}

	{
		t_BuildInfo->rootSigDesc.Desc_1_1.NumParameters = t_RootParamCount;
		t_BuildInfo->rootSigDesc.Desc_1_1.pParameters = t_Prams;
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

		DXASSERT(s_DX12B.device->CreateRootSignature(0,
			t_Signature->GetBufferPointer(),
			t_Signature->GetBufferSize(),
			IID_PPV_ARGS(&t_BuildInfo->buildPipeline.rootSig)),
			"DX12: Failed to create root signature.");

		t_BuildInfo->buildPipeline.rootSig->SetName(L"Hello Triangle Root Signature");

		if (t_Signature != nullptr)
			t_Signature->Release();
	}

	t_BuildInfo->PSOdesc.pRootSignature = t_BuildInfo->buildPipeline.rootSig;
	t_BuildInfo->PSOdesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	t_BuildInfo->PSOdesc.DSVFormat = DEPTH_FORMAT;
	t_BuildInfo->PSOdesc.SampleMask = UINT_MAX;
	t_BuildInfo->PSOdesc.NumRenderTargets = 1;
	t_BuildInfo->PSOdesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	t_BuildInfo->PSOdesc.SampleDesc.Count = 1;


	DXASSERT(s_DX12B.device->CreateGraphicsPipelineState(
		&t_BuildInfo->PSOdesc, IID_PPV_ARGS(&t_BuildInfo->buildPipeline.pipelineState)),
		"DX12: Failed to create graphics pipeline");

	DXPipeline* t_ReturnPipeline = s_DX12B.pipelinePool.Get();
	*t_ReturnPipeline = t_BuildInfo->buildPipeline;

	BBfree(s_DX12Allocator, t_BuildInfo);

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

void BB::DX12StartRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_RenderInfo)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	t_CommandList->rtv = s_DX12B.swapchainRenderTargets[s_DX12B.currentFrame];

	D3D12_RESOURCE_BARRIER  t_ImageBarrier{};
	t_ImageBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	t_ImageBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	t_ImageBarrier.Transition.StateBefore = DXConv::ResourceStateImage(a_RenderInfo.colorInitialLayout);
	t_ImageBarrier.Transition.StateAfter = DXConv::ResourceStateImage(a_RenderInfo.colorFinalLayout);
	t_ImageBarrier.Transition.pResource = t_CommandList->rtv;
	t_ImageBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	//jank, this is not an issue in Vulkan and maybe also not in enhanced barriers dx12.
	if(t_ImageBarrier.Transition.StateBefore != t_ImageBarrier.Transition.StateAfter)
		t_CommandList->List()->ResourceBarrier(1, &t_ImageBarrier);

	//D3D12_TEXTURE_BARRIER t_TextBarrier{};
	//t_TextBarrier.AccessAfter = D3D12_BARRIER_ACCESS_RENDER_TARGET;
	//t_TextBarrier.LayoutBefore = DXConv::BarrierLayout(a_RenderInfo.colorInitialLayout);
	//t_TextBarrier.LayoutAfter = DXConv::BarrierLayout(a_RenderInfo.colorFinalLayout);
	//t_TextBarrier.pResource = t_CommandList->rtv;
	//t_TextBarrier.Subresources.FirstPlane = 0;
	//t_TextBarrier.Subresources.NumPlanes = 1;
	//t_TextBarrier.Subresources.IndexOrFirstMipLevel = 0;
	//t_TextBarrier.Subresources.NumMipLevels = 1;

	//D3D12_BARRIER_GROUP t_RenderTargetBarrier{};
	//t_RenderTargetBarrier.Type = D3D12_BARRIER_TYPE_TEXTURE;
	//t_RenderTargetBarrier.NumBarriers = 1;
	//t_RenderTargetBarrier.pTextureBarriers = &t_TextBarrier;
	//t_CommandList->List()->Barrier(1, &t_RenderTargetBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE t_RtvHandle(s_DX12B.swapchainRTVHeap->GetCPUDescriptorHandleForHeapStart());
	t_RtvHandle.ptr += static_cast<size_t>(s_DX12B.currentFrame *
		s_DX12B.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	//set depth
	if (a_RenderInfo.depthStencil.handle != 0)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE t_DsvHandle = reinterpret_cast<DXImage*>(a_RenderInfo.depthStencil.ptrHandle)->GetDepthMetaData().dsvHandle;
		t_CommandList->List()->OMSetRenderTargets(1, &t_RtvHandle, FALSE, &t_DsvHandle);
		t_CommandList->List()->ClearDepthStencilView(t_DsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}
	else
	{
		t_CommandList->List()->OMSetRenderTargets(1, &t_RtvHandle, FALSE, nullptr);
	}

	switch (a_RenderInfo.colorLoadOp)
	{
	case RENDER_LOAD_OP::LOAD:

		break;
	case RENDER_LOAD_OP::CLEAR:
		t_CommandList->List()->ClearRenderTargetView(t_RtvHandle, a_RenderInfo.clearColor, 0, nullptr);
		break;
	case RENDER_LOAD_OP::DONT_CARE:

		break;
	default:
		BB_ASSERT(false, "DX12, no color load op specified!");
		break;
	}


	D3D12_VIEWPORT t_Viewport{};

	t_Viewport.Width = static_cast<FLOAT>(a_RenderInfo.viewportWidth);
	t_Viewport.Height = static_cast<FLOAT>(a_RenderInfo.viewportHeight);
	t_Viewport.MinDepth = .1f;
	t_Viewport.MaxDepth = 1000.f;

	t_CommandList->List()->RSSetViewports(1, &t_Viewport);

	D3D12_RECT t_Rect{ 0, 0, a_RenderInfo.viewportWidth, a_RenderInfo.viewportHeight };

	t_CommandList->List()->RSSetScissorRects(1, &t_Rect);
}

void BB::DX12SetScissor(const RecordingCommandListHandle a_RecordingCmdHandle, const ScissorInfo& a_ScissorInfo)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	D3D12_RECT t_Rect{ a_ScissorInfo.offset.x, a_ScissorInfo.offset.y, a_ScissorInfo.extent.x, a_ScissorInfo.extent.y };

	t_CommandList->List()->RSSetScissorRects(1, &t_Rect);
}

void BB::DX12EndRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	D3D12_RESOURCE_BARRIER t_PresentBarrier{};
	t_PresentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	t_PresentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	t_PresentBarrier.Transition.pResource = t_CommandList->rtv;
	t_PresentBarrier.Transition.StateBefore = DXConv::ResourceStates(a_EndInfo.colorInitialLayout);
	t_PresentBarrier.Transition.StateAfter = DXConv::ResourceStates(a_EndInfo.colorFinalLayout);
	t_PresentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	if (t_PresentBarrier.Transition.StateBefore != t_PresentBarrier.Transition.StateAfter)
		t_CommandList->List()->ResourceBarrier(1, &t_PresentBarrier);

	//D3D12_TEXTURE_BARRIER t_TextBarrier{};
	//t_TextBarrier.AccessBefore = D3D12_BARRIER_ACCESS_RENDER_TARGET;
	//t_TextBarrier.LayoutBefore = DXConv::BarrierLayout(a_EndInfo.colorInitialLayout);
	//t_TextBarrier.LayoutAfter = DXConv::BarrierLayout(a_EndInfo.colorFinalLayout);
	//t_TextBarrier.pResource = t_CommandList->rtv;
	//t_TextBarrier.Subresources.FirstPlane = 0;
	//t_TextBarrier.Subresources.NumPlanes = 1;
	//t_TextBarrier.Subresources.IndexOrFirstMipLevel = 0;
	//t_TextBarrier.Subresources.NumMipLevels = 1;

	//D3D12_BARRIER_GROUP t_PresentBarrier{};
	//t_PresentBarrier.Type = D3D12_BARRIER_TYPE_TEXTURE;
	//t_PresentBarrier.NumBarriers = 1;
	//t_PresentBarrier.pTextureBarriers = &t_TextBarrier;
	//t_CommandList->List()->Barrier(1, &t_PresentBarrier);
}

void BB::DX12CopyBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferInfo& a_CopyInfo)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	ID3D12Resource* t_DestResource = reinterpret_cast<DXResource*>(a_CopyInfo.dst.ptrHandle)->GetResource();
	ID3D12Resource* t_SrcResource = reinterpret_cast<DXResource*>(a_CopyInfo.src.handle)->GetResource();
	t_CommandList->List()->CopyBufferRegion(
		t_DestResource,
		a_CopyInfo.dstOffset,
		t_SrcResource,
		a_CopyInfo.srcOffset,
		a_CopyInfo.size);
}

void BB::DX12CopyBufferImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferImageInfo& a_CopyInfo)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	DXImage* t_DestImage = reinterpret_cast<DXImage*>(a_CopyInfo.dstImage.ptrHandle);
	DXResource* t_SrcResource = reinterpret_cast<DXResource*>(a_CopyInfo.srcBuffer.handle);

	//may include offsets later.
	D3D12_TEXTURE_COPY_LOCATION t_DestCopy = {};
	t_DestCopy.pResource = t_DestImage->GetResource();
	t_DestCopy.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	t_DestCopy.SubresourceIndex = 0;


	D3D12_PLACED_SUBRESOURCE_FOOTPRINT t_Layouts{};
	s_DX12B.device->GetCopyableFootprints(&t_DestImage->GetResource()->GetDesc(), 0,
		1, 0,
		&t_Layouts,
		nullptr,
		nullptr,
		nullptr);
	t_Layouts.Offset = a_CopyInfo.srcBufferOffset;

	D3D12_TEXTURE_COPY_LOCATION t_SrcCopy = {};
	t_SrcCopy.pResource = t_SrcResource->GetResource();
	t_SrcCopy.PlacedFootprint = t_Layouts;
	t_SrcCopy.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

	t_CommandList->List()->CopyTextureRegion(&t_DestCopy, 
		a_CopyInfo.dstImageInfo.offsetX,
		a_CopyInfo.dstImageInfo.offsetY,
		a_CopyInfo.dstImageInfo.offsetZ,
		&t_SrcCopy, nullptr);
}

void BB::DX12TransitionImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderTransitionImageInfo& a_TransitionInfo)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	D3D12_RESOURCE_BARRIER t_Barrier{};
	t_Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	t_Barrier.Transition.pResource = reinterpret_cast<DXImage*>(a_TransitionInfo.image.ptrHandle)->GetResource();
	t_Barrier.Transition.StateBefore = DXConv::ResourceStateImage(a_TransitionInfo.oldLayout);
	t_Barrier.Transition.StateAfter = DXConv::ResourceStateImage(a_TransitionInfo.newLayout);
	t_Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	t_CommandList->List()->ResourceBarrier(1, &t_Barrier);

	//D3D12_TEXTURE_BARRIER t_TextBarrier{};
	//t_TextBarrier.AccessBefore = DXConv::BarrierAccess(a_TransitionInfo.srcMask);
	//t_TextBarrier.AccessAfter = DXConv::BarrierAccess(a_TransitionInfo.dstMask);
	//t_TextBarrier.LayoutBefore = DXConv::BarrierLayout(a_TransitionInfo.oldLayout);
	//t_TextBarrier.LayoutAfter = DXConv::BarrierLayout(a_TransitionInfo.newLayout);
	//t_TextBarrier.SyncBefore = DXConv::BarrierSync(a_TransitionInfo.srcStage);
	//t_TextBarrier.SyncAfter = DXConv::BarrierSync(a_TransitionInfo.dstStage);
	//t_TextBarrier.pResource = reinterpret_cast<DXImage*>(a_TransitionInfo.image.ptrHandle)->GetResource();
	//t_TextBarrier.Subresources.FirstPlane = a_TransitionInfo.baseArrayLayer;
	//t_TextBarrier.Subresources.NumPlanes = a_TransitionInfo.layerCount;
	//t_TextBarrier.Subresources.IndexOrFirstMipLevel = a_TransitionInfo.baseMipLevel;
	//t_TextBarrier.Subresources.NumMipLevels = a_TransitionInfo.levelCount;
	//
	//D3D12_BARRIER_GROUP t_PresentBarrier{};
	//t_PresentBarrier.Type = D3D12_BARRIER_TYPE_TEXTURE;
	//t_PresentBarrier.NumBarriers = 1;
	//t_PresentBarrier.pTextureBarriers = &t_TextBarrier;
	//t_CommandList->List()->Barrier(1, &t_PresentBarrier);
}

void BB::DX12BindDescriptorHeaps(const RecordingCommandListHandle a_RecordingCmdHandle, const RDescriptorHeap a_ResourceHeap, const RDescriptorHeap a_SamplerHeap)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	uint32_t t_HeapCount = 1;
	ID3D12DescriptorHeap* t_Heaps[2]{};
	t_Heaps[0] = reinterpret_cast<DXDescriptorHeap*>(a_ResourceHeap.handle)->GetHeap();
	t_CommandList->heaps[0] = reinterpret_cast<DXDescriptorHeap*>(a_ResourceHeap.handle);
	if (a_SamplerHeap.handle != 0)
	{
		t_Heaps[1] = reinterpret_cast<DXDescriptorHeap*>(a_SamplerHeap.handle)->GetHeap();
		t_CommandList->heaps[1] = reinterpret_cast<DXDescriptorHeap*>(a_SamplerHeap.handle);
		++t_HeapCount;
	}

	t_CommandList->List()->SetDescriptorHeaps(t_HeapCount, t_Heaps);
}

void BB::DX12BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	DXPipeline* t_Pipeline = reinterpret_cast<DXPipeline*>(a_Pipeline.ptrHandle);

	t_CommandList->boundPipeline = t_Pipeline;
	t_CommandList->List()->SetPipelineState(t_Pipeline->pipelineState);
	t_CommandList->List()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	t_CommandList->List()->SetGraphicsRootSignature(t_Pipeline->rootSig);
}

void BB::DX12SetDescriptorHeapOffsets(const RecordingCommandListHandle a_RecordingCmdHandle, const RENDER_DESCRIPTOR_SET a_FirstSet, const uint32_t a_SetCount, const uint32_t* a_HeapIndex, const size_t* a_Offsets)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	for (size_t i = 0; i < a_SetCount; i++)
	{
		const UINT t_Offset = a_Offsets[i] * t_CommandList->heaps[0]->GetIncrementSize();
		D3D12_GPU_DESCRIPTOR_HANDLE t_GpuHandle{ t_CommandList->heaps[0]->GetGPUStartPtr().ptr + t_Offset};
		t_CommandList->List()->SetGraphicsRootDescriptorTable(
			t_CommandList->boundPipeline->rootParamBindingOffset[static_cast<uint32_t>(a_FirstSet)], 
			t_GpuHandle);
	}

}

void BB::DX12BindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	D3D12_VERTEX_BUFFER_VIEW t_Views[12]{};
	for (size_t i = 0; i < a_BufferCount; i++)
	{
		t_Views[i].SizeInBytes = reinterpret_cast<DXResource*>(a_Buffers[i].ptrHandle)->GetResourceSize();
		t_Views[i].StrideInBytes = sizeof(Vertex);
		t_Views[i].BufferLocation = reinterpret_cast<DXResource*>(a_Buffers[i].ptrHandle)->GetResource()->GetGPUVirtualAddress() + a_BufferOffsets[i];
	}
	
	t_CommandList->List()->IASetVertexBuffers(0, static_cast<uint32_t>(a_BufferCount), t_Views);
}

void BB::DX12BindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);
	D3D12_INDEX_BUFFER_VIEW t_View{};
	t_View.SizeInBytes = reinterpret_cast<DXResource*>(a_Buffer.ptrHandle)->GetResourceSize();
	t_View.Format = DXGI_FORMAT_R32_UINT;
	t_View.BufferLocation = reinterpret_cast<DXResource*>(a_Buffer.ptrHandle)->GetResource()->GetGPUVirtualAddress();
	t_View.BufferLocation += a_Offset;
	t_CommandList->List()->IASetIndexBuffer(&t_View);
}

void BB::DX12BindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_DwordOffset, const void* a_Data)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	t_CommandList->List()->SetGraphicsRoot32BitConstants(0,
		a_DwordCount,
		a_Data,
		a_DwordOffset);
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

void BB::DX12StartFrame(const StartFrameInfo& a_StartInfo)
{
	s_DX12B.frameFences[s_DX12B.currentFrame].WaitIdle();
}

void BB::DX12ExecuteCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount)
{
	for (size_t i = 0; i < a_ExecuteInfoCount; i++)
	{
		ID3D12CommandList** t_CommandLists = BBnewArr(
			s_DX12TempAllocator,
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
	}
}

//Special execute commands, not sure if DX12 needs anything special yet.
void BB::DX12ExecutePresentCommand(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo)
{
	ID3D12CommandList** t_CommandLists = BBnewArr(
		s_DX12TempAllocator,
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
}

FrameIndex BB::DX12PresentFrame(const PresentFrameInfo& a_PresentInfo)
{
	s_DX12B.swapchain->Present(1, 0);
	s_DX12B.currentFrame = s_DX12B.swapchain->GetCurrentBackBufferIndex();

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

void BB::DX12WaitCommands(const RenderWaitCommandsInfo& a_WaitInfo)
{
	for (size_t i = 0; i < a_WaitInfo.fences.size(); i++)
	{
		reinterpret_cast<DXFence*>(a_WaitInfo.fences[i].ptrHandle)->WaitIdle();
	}

	for (size_t i = 0; i < a_WaitInfo.queues.size(); i++)
	{
		reinterpret_cast<DXCommandQueue*>(a_WaitInfo.queues[i].ptrHandle)->WaitIdle();
	}
}

void BB::DX12DestroyFence(const RFenceHandle a_Handle)
{
	DXFence* t_Fence = reinterpret_cast<DXFence*>(a_Handle.ptrHandle);
	t_Fence->~DXFence();
	s_DX12B.fencePool.Free(t_Fence);
}

void BB::DX12DestroySampler(const RSamplerHandle a_Handle)
{
	DXSampler* t_Sampler = reinterpret_cast<DXSampler*>(a_Handle.ptrHandle);
	t_Sampler->~DXSampler();
	s_DX12B.samplerPool.Free(t_Sampler);
	BB_WARNING(false, "DX12 Sampler deletion not yet implemented.", WarningType::OPTIMALIZATION);
}

void BB::DX12DestroyImage(const RImageHandle a_Handle)
{
	DXImage* t_Resource = reinterpret_cast<DXImage*>(a_Handle.ptrHandle);
	t_Resource->~DXImage();
	s_DX12B.renderImages.Free(t_Resource);
}

void BB::DX12DestroyBuffer(const RBufferHandle a_Handle)
{
	DXResource* t_Resource = reinterpret_cast<DXResource*>(a_Handle.ptrHandle);
	t_Resource->~DXResource();
	s_DX12B.renderResources.Free(t_Resource);
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

void BB::DX12DestroyDescriptor(const RDescriptor a_Handle)
{
	DXDescriptor* t_Descriptor = reinterpret_cast<DXDescriptor*>(a_Handle.ptrHandle);
	BBfree(s_DX12Allocator, t_Descriptor->tableEntries);
	memset(t_Descriptor, 0, sizeof(DXDescriptor));

	s_DX12B.descriptorPool.Free(t_Descriptor);
}

void BB::DX12DestroyDescriptorHeap(const RDescriptorHeap a_Handle)
{
	DXDescriptorHeap* t_Heap = reinterpret_cast<DXDescriptorHeap*>(a_Handle.ptrHandle);
	t_Heap->~DXDescriptorHeap();
	BBfree(s_DX12Allocator, t_Heap);
}

void BB::DX12DestroyBackend()
{
	s_DX12B.swapchainRTVHeap->Release();
	s_DX12B.swapchain->SetFullscreenState(false, NULL);
	s_DX12B.swapchain->Release();
	s_DX12B.swapchain = nullptr;

	s_DX12B.DXMA->Release();
	if (s_DX12B.debugDevice)
	{
		s_DX12B.debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_NONE);
		s_DX12B.debugDevice->Release();
	}


	s_DX12B.device->Release();
	s_DX12B.adapter->Release();
	if (s_DX12B.debugController)
		s_DX12B.debugController->Release();

	s_DX12B.factory->Release();
}

//jank
D3D12_CPU_DESCRIPTOR_HANDLE DepthDescriptorHeap::Allocate()
{
	D3D12_CPU_DESCRIPTOR_HANDLE t_Handle = heap->GetCPUDescriptorHandleForHeapStart();
	t_Handle.ptr += static_cast<uint64_t>(pos) * s_DX12B.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	++pos;
	BB_ASSERT(max > pos, "DX12, too many depth allocations!");
	return t_Handle;
}
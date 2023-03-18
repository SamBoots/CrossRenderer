#include "DX12Common.h"
#include "DX12HelperTypes.h"

#include "Slotmap.h"
#include "Pool.h"
#include "BBString.h"

#include "TemporaryAllocator.h"

//Tutorial used for this DX12 backend was https://alain.xyz/blog/raw-directx12 

using namespace BB;

struct DXPipelineBuildInfo
{
	//temporary allocator, this gets removed when we are finished building.
	TemporaryAllocator buildAllocator{ s_DX12Allocator };
	DXPipeline buildPipeline;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSOdesc{};
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc{};

	struct RegSpace
	{
		UINT regCBV = 0;
		UINT regSRV = 0;
		UINT regUAV = 0;
	};
	RegSpace regSpaces[4];

	//Maximum of 4 bindings.
	uint32_t rootParamCount = 0;
	//Use this as if it always picks constants
	D3D12_ROOT_PARAMETER1* rootParams{};
};

enum class ShaderType
{
	VERTEX,
	PIXEL
};

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
		D3D_FEATURE_LEVEL_12_0,
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

	SetupBackendSwapChain(a_CreateInfo.windowWidth, 
		a_CreateInfo.windowHeight, 
		reinterpret_cast<HWND>(a_CreateInfo.windowHandle.ptrHandle));

	//Create the two main heaps.
	s_DX12B.CBV_SRV_UAVHeap = BBnew(s_DX12Allocator,
		DescriptorHeap)(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			4096,
			true);

	s_DX12B.samplerHeap = BBnew(s_DX12Allocator,
		DescriptorHeap)(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			128,
			true);

	s_DX12B.dsvHeap = BBnew(s_DX12Allocator,
		DescriptorHeap)(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			32,
			false);

	s_DX12B.frameFences = BBnewArr(s_DX12Allocator,
		s_DX12B.backBufferCount,
		DXFence);

	for (size_t i = 0; i < s_DX12B.backBufferCount; i++)
	{
		new (&s_DX12B.frameFences[i]) DXFence();
	}

	//Returns some info to the global backend that is important.
	BackendInfo t_BackendInfo;
	t_BackendInfo.currentFrame = s_DX12B.currentFrame;
	t_BackendInfo.framebufferCount = s_DX12B.backBufferCount;

	return t_BackendInfo;
}

RDescriptorHandle BB::DX12CreateDescriptor(const RenderDescriptorCreateInfo& a_Info)
{
	DXDescriptor* t_Descriptor = s_DX12B.bindingSetPool.Get();
	*t_Descriptor = {};

	t_Descriptor->shaderSpace = a_Info.bindingSet;
	UINT t_ParamIndex = 0;

	uint32_t t_TableDescriptorCount = 0;
	uint32_t t_TableBindingCount = 0;
	DescriptorBinding** t_TableBindings = BBnewArr(
		s_DX12TempAllocator,
		a_Info.bindings.size(),
		DescriptorBinding*);

	for (size_t i = 0; i < a_Info.bindings.size(); i++)
	{
		//Go through all the buffers.
		for (size_t i = 0; i < a_Info.bindings.size(); i++)
		{
			//If it's part of the shader table, make sure to register it.
			switch (a_Info.bindings[i].type)
			{
			case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:
				t_TableBindings[t_TableBindingCount++] = &a_Info.bindings[i];
				t_TableDescriptorCount += a_Info.bindings[i].descriptorCount;
				break;
			case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:
				t_TableBindings[t_TableBindingCount++] = &a_Info.bindings[i];
				t_TableDescriptorCount += a_Info.bindings[i].descriptorCount;
				break;
			case RENDER_DESCRIPTOR_TYPE::READWRITE:
				t_TableBindings[t_TableBindingCount++] = &a_Info.bindings[i];
				t_TableDescriptorCount += a_Info.bindings[i].descriptorCount;
				break;
			case RENDER_DESCRIPTOR_TYPE::COMBINED_IMAGE_SAMPLER:
				t_TableBindings[t_TableBindingCount++] = &a_Info.bindings[i];
				t_TableDescriptorCount += a_Info.bindings[i].descriptorCount;
				break;
			}
		}
	}

	if (t_TableBindingCount != 0)
	{
		uint32_t t_TableOffset = 0;

		t_Descriptor->tables.table = s_DX12B.CBV_SRV_UAVHeap->Allocate(t_TableDescriptorCount);
		t_Descriptor->tables.rootIndex = t_ParamIndex++;

		t_Descriptor->tableDescRangeCount = t_TableBindingCount;

		//set the ranges.
		t_Descriptor->tableDescRanges = BBnewArr(
			s_DX12Allocator,
			t_TableDescriptorCount,
			D3D12_DESCRIPTOR_RANGE1);

		for (size_t i = 0; i < t_TableBindingCount; i++)
		{
			if (t_TableBindings[i]->descriptorCount > 1)
				t_Descriptor->tableDescRanges[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

			switch (t_TableBindings[i]->type)
			{	
			case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:
				t_Descriptor->tableDescRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				break;
			case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:
				t_Descriptor->tableDescRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				break;
			case RENDER_DESCRIPTOR_TYPE::READWRITE:
				t_Descriptor->tableDescRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				break;
			case RENDER_DESCRIPTOR_TYPE::COMBINED_IMAGE_SAMPLER:
				t_Descriptor->tableDescRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				break;
			}
			
			t_Descriptor->tableDescRanges[i].OffsetInDescriptorsFromTableStart = t_TableOffset;
			t_Descriptor->tableDescRanges[i].NumDescriptors = t_TableBindings[i]->descriptorCount;
			t_Descriptor->tableDescRanges[i].RegisterSpace = static_cast<UINT>(t_Descriptor->shaderSpace);

			t_TableOffset += t_TableBindings[i]->descriptorCount;
		}
	}

	//Go through all the buffers.
	for (size_t i = 0; i < a_Info.bindings.size(); i++)
	{
		switch (a_Info.bindings[i].type)
		{
		case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT_DYNAMIC:
			t_Descriptor->rootCBV[t_Descriptor->cbvCount++].rootIndex = t_ParamIndex++;

			break;
		case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER_DYNAMIC:
			t_Descriptor->rootSRV[t_Descriptor->srvCount++].rootIndex = t_ParamIndex++;

			break;
		}
	}

	return RDescriptorHandle(t_Descriptor);
}

CommandQueueHandle BB::DX12CreateCommandQueue(const RenderCommandQueueCreateInfo& a_Info)
{
	switch (a_Info.queue)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		return CommandQueueHandle(new (s_DX12B.cmdQueues.Get())
			DXCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, s_DX12B.directpresentqueue));
		break;
	case RENDER_QUEUE_TYPE::TRANSFER_COPY:
		return CommandQueueHandle(new (s_DX12B.cmdQueues.Get())
			DXCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY));
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
		return CommandQueueHandle(new (s_DX12B.cmdQueues.Get())
			DXCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE));
		break;
	default:
		BB_ASSERT(false, "DX12: Tried to make a command queue with a queue type that does not exist.");
		return CommandQueueHandle(new (s_DX12B.cmdQueues.Get())
			DXCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, s_DX12B.directpresentqueue));
		break;
	}
}

CommandAllocatorHandle BB::DX12CreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo)
{
	//Create the command allocator and it's command lists.
	DXCommandAllocator* t_CmdAllocator = new (s_DX12B.cmdAllocators.Get())
		DXCommandAllocator(DXConv::CommandListType(a_CreateInfo.queueType),
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
		DXResource(a_Info.usage, a_Info.memProperties, a_Info.size);

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

RImageHandle BB::DX12CreateImage(const RenderImageCreateInfo& a_CreateInfo)
{
	DXImage* t_Image = new (s_DX12B.renderImages.Get())
		DXImage(a_CreateInfo);

	return RImageHandle(t_Image);
}

RFenceHandle BB::DX12CreateFence(const FenceCreateInfo& a_Info)
{
	return RFenceHandle(new (s_DX12B.fencePool.Get()) DXFence());
}

void BB::DX12UpdateDescriptorBuffer(const UpdateDescriptorBufferInfo& a_Info)
{
	D3D12_GPU_VIRTUAL_ADDRESS t_Address = reinterpret_cast<DXResource*>(a_Info.buffer.ptrHandle)->GetResource()->GetGPUVirtualAddress();
	t_Address += a_Info.bufferOffset;
	
	DXDescriptor* t_Descriptor = reinterpret_cast<DXDescriptor*>(a_Info.set.ptrHandle);
	D3D12_CPU_DESCRIPTOR_HANDLE t_DescHandle = t_Descriptor->tables.table.cpuHandle;
	t_DescHandle.ptr += static_cast<uint64_t>(t_Descriptor->tables.table.incrementSize * a_Info.descriptorIndex);

	switch (a_Info.type)
	{
	case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC t_View{};
		t_View.BufferLocation = t_Address;
		t_View.SizeInBytes = a_Info.bufferSize;
		s_DX12B.device->CreateConstantBufferView(&t_View, t_DescHandle);
	}
	break;
	case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC t_View{};
		t_View.BufferLocation = t_Address;
		t_View.SizeInBytes = a_Info.bufferSize;
		s_DX12B.device->CreateConstantBufferView(&t_View, t_DescHandle);
	}
		break;
	case RENDER_DESCRIPTOR_TYPE::READWRITE:
	{
		//D3D12_UNORDERED_ACCESS_VIEW_DESC t_View{};

		//s_DX12B.device->CreateUnorderedAccessView();
	}
	case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT_DYNAMIC:
	{
		t_Descriptor->rootSRV[a_Info.binding].virtAddress = t_Address;
	}
	break;
	case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER_DYNAMIC:
	{
		t_Descriptor->rootSRV[a_Info.binding].virtAddress = t_Address;
	}
	break;
	default:
		DXASSERT(false, "DX12, Trying to update a buffer descriptor with an invalid type.");
		break;
	}
}

void BB::DX12UpdateDescriptorImage(const UpdateDescriptorImageInfo& a_Info)
{
	DXImage* t_Image = reinterpret_cast<DXImage*>(a_Info.image.ptrHandle);

	DXDescriptor* t_Descriptor = reinterpret_cast<DXDescriptor*>(a_Info.set.ptrHandle);
	D3D12_CPU_DESCRIPTOR_HANDLE t_DescHandle = t_Descriptor->tables.table.cpuHandle;
	t_DescHandle.ptr += static_cast<uint64_t>(t_Descriptor->tables.table.incrementSize * a_Info.descriptorIndex);

	switch (a_Info.type)
	{
	case RENDER_DESCRIPTOR_TYPE::COMBINED_IMAGE_SAMPLER:
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC t_View = {};
		t_View.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		t_View.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		t_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		t_View.Texture2D.MipLevels = 1;
		t_View.Texture2D.MostDetailedMip = 0;
		s_DX12B.device->CreateShaderResourceView(t_Image->GetResource(), &t_View, t_DescHandle);
	}
		break;
	default:
		DXASSERT(false, "DX12, Trying to update a image descriptor with an invalid type.");
		break;
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

//PipelineBuilder
PipelineBuilderHandle BB::DX12PipelineBuilderInit(const PipelineInitInfo& t_InitInfo)
{
	constexpr size_t MAXIMUM_ROOT_PARAMETERS = 64;
	DXPipelineBuildInfo* t_BuildInfo = BBnew(s_DX12Allocator, DXPipelineBuildInfo);

	D3D12_FEATURE_DATA_ROOT_SIGNATURE t_FeatureData = {};
	t_FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(s_DX12B.device->CheckFeatureSupport(
		D3D12_FEATURE_ROOT_SIGNATURE,
		&t_FeatureData, sizeof(t_FeatureData))))
	{
		BB_ASSERT(false, "DX12, root signature version 1.1 not supported! We do not currently support this.")
	}
	t_BuildInfo->rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	t_BuildInfo->rootSigDesc.Desc_1_1.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	

	t_BuildInfo->rootParams = BBnewArr(
		t_BuildInfo->buildAllocator,
		MAXIMUM_ROOT_PARAMETERS,
		D3D12_ROOT_PARAMETER1);

	//Reserve one space for the root constants
	t_BuildInfo->rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	t_BuildInfo->rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	t_BuildInfo->rootParams[0].Constants.RegisterSpace = 0;
	t_BuildInfo->rootParams[0].Constants.ShaderRegister = t_BuildInfo->regSpaces[0].regCBV++;
	t_BuildInfo->rootParams[0].Constants.Num32BitValues = 1;
	++t_BuildInfo->rootParamCount;

	return PipelineBuilderHandle(t_BuildInfo);
}

void BB::DX12PipelineBuilderBindDescriptor(const PipelineBuilderHandle a_Handle, const RDescriptorHandle a_BindingSetHandle)
{
	DXPipelineBuildInfo* t_BuildInfo = reinterpret_cast<DXPipelineBuildInfo*>(a_Handle.ptrHandle);
	const DXDescriptor* t_BindingSet = reinterpret_cast<DXDescriptor*>(a_BindingSetHandle.ptrHandle);

	uint32_t t_ParamIndex = t_BuildInfo->rootParamCount;
	t_BuildInfo->buildPipeline.rootParamBindingOffset[static_cast<uint32_t>(t_BindingSet->shaderSpace)] = t_ParamIndex;

	if (t_BindingSet->tables.table.count != 0)
	{
		for (uint32_t i = 0; i < t_BindingSet->tableDescRangeCount; i++)
		{
			t_BindingSet->tableDescRanges[i].RegisterSpace = static_cast<uint32_t>(t_BindingSet->shaderSpace);

			switch (t_BindingSet->tableDescRanges[i].RangeType)
			{
			case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
				t_BindingSet->tableDescRanges[i].BaseShaderRegister = t_BuildInfo->regSpaces[static_cast<uint32_t>(t_BindingSet->shaderSpace)].regCBV++;
				break;
			case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
				t_BindingSet->tableDescRanges[i].BaseShaderRegister = t_BuildInfo->regSpaces[static_cast<uint32_t>(t_BindingSet->shaderSpace)].regSRV++;
				break;
			default:
				BB_ASSERT(false, "DirectX12, Descriptor range type not yet supported!");
				break;
			}
			
		}

		t_BuildInfo->rootParams[t_ParamIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		t_BuildInfo->rootParams[t_ParamIndex].DescriptorTable.NumDescriptorRanges = t_BindingSet->tableDescRangeCount;
		t_BuildInfo->rootParams[t_ParamIndex].DescriptorTable.pDescriptorRanges = t_BindingSet->tableDescRanges;

		++t_ParamIndex;
	}

	for (uint32_t i = 0; i < t_BindingSet->cbvCount; i++)
	{
		t_BuildInfo->rootParams[t_ParamIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		t_BuildInfo->rootParams[t_ParamIndex].Descriptor.ShaderRegister = t_BuildInfo->regSpaces[static_cast<uint32_t>(t_BindingSet->shaderSpace)].regCBV++;
		t_BuildInfo->rootParams[t_ParamIndex].Descriptor.RegisterSpace = static_cast<uint32_t>(t_BindingSet->shaderSpace);

		++t_ParamIndex;
	}

	for (uint32_t i = 0; i < t_BindingSet->srvCount; i++)
	{
		t_BuildInfo->rootParams[t_ParamIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		t_BuildInfo->rootParams[t_ParamIndex].Descriptor.ShaderRegister = t_BuildInfo->regSpaces[static_cast<uint32_t>(t_BindingSet->shaderSpace)].regSRV++;
		t_BuildInfo->rootParams[t_ParamIndex].Descriptor.RegisterSpace = static_cast<uint32_t>(t_BindingSet->shaderSpace);

		++t_ParamIndex;
	}

	t_BuildInfo->rootParamCount = t_ParamIndex;
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

PipelineHandle BB::DX12PipelineBuildPipeline(const PipelineBuilderHandle a_Handle)
{
	DXPipelineBuildInfo* t_BuildInfo = reinterpret_cast<DXPipelineBuildInfo*>(a_Handle.ptrHandle);

	{
		D3D12_STATIC_SAMPLER_DESC t_SamplerDesc{};
		t_SamplerDesc.RegisterSpace = 1;
		t_SamplerDesc.ShaderRegister = 0;
		t_SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		t_SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		t_SamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		t_SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		t_SamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		t_SamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		t_SamplerDesc.MinLOD = 0.0f;
		t_SamplerDesc.MaxLOD = 0.0f;
		t_SamplerDesc.MipLODBias = 0;
		t_SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		
		t_BuildInfo->rootSigDesc.Desc_1_1.NumParameters = t_BuildInfo->rootParamCount;
		t_BuildInfo->rootSigDesc.Desc_1_1.pParameters = t_BuildInfo->rootParams;
		t_BuildInfo->rootSigDesc.Desc_1_1.NumStaticSamplers = 1;
		t_BuildInfo->rootSigDesc.Desc_1_1.pStaticSamplers = &t_SamplerDesc;
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

	FixedArray<D3D12_INPUT_ELEMENT_DESC, 4> t_InputElementDescs = VertexInputElements();

	t_BuildInfo->PSOdesc.InputLayout.pInputElementDescs = t_InputElementDescs.data();
	t_BuildInfo->PSOdesc.InputLayout.NumElements = static_cast<uint32_t>(t_InputElementDescs.size());


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

	t_BuildInfo->PSOdesc.RasterizerState = t_RasterDesc;
	t_BuildInfo->PSOdesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	t_BuildInfo->PSOdesc.DepthStencilState = t_DepthStencil;
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

	ID3D12DescriptorHeap* t_Heaps[] =
	{
		s_DX12B.CBV_SRV_UAVHeap->GetHeap(),
		s_DX12B.samplerHeap->GetHeap()
	};

	t_CommandList->List()->SetDescriptorHeaps(2, t_Heaps);

	D3D12_RESOURCE_STATES t_StateBefore;
	D3D12_RESOURCE_STATES t_StateAfter;
	switch (a_RenderInfo.colorInitialLayout)
	{
	case RENDER_IMAGE_LAYOUT::UNDEFINED:
		t_StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		break;
	case RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL:
		t_StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		break;
	case RENDER_IMAGE_LAYOUT::TRANSFER_SRC:
		t_StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		break;
	case RENDER_IMAGE_LAYOUT::TRANSFER_DST:
		t_StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		break;
	default: BB_ASSERT(false, "DX12: invalid initial state for end rendering!");
	}

	switch (a_RenderInfo.colorFinalLayout)
	{
	case RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL:
		t_StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		break;
	case RENDER_IMAGE_LAYOUT::TRANSFER_SRC:
		t_StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		break;
	case RENDER_IMAGE_LAYOUT::TRANSFER_DST:
		t_StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		break;
	default: BB_ASSERT(false, "DX12: invalid initial state for end rendering!");
	}

	D3D12_RESOURCE_BARRIER t_RenderTargetBarrier;
	t_RenderTargetBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	t_RenderTargetBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	t_RenderTargetBarrier.Transition.pResource = t_CommandList->rtv;
	t_RenderTargetBarrier.Transition.StateBefore = t_StateBefore;
	t_RenderTargetBarrier.Transition.StateAfter = t_StateAfter;
	t_RenderTargetBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	t_CommandList->List()->ResourceBarrier(1, &t_RenderTargetBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE t_RtvHandle(s_DX12B.swapchainRTVHeap->GetCPUDescriptorHandleForHeapStart());
	t_RtvHandle.ptr += static_cast<size_t>(s_DX12B.currentFrame *
		s_DX12B.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	D3D12_CPU_DESCRIPTOR_HANDLE t_DsvHandle = reinterpret_cast<DXImage*>(a_RenderInfo.depthStencil.ptrHandle)->GetDepthMetaData().dsvHandle;
	t_CommandList->List()->OMSetRenderTargets(1, &t_RtvHandle, FALSE, &t_DsvHandle);

	D3D12_VIEWPORT t_Viewport{};

	t_Viewport.Width = static_cast<FLOAT>(a_RenderInfo.viewportWidth);
	t_Viewport.Height = static_cast<FLOAT>(a_RenderInfo.viewportHeight);
	t_Viewport.MinDepth = .1f;
	t_Viewport.MaxDepth = 1000.f;

	t_CommandList->List()->RSSetViewports(1, &t_Viewport);

	D3D12_RECT t_Rect{};
	t_Rect.left = 0;
	t_Rect.top = 0;
	t_Rect.right = static_cast<LONG>(a_RenderInfo.viewportWidth);
	t_Rect.bottom = static_cast<LONG>(a_RenderInfo.viewportHeight);
	
	t_CommandList->List()->RSSetScissorRects(1, &t_Rect);
	t_CommandList->List()->ClearRenderTargetView(t_RtvHandle, a_RenderInfo.clearColor, 0, nullptr);
	t_CommandList->List()->ClearDepthStencilView(t_DsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void BB::DX12EndRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	D3D12_RESOURCE_STATES t_StateBefore{};
	D3D12_RESOURCE_STATES t_StateAfter{};
	switch (a_EndInfo.colorInitialLayout)
	{
	case RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL: 
		t_StateBefore =  D3D12_RESOURCE_STATE_RENDER_TARGET; 
		break;
	case RENDER_IMAGE_LAYOUT::TRANSFER_SRC:				
		t_StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		break;
	case RENDER_IMAGE_LAYOUT::TRANSFER_DST:				
		t_StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		break;
	default:											
		BB_ASSERT(false, "DX12: invalid initial state for end rendering!");
		break;
	}

	switch (a_EndInfo.colorFinalLayout)
	{
	case RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL: 
		t_StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		break;
	case RENDER_IMAGE_LAYOUT::TRANSFER_SRC:				
		t_StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		break;
	case RENDER_IMAGE_LAYOUT::TRANSFER_DST:				
		t_StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		break;
	case RENDER_IMAGE_LAYOUT::PRESENT:					
		t_StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		break;
	default:
		BB_ASSERT(false, "DX12: invalid initial state for end rendering!");
		break;
	}

	//// Indicate that the back buffer will now be used to present.
	D3D12_RESOURCE_BARRIER t_PresentBarrier;
	t_PresentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	t_PresentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	t_PresentBarrier.Transition.pResource = t_CommandList->rtv;
	t_PresentBarrier.Transition.StateBefore = t_StateBefore;
	t_PresentBarrier.Transition.StateAfter = t_StateAfter;
	t_PresentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	t_CommandList->List()->ResourceBarrier(1, &t_PresentBarrier);
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

void BB::DX12BindDescriptors(const RecordingCommandListHandle a_RecordingCmdHandle, const RDescriptorHandle* a_Sets, const uint32_t a_SetCount, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	for (size_t i = 0; i < a_SetCount; i++)
	{
		const DXDescriptor* t_BindingSet = reinterpret_cast<DXDescriptor*>(a_Sets[i].ptrHandle);
		const uint32_t t_StartBindingIndex =
			t_CommandList->boundPipeline->rootParamBindingOffset[static_cast<uint32_t>(t_BindingSet->shaderSpace)];


		if (t_BindingSet->tableDescRangeCount)
			t_CommandList->List()->SetGraphicsRootDescriptorTable(t_BindingSet->tables.rootIndex + t_StartBindingIndex, t_BindingSet->tables.table.gpuHandle);

		//TODO: dynamic offsets not simulate how vulkan does it yet. No issue for now since everything in vulkan has a dynamic offset.
		for (size_t i = 0; i < t_BindingSet->cbvCount; i++)
		{
			t_CommandList->List()->SetGraphicsRootConstantBufferView(
				t_BindingSet->rootCBV[i].rootIndex + t_StartBindingIndex,
				t_BindingSet->rootCBV[i].virtAddress + a_DynamicOffsets[i]);
		}

		//TODO: dynamic offsets not simulate how vulkan does it yet. No issue for now since everything in vulkan has a dynamic offset.
		for (size_t i = 0; i < t_BindingSet->srvCount; i++)
		{
			t_CommandList->List()->SetGraphicsRootShaderResourceView(
				t_BindingSet->rootSRV[i].rootIndex + t_StartBindingIndex,
				t_BindingSet->rootSRV[i].virtAddress + a_DynamicOffsets[i]);
		}
	}
}

void BB::DX12BindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_Offset, const void* a_Data)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_RecordingCmdHandle.ptrHandle);

	t_CommandList->List()->SetGraphicsRoot32BitConstants(0,
		a_DwordCount,
		a_Data,
		a_Offset);
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

void BB::DX12DestroyImage(const RImageHandle a_Handle)
{
	DXImage* t_Resource = reinterpret_cast<DXImage*>(a_Handle.ptrHandle);
	s_DX12B.renderImages.Free(t_Resource);
	t_Resource->~DXImage();
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

void BB::DX12DestroyDescriptor(const RDescriptorHandle a_Handle)
{
	DXDescriptor* t_Set = reinterpret_cast<DXDescriptor*>(a_Handle.ptrHandle);
	*t_Set = {}; //zero it for safety
	s_DX12B.bindingSetPool.Free(t_Set);
}

void BB::DX12DestroyBackend()
{
	s_DX12B.swapchainRTVHeap->Release();
	s_DX12B.swapchain->SetFullscreenState(false, NULL);
	s_DX12B.swapchain->Release();
	s_DX12B.swapchain = nullptr;
	BBfree(s_DX12Allocator, s_DX12B.CBV_SRV_UAVHeap);
	BBfree(s_DX12Allocator, s_DX12B.dsvHeap);
	BBfree(s_DX12Allocator, s_DX12B.samplerHeap);

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
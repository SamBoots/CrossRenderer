#include "DX12HelperTypes.h"
#include "Utils.h"
#include "Math.inl"

using namespace BB;

DX12Backend_inst BB::s_DX12B{};

const D3D12_SHADER_VISIBILITY BB::DXConv::ShaderVisibility(const RENDER_SHADER_STAGE a_Stage)
{
	switch (a_Stage)
	{
	case RENDER_SHADER_STAGE::ALL:					return D3D12_SHADER_VISIBILITY_ALL;
	case RENDER_SHADER_STAGE::FRAGMENT_PIXEL:		return D3D12_SHADER_VISIBILITY_PIXEL;
	case RENDER_SHADER_STAGE::VERTEX:				return D3D12_SHADER_VISIBILITY_VERTEX;
	default:
		BB_ASSERT(false, "DX12, this RENDER_SHADER_STAGE not supported by DX12!");
		return D3D12_SHADER_VISIBILITY_ALL;
		break;
	}
}

const D3D12_RESOURCE_STATES BB::DXConv::ResourceStateImage(const RENDER_IMAGE_LAYOUT a_ImageLayout)
{
	switch (a_ImageLayout)
	{
	case RENDER_IMAGE_LAYOUT::UNDEFINED:			return D3D12_RESOURCE_STATE_COMMON;
	case RENDER_IMAGE_LAYOUT::TRANSFER_DST:			return D3D12_RESOURCE_STATE_COPY_DEST;
	case RENDER_IMAGE_LAYOUT::TRANSFER_SRC:			return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY:		return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	default:
		BB_ASSERT(false, "DX12, this Image Layout not supported by DX12!");
		return D3D12_RESOURCE_STATE_COMMON;
		break;
	}
}

const D3D12_HEAP_TYPE BB::DXConv::HeapType(const RENDER_MEMORY_PROPERTIES a_Properties)
{
	switch (a_Properties)
	{
	case RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL:	return D3D12_HEAP_TYPE_DEFAULT;
	case RENDER_MEMORY_PROPERTIES::HOST_VISIBLE:	return D3D12_HEAP_TYPE_UPLOAD;
	default:
		BB_ASSERT(false, "DX12: Tried to make a commandlist with a queue type that does not exist.");
		return D3D12_HEAP_TYPE_DEFAULT;
		break;
	}
}

const D3D12_COMMAND_LIST_TYPE BB::DXConv::CommandListType(const RENDER_QUEUE_TYPE a_RenderQueueType)
{
	switch (a_RenderQueueType)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:				return D3D12_COMMAND_LIST_TYPE_DIRECT;
	case RENDER_QUEUE_TYPE::TRANSFER_COPY:			return D3D12_COMMAND_LIST_TYPE_COPY;
	default:
		BB_ASSERT(false, "DX12: Tried to make a commandlist with a queue type that does not exist.");
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
		break;
	}
}


const D3D12_BLEND BB::DXConv::Blend(const RENDER_BLEND_FACTOR a_BlendFactor)
{
	switch (a_BlendFactor)
	{
	case RENDER_BLEND_FACTOR::ZERO:					return D3D12_BLEND_ZERO;
	case RENDER_BLEND_FACTOR::ONE:					return D3D12_BLEND_ONE;
	case RENDER_BLEND_FACTOR::SRC_ALPHA:			return D3D12_BLEND_SRC_ALPHA;
	case RENDER_BLEND_FACTOR::ONE_MINUS_SRC_ALPHA:	return D3D12_BLEND_INV_SRC_ALPHA;
	default:
		BB_ASSERT(false, "DX12: RENDER_BLEND_FACTOR failed to convert to a D3D12_BLEND.");
		return D3D12_BLEND_ZERO;
		break;
	}
}
const D3D12_BLEND_OP BB::DXConv::BlendOp(const RENDER_BLEND_OP a_BlendOp)
{
	switch (a_BlendOp)
	{
	case RENDER_BLEND_OP::ADD:						return D3D12_BLEND_OP_ADD;
	case RENDER_BLEND_OP::SUBTRACT:					return D3D12_BLEND_OP_SUBTRACT;
	default:
		BB_ASSERT(false, "DX12: RENDER_BLEND_OP failed to convert to a D3D12_BLEND_OP.");
		return D3D12_BLEND_OP_ADD;
		break;
	}
}

const D3D12_LOGIC_OP BB::DXConv::LogicOp(const RENDER_LOGIC_OP a_LogicOp)
{
	switch (a_LogicOp)
	{
	case BB::RENDER_LOGIC_OP::CLEAR:				return D3D12_LOGIC_OP_CLEAR;
	case BB::RENDER_LOGIC_OP::COPY:					return D3D12_LOGIC_OP_COPY;
	default:
		BB_ASSERT(false, "Vulkan: RENDER_LOGIC_OP failed to convert to a D3D12_LOGIC_OP.");
		return D3D12_LOGIC_OP_CLEAR;
		break;
	}
}

DXFence::DXFence(const char* a_Name)
{
	DXASSERT(s_DX12B.device->CreateFence(0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_Fence)),
		"DX12: Failed to create fence.");

	m_FenceEvent = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
	BB_ASSERT(m_FenceEvent != NULL, "WIN, failed to create event.");

	m_LastCompleteValue = 0;
	m_NextFenceValue = 1;

#ifdef _DEBUG
	if (a_Name)
		m_Fence->SetName(UTF8ToUnicodeString(s_DX12TempAllocator, a_Name));
#endif
}

DXFence::~DXFence()
{
	CloseHandle(m_FenceEvent);
	DXRelease(m_Fence);
	memset(this, 0, sizeof(*this));
}

uint64_t DXFence::PollFenceValue()
{
	m_LastCompleteValue = Max(m_LastCompleteValue, m_Fence->GetCompletedValue());
	return m_LastCompleteValue;
}

bool DXFence::IsFenceComplete(const uint64_t a_FenceValue)
{
	if (a_FenceValue > m_LastCompleteValue)
	{
		PollFenceValue();
	}

	return a_FenceValue <= m_LastCompleteValue;
}

void DXFence::WaitFenceCPU(const uint64_t a_FenceValue)
{
	if (IsFenceComplete(a_FenceValue))
	{
		return;
	}

	m_Fence->SetEventOnCompletion(a_FenceValue, m_FenceEvent);
	WaitForSingleObjectEx(m_FenceEvent, INFINITE, false);
	m_LastCompleteValue = a_FenceValue;
}

DXResource::DXResource(const RenderBufferCreateInfo& a_CreateInfo)
	: m_Size(static_cast<UINT>(a_CreateInfo.size))
{
	D3D12_RESOURCE_DESC t_ResourceDesc = {};
	t_ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	t_ResourceDesc.Alignment = 0;
	t_ResourceDesc.Width = static_cast<UINT64>(m_Size);
	t_ResourceDesc.Height = 1;
	t_ResourceDesc.DepthOrArraySize = 1;
	t_ResourceDesc.MipLevels = 1;
	t_ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	t_ResourceDesc.SampleDesc.Count = 1;
	t_ResourceDesc.SampleDesc.Quality = 0;
	t_ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	t_ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12MA::ALLOCATION_DESC t_AllocationDesc = {};
	switch (a_CreateInfo.memProperties)
	{
	case RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL:
		t_AllocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		break;
	case RENDER_MEMORY_PROPERTIES::HOST_VISIBLE:
		t_AllocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		break;
	}

	D3D12_RESOURCE_STATES t_State{};
	switch (a_CreateInfo.usage)
	{
	case RENDER_BUFFER_USAGE::INDEX:
		t_State = D3D12_RESOURCE_STATE_COMMON;
		break;
	case RENDER_BUFFER_USAGE::STAGING:
		t_State = D3D12_RESOURCE_STATE_GENERIC_READ;
		BB_ASSERT(t_AllocationDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD,
			"DX12, tries to make an upload resource but the heap type is not upload!");
		break;
	}

	DXASSERT(s_DX12B.DXMA->CreateResource(
		&t_AllocationDesc,
		&t_ResourceDesc,
		t_State,
		NULL,
		&m_Allocation,
		IID_PPV_ARGS(&m_Resource)),
		"DX12: Failed to create resource using D3D12 Memory Allocator");

#ifdef _DEBUG
	if (a_CreateInfo.name)
		m_Resource->SetName(UTF8ToUnicodeString(s_DX12TempAllocator, a_CreateInfo.name));
#endif
}

DXResource::~DXResource()
{
	m_Resource->Release();
	m_Allocation->Release();
	memset(this, 0, sizeof(*this));
}

DXImage::DXImage(const RenderImageCreateInfo& a_CreateInfo)
{
	D3D12_RESOURCE_DESC t_Desc{};
	t_Desc.Alignment = 0;
	t_Desc.Width = static_cast<UINT64>(a_CreateInfo.width);
	t_Desc.Height = a_CreateInfo.height;
	t_Desc.DepthOrArraySize = a_CreateInfo.arrayLayers;
	t_Desc.MipLevels = a_CreateInfo.mipLevels;
	t_Desc.SampleDesc.Count = 1;
	t_Desc.SampleDesc.Quality = 0;
	t_Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	
	D3D12_CLEAR_VALUE* t_ClearValue = nullptr;
	bool t_IsDepth = false;

	D3D12_RESOURCE_STATES t_StartState{};
	switch (a_CreateInfo.format)
	{
	case RENDER_IMAGE_FORMAT::RGBA8_SRGB:
		t_Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		t_Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		t_StartState = D3D12_RESOURCE_STATE_COMMON;

		m_TextureData.format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		break;
	case RENDER_IMAGE_FORMAT::RGBA8_UNORM:
		t_Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		t_Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		t_StartState = D3D12_RESOURCE_STATE_COMMON;

		m_TextureData.format = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case RENDER_IMAGE_FORMAT::DEPTH_STENCIL:
		t_Desc.Format = DEPTH_FORMAT;
		t_Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		t_StartState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		t_ClearValue = BBnew(
			s_DX12TempAllocator,
			D3D12_CLEAR_VALUE);
		t_ClearValue->Format = t_Desc.Format;
		t_ClearValue->DepthStencil.Depth = 1.0f;
		t_ClearValue->DepthStencil.Stencil = 0;
		t_IsDepth = true;
		break;
	default:
		BB_ASSERT(false, "DX12, image usage not supported!")
			break;
	}

	switch (a_CreateInfo.type)
	{
	case RENDER_IMAGE_TYPE::TYPE_2D:
		t_Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		break;
	default:
		BB_ASSERT(false, "DX12, image type not supported!")
		break;
	}

	D3D12MA::ALLOCATION_DESC t_AllocationDesc = {};
	t_AllocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	DXASSERT(s_DX12B.DXMA->CreateResource(
		&t_AllocationDesc,
		&t_Desc,
		t_StartState,
		t_ClearValue,
		&m_Allocation,
		IID_PPV_ARGS(&m_Resource)),
		"DX12: Failed to create resource using D3D12 Memory Allocator");

	if (t_IsDepth)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC t_DepthStencilDesc = {};
		t_DepthStencilDesc.Format = DEPTH_FORMAT;
		t_DepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		t_DepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_CPU_DESCRIPTOR_HANDLE t_HeapHandle = s_DX12B.dsvHeap.Allocate();
		m_DepthData.dsvHandle = t_HeapHandle;
		s_DX12B.device->CreateDepthStencilView(m_Resource, &t_DepthStencilDesc, m_DepthData.dsvHandle);
	}

#ifdef _DEBUG
	if (a_CreateInfo.name)
		m_Resource->SetName(UTF8ToUnicodeString(s_DX12TempAllocator, a_CreateInfo.name));
#endif
}

DXImage::~DXImage()
{
	if (m_Resource->GetDesc().Flags == D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		//free the descriptor info for DSV.

	m_Resource->Release();
	m_Allocation->Release();
	memset(this, 0, sizeof(*this));
}

DXSampler::DXSampler(const SamplerCreateInfo& a_Info)
{
	//Some standard stuff that will change later.
	m_Desc.MipLODBias = 0;
	m_Desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	m_Desc.BorderColor[0] = 0.0f;
	m_Desc.BorderColor[1] = 0.0f;
	m_Desc.BorderColor[2] = 0.0f;
	m_Desc.BorderColor[3] = 0.0f;
	m_Desc.MinLOD = 0.0f;
	m_Desc.MaxLOD = 0.0f;
	UpdateSamplerInfo(a_Info);
}
DXSampler::~DXSampler()
{
	memset(this, 0, sizeof(*this));
}

void DXSampler::UpdateSamplerInfo(const SamplerCreateInfo& a_Info)
{
	m_Desc.AddressU = DXConv::AddressMode(a_Info.addressModeU);
	m_Desc.AddressV = DXConv::AddressMode(a_Info.addressModeV);
	m_Desc.AddressW = DXConv::AddressMode(a_Info.addressModeW);
	switch (a_Info.filter)
	{
	case SAMPLER_FILTER::NEAREST:
		m_Desc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		break;
	case SAMPLER_FILTER::LINEAR:
		m_Desc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		break;
	default:
		BB_ASSERT(false, "DX12, does not support this type of sampler filter!");
		break;
	}
	m_Desc.MinLOD = a_Info.minLod;
	m_Desc.MaxLOD = a_Info.maxLod;
	m_Desc.MaxAnisotropy = static_cast<UINT>(a_Info.maxAnistoropy);
}

DXCommandQueue::DXCommandQueue(const RenderCommandQueueCreateInfo& a_CreateInfo)
	: m_Fence(a_CreateInfo.name)
{
	D3D12_COMMAND_LIST_TYPE t_Type{};
	switch (a_CreateInfo.queue)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		t_Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		break;
	case RENDER_QUEUE_TYPE::TRANSFER_COPY:
		t_Type = D3D12_COMMAND_LIST_TYPE_COPY;
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
		t_Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		break;
	default:
		BB_ASSERT(false, "DX12: Tried to make a command queue with a queue type that does not exist.");
		t_Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		break;
	}

	D3D12_COMMAND_QUEUE_DESC t_QueueDesc{};
	t_QueueDesc.Type = t_Type;
	t_QueueDesc.NodeMask = 0;
	m_QueueType = t_Type;
	DXASSERT(s_DX12B.device->CreateCommandQueue(&t_QueueDesc,
		IID_PPV_ARGS(&m_Queue)),
		"DX12: Failed to create queue.");

#ifdef _DEBUG
	if (a_CreateInfo.name)
		m_Queue->SetName(UTF8ToUnicodeString(s_DX12TempAllocator, a_CreateInfo.name));
#endif
}

DXCommandQueue::DXCommandQueue(const D3D12_COMMAND_LIST_TYPE a_CommandType, ID3D12CommandQueue* a_CommandQueue)
	: m_Fence("MAYBE PRESENT QUEUE FENCE")
{
	m_Queue = a_CommandQueue;
	m_QueueType = a_CommandType;
}

DXCommandQueue::~DXCommandQueue()
{
	DXRelease(m_Queue);
	memset(this, 0, sizeof(*this));
}

void DXCommandQueue::InsertWait(const uint64_t a_FenceValue)
{
	m_Queue->Wait(m_Fence.m_Fence, a_FenceValue);
}

void DXCommandQueue::InsertWaitQueue(const DXCommandQueue& a_WaitQueue)
{
	m_Queue->Wait(a_WaitQueue.GetFence(), a_WaitQueue.GetNextFenceValue() - 1);
}

void DXCommandQueue::InsertWaitQueueFence(const DXCommandQueue& a_WaitQueue, const uint64_t a_FenceValue)
{
	m_Queue->Wait(a_WaitQueue.GetFence(), a_FenceValue);
}

void DXCommandQueue::ExecuteCommandlist(ID3D12CommandList** a_CommandLists, const uint32_t a_CommandListCount)
{
	m_Queue->ExecuteCommandLists(a_CommandListCount, a_CommandLists);
}

void DXCommandQueue::SignalQueue()
{
	m_Queue->Signal(m_Fence.m_Fence, m_Fence.m_NextFenceValue);
	++m_Fence.m_NextFenceValue;
}

void DXCommandQueue::SignalQueue(DXFence& a_Fence)
{
	m_Queue->Signal(a_Fence.m_Fence, a_Fence.m_NextFenceValue);
	++a_Fence.m_NextFenceValue;
}

DXCommandAllocator::DXCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo)
{
	m_ListSize = a_CreateInfo.commandListCount;
	m_Type = DXConv::CommandListType(a_CreateInfo.queueType);
	s_DX12B.device->CreateCommandAllocator(m_Type, IID_PPV_ARGS(&m_Allocator));
	m_Lists.CreatePool(s_DX12Allocator, m_ListSize);
	//pre-reserve the commandlists, doing it here so it becomes easy to make a cross-API renderer with vulkan.
	for (size_t i = 0; i < m_ListSize; i++)
	{
		//we name them when we use them.
		new (&m_Lists.data()[i]) DXCommandList(* this);
	}

#ifdef _DEBUG
	if (a_CreateInfo.name)
		m_Allocator->SetName(UTF8ToUnicodeString(s_DX12TempAllocator, a_CreateInfo.name));
#endif
}

DXCommandAllocator::~DXCommandAllocator()
{
	m_Lists.DestroyPool(s_DX12Allocator);
	DXRelease(m_Allocator);
	memset(this, 0, sizeof(*this));
}

void DXCommandAllocator::FreeCommandList(DXCommandList* a_CmdList)
{
	m_Lists.Free(a_CmdList);
}

void DXCommandAllocator::ResetCommandAllocator()
{
	m_Allocator->Reset();
}

DXCommandList* DXCommandAllocator::GetCommandList()
{
	return m_Lists.Get();
}

DXCommandList::DXCommandList(DXCommandAllocator& a_CmdAllocator)
	: m_CmdAllocator(a_CmdAllocator)
{
	DXASSERT(s_DX12B.device->CreateCommandList(0,
		m_CmdAllocator.m_Type,
		m_CmdAllocator.m_Allocator,
		nullptr,
		IID_PPV_ARGS(&m_List)),
		"DX12: Failed to allocate commandlist.");
	m_List->Close();
	//Caching variables just null.
	rtv = nullptr;
	boundPipeline = nullptr;
}

DXCommandList::~DXCommandList()
{
	DXRelease(m_List);
	memset(this, 0, sizeof(*this));
}

void DXCommandList::Reset(ID3D12PipelineState* a_PipeState)
{
	m_List->Reset(m_CmdAllocator.m_Allocator, a_PipeState);
}

void DXCommandList::Close()
{
	m_List->Close();
	rtv = nullptr;
}

void DXCommandList::Free()
{
	m_CmdAllocator.FreeCommandList(this);
}

DXDescriptorHeap::DXDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE a_HeapType, uint32_t a_DescriptorCount, bool a_ShaderVisible, const char* a_Name)
	:	m_HeapType(a_HeapType), m_MaxDescriptors(a_DescriptorCount), m_IncrementSize(s_DX12B.device->GetDescriptorHandleIncrementSize(a_HeapType))
{
	m_HeapGPUStart = {};
	D3D12_DESCRIPTOR_HEAP_DESC t_HeapInfo{};
	t_HeapInfo.Type = a_HeapType;
	t_HeapInfo.NumDescriptors = a_DescriptorCount;
	t_HeapInfo.Flags = a_ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	t_HeapInfo.NodeMask = 0;

	DXASSERT(s_DX12B.device->CreateDescriptorHeap(&t_HeapInfo, IID_PPV_ARGS(&m_DescriptorHeap)),
		"DX12, Failed to create descriptor heap.");
#ifdef _DEBUG
	if (a_Name)
		m_DescriptorHeap->SetName(UTF8ToUnicodeString(s_DX12TempAllocator, a_Name));
#endif

	m_HeapCPUStart = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	if (a_ShaderVisible)
	{
		m_HeapGPUStart = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	}
}

DXDescriptorHeap::~DXDescriptorHeap()
{
	DXRelease(m_DescriptorHeap);
	m_DescriptorHeap = nullptr;
	memset(this, 0, sizeof(*this));
}

DescriptorAllocation DXDescriptorHeap::Allocate(const RDescriptor a_Layout, const uint32_t a_HeapOffset)
{
	DXDescriptor* t_Desc = reinterpret_cast<DXDescriptor*>(a_Layout.handle);
	const uint64_t t_AllocSize = static_cast<uint64_t>(t_Desc->descriptorCount) * m_IncrementSize;

	DescriptorAllocation t_Allocation{};
	t_Allocation.descriptorCount = t_Desc->descriptorCount;
	t_Allocation.offset = a_HeapOffset;
	t_Allocation.descriptor = a_Layout;
	t_Allocation.bufferStart = reinterpret_cast<void*>(m_HeapCPUStart.ptr);

	m_InUse += t_Allocation.descriptorCount;
	return t_Allocation;
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthDescriptorHeap::Allocate()
{
	D3D12_CPU_DESCRIPTOR_HANDLE t_Handle = heap->GetCPUDescriptorHandleForHeapStart();
	t_Handle.ptr += static_cast<uint64_t>(pos) * s_DX12B.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	++pos;
	BB_ASSERT(max > pos, "DX12, too many depth allocations!");
	return t_Handle;
}
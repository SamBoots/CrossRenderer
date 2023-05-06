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

const D3D12_RESOURCE_STATES BB::DXConv::ResourceStates(const RENDER_BUFFER_USAGE a_Usage)
{
	switch (a_Usage)
	{
	case RENDER_BUFFER_USAGE::VERTEX:				return D3D12_RESOURCE_STATE_COPY_DEST;
	case RENDER_BUFFER_USAGE::INDEX:				return D3D12_RESOURCE_STATE_COPY_DEST;
	case RENDER_BUFFER_USAGE::STORAGE:				return D3D12_RESOURCE_STATE_COPY_DEST;
	case RENDER_BUFFER_USAGE::STAGING:				return D3D12_RESOURCE_STATE_GENERIC_READ;
	default:
		BB_ASSERT(false, "DX12, this Buffer Usage not supported by DX12!");
		return D3D12_RESOURCE_STATE_COMMON;
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
	case RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY:		return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
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

//Safely releases a type by setting it back to null
void BB::DXRelease(IUnknown* a_Obj)
{
	if (a_Obj)
		a_Obj->Release();
}

DXFence::DXFence()
{
	DXASSERT(s_DX12B.device->CreateFence(0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_Fence)),
		"DX12: Failed to create fence.");

	m_FenceEvent = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
	BB_ASSERT(m_FenceEvent != NULL, "WIN, failed to create event.");

	m_LastCompleteValue = 0;
	m_NextFenceValue = 1;
}

DXFence::~DXFence()
{
	CloseHandle(m_FenceEvent);
	DXRelease(m_Fence);
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

DXResource::DXResource(const RENDER_BUFFER_USAGE a_BufferUsage, const RENDER_MEMORY_PROPERTIES a_MemProperties, const uint64_t a_Size)
	: m_Size(static_cast<UINT>(a_Size))
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

	switch (a_MemProperties)
	{
	case RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL:
		t_AllocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		break;
	case RENDER_MEMORY_PROPERTIES::HOST_VISIBLE:
		t_AllocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		break;
	}
	
	D3D12_RESOURCE_STATES t_State{};
	switch (a_BufferUsage)
	{
	case RENDER_BUFFER_USAGE::VERTEX:
		t_State = D3D12_RESOURCE_STATE_COMMON;
		break;
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
}

DXResource::~DXResource()
{
	m_Resource->Release();
	m_Allocation->Release();
}

DXImage::DXImage(const RenderImageCreateInfo& a_Info)
{
	D3D12_RESOURCE_DESC t_Desc{};
	t_Desc.Alignment = 0;
	t_Desc.Width = static_cast<UINT64>(a_Info.width);
	t_Desc.Height = a_Info.height;
	t_Desc.DepthOrArraySize = a_Info.arrayLayers;
	t_Desc.MipLevels = a_Info.mipLevels;
	t_Desc.SampleDesc.Count = 1;
	t_Desc.SampleDesc.Quality = 0;
	t_Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	
	D3D12_CLEAR_VALUE* t_ClearValue = nullptr;
	bool t_IsDepth = false;

	D3D12_RESOURCE_STATES t_StartState{};
	switch (a_Info.format)
	{
	case RENDER_IMAGE_FORMAT::RGBA8_SRGB:
		t_Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		t_Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		t_StartState = D3D12_RESOURCE_STATE_COMMON;
		break;
	case RENDER_IMAGE_FORMAT::RGBA8_UNORM:
		t_Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		t_Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		t_StartState = D3D12_RESOURCE_STATE_COMMON;
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

	switch (a_Info.type)
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

		DescriptorHeapHandle t_HeapHandle = s_DX12B.dsvHeap->Allocate(1);
		m_DepthData.dsvHandle = t_HeapHandle.cpuHandle;
		s_DX12B.device->CreateDepthStencilView(m_Resource, &t_DepthStencilDesc, m_DepthData.dsvHandle);
	}
}

DXImage::~DXImage()
{
	if (m_Resource->GetDesc().Flags == D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		//free the descriptor info for DSV.

	m_Resource->Release();
	m_Allocation->Release();
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

DXCommandQueue::DXCommandQueue(const D3D12_COMMAND_LIST_TYPE a_CommandType)
	: m_Fence()
{
	D3D12_COMMAND_QUEUE_DESC t_QueueDesc{};
	t_QueueDesc.Type = a_CommandType;
	t_QueueDesc.NodeMask = 0;
	m_QueueType = a_CommandType;
	DXASSERT(s_DX12B.device->CreateCommandQueue(&t_QueueDesc,
		IID_PPV_ARGS(&m_Queue)),
		"DX12: Failed to create queue.");
}

DXCommandQueue::DXCommandQueue(const D3D12_COMMAND_LIST_TYPE a_CommandType, ID3D12CommandQueue* a_CommandQueue)
	: m_Fence()
{
	m_Queue = a_CommandQueue;
	m_QueueType = a_CommandType;
}

DXCommandQueue::~DXCommandQueue()
{
	DXRelease(m_Queue);
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

DXCommandAllocator::DXCommandAllocator(const D3D12_COMMAND_LIST_TYPE a_QueueType, const uint32_t a_CommandListCount)
{
	m_ListSize = a_CommandListCount;
	m_Type = a_QueueType;
	s_DX12B.device->CreateCommandAllocator(m_Type, IID_PPV_ARGS(&m_Allocator));
	m_Lists.CreatePool(s_DX12Allocator, m_ListSize);
	//pre-reserve the commandlists, doing it here so it becomes easy to make a cross-API renderer with vulkan.
	for (size_t i = 0; i < m_ListSize; i++)
	{
		new (&m_Lists.data()[i]) DXCommandList(* this);
	}
}

DXCommandAllocator::~DXCommandAllocator()
{
	m_Lists.DestroyPool(s_DX12Allocator);
	DXRelease(m_Allocator);
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
}

DXCommandList::~DXCommandList()
{
	DXRelease(m_List);
	rtv = nullptr;
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

DescriptorHeap::DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE a_HeapType,
	uint32_t a_DescriptorCount, bool a_ShaderVisible)
{
	m_HeapType = a_HeapType;
	m_MaxDescriptors = a_DescriptorCount;
	m_HeapGPUStart = {};
	D3D12_DESCRIPTOR_HEAP_DESC t_HeapInfo{};
	t_HeapInfo.Type = a_HeapType;
	t_HeapInfo.NumDescriptors = a_DescriptorCount;
	t_HeapInfo.Flags = a_ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	t_HeapInfo.NodeMask = 0;

	DXASSERT(s_DX12B.device->CreateDescriptorHeap(&t_HeapInfo, IID_PPV_ARGS(&m_DescriptorHeap)),
		"DX12, Failed to create descriptor heap.");

	m_HeapCPUStart = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	if (a_ShaderVisible)
	{
		m_HeapGPUStart = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	}

	m_IncrementSize = s_DX12B.device->GetDescriptorHandleIncrementSize(a_HeapType);
	
}

DescriptorHeap::~DescriptorHeap()
{
	DXRelease(m_DescriptorHeap);
	m_DescriptorHeap = nullptr;
}

DescriptorHeapHandle DescriptorHeap::Allocate(const uint32_t a_Count)
{
	DescriptorHeapHandle t_AllocHandle{};
	BB_ASSERT((m_InUse + a_Count < m_MaxDescriptors),
		"DX12, Descriptorheap has no more descriptors left!");

	t_AllocHandle.heap = m_DescriptorHeap;
	t_AllocHandle.cpuHandle.ptr = m_HeapCPUStart.ptr + static_cast<uintptr_t>(m_InUse * m_IncrementSize);
	t_AllocHandle.gpuHandle.ptr = m_HeapGPUStart.ptr + static_cast<uintptr_t>(m_InUse * m_IncrementSize);
	t_AllocHandle.incrementSize = m_IncrementSize;
	t_AllocHandle.heapIndex = m_InUse;
	t_AllocHandle.count = a_Count;

	m_InUse += a_Count;

	return t_AllocHandle;
}

void DescriptorHeap::Reset()
{
	m_InUse = 0;
}
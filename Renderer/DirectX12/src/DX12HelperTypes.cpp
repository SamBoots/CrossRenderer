#include "DX12HelperTypes.h"
#include "Utils.h"

using namespace BB;

const D3D12_RESOURCE_STATES BB::DXConv::ResourceStates(const RENDER_BUFFER_USAGE a_Usage)
{
	switch (a_Usage)
	{
	case RENDER_BUFFER_USAGE::VERTEX:
		return D3D12_RESOURCE_STATE_COPY_DEST;
		break;
	case RENDER_BUFFER_USAGE::INDEX:
		return D3D12_RESOURCE_STATE_COPY_DEST;
		break;
	case RENDER_BUFFER_USAGE::STORAGE:
		return D3D12_RESOURCE_STATE_COPY_DEST;
		break;
	case RENDER_BUFFER_USAGE::STAGING:
		return D3D12_RESOURCE_STATE_GENERIC_READ;
		break;
	default:
		BB_ASSERT(false, "DX12, Buffer Usage not supported by DX12!");
		return D3D12_RESOURCE_STATE_COMMON;
		break;
	}
}

const D3D12_HEAP_TYPE BB::DXConv::HeapType(const RENDER_MEMORY_PROPERTIES a_Properties)
{
	switch (a_Properties)
	{
	case RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL:
		return D3D12_HEAP_TYPE_DEFAULT;
		break;
	case RENDER_MEMORY_PROPERTIES::HOST_VISIBLE:
		return D3D12_HEAP_TYPE_UPLOAD;
		break;
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
	case RENDER_QUEUE_TYPE::GRAPHICS:
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
		break;
	case RENDER_QUEUE_TYPE::TRANSFER_COPY:
		return D3D12_COMMAND_LIST_TYPE_COPY;
		break;
	default:
		BB_ASSERT(false, "DX12: Tried to make a commandlist with a queue type that does not exist.");
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
		break;
	}
}

//Safely releases a type by setting it back to null
void BB::DXRelease(IUnknown* a_Obj)
{
	if (a_Obj)
		a_Obj->Release();
}

DXResource::DXResource(D3D12MA::Allocator* a_ResourceAllocator, const RENDER_BUFFER_USAGE a_BufferUsage, const RENDER_MEMORY_PROPERTIES a_MemProperties, const uint64_t a_Size)
{
	D3D12_RESOURCE_DESC t_ResourceDesc = {};
	t_ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	t_ResourceDesc.Alignment = 0;
	t_ResourceDesc.Width = a_Size;
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
	

	switch (a_BufferUsage)
	{
	case RENDER_BUFFER_USAGE::VERTEX:
		m_CurrentState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		break;
	case RENDER_BUFFER_USAGE::INDEX:
		m_CurrentState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
		break;
	case RENDER_BUFFER_USAGE::STAGING:
		m_CurrentState = D3D12_RESOURCE_STATE_GENERIC_READ;
		BB_ASSERT(t_AllocationDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD,
			"DX12, tries to make an upload resource but the heap type is not upload!");
		break;
	}

	DXASSERT(a_ResourceAllocator->CreateResource(
		&t_AllocationDesc,
		&t_ResourceDesc,
		m_CurrentState,
		NULL,
		&m_Allocation,
		IID_PPV_ARGS(&m_Resource)),
		"DX12: Failed to create resource using D3D12 Memory Allocator");


	switch (a_BufferUsage)
	{
	case RENDER_BUFFER_USAGE::VERTEX:
		m_View.vertexView.BufferLocation = m_Resource->GetGPUVirtualAddress();
		m_View.vertexView.StrideInBytes = sizeof(Vertex);
		m_View.vertexView.SizeInBytes = static_cast<UINT>(a_Size);
		break;
	case RENDER_BUFFER_USAGE::INDEX:
		m_View.indexView.BufferLocation = m_Resource->GetGPUVirtualAddress();
		m_View.indexView.Format = DXGI_FORMAT_R32_UINT;
		m_View.indexView.SizeInBytes = static_cast<UINT>(a_Size);
		break;
	default:
		m_View.constantView.BufferLocation = m_Resource->GetGPUVirtualAddress();
		m_View.constantView.SizeInBytes = static_cast<UINT>(a_Size);
		break;
	}
}

DXResource::~DXResource()
{
	m_Resource->Release();
	m_Allocation->Release();
}

DXCommandQueue::DXCommandQueue(ID3D12Device* a_Device, const D3D12_COMMAND_LIST_TYPE a_CommandType)
{
	D3D12_COMMAND_QUEUE_DESC t_QueueDesc{};
	t_QueueDesc.Type = a_CommandType;
	t_QueueDesc.NodeMask = 0;
	m_QueueType = a_CommandType;
	DXASSERT(a_Device->CreateCommandQueue(&t_QueueDesc, 
		IID_PPV_ARGS(&m_Queue)),
		"DX12: Failed to create queue.");

	DXASSERT(a_Device->CreateFence(0, 
		D3D12_FENCE_FLAG_NONE, 
		IID_PPV_ARGS(&m_Fence)),
		"DX12: Failed to create fence in command queue.");

	m_FenceEvent = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
	BB_ASSERT(m_FenceEvent != NULL, "WIN, failed to create event.");

	m_LastCompleteValue = 0;
	m_NextFenceValue = 1;
}

DXCommandQueue::DXCommandQueue(ID3D12Device* a_Device, const D3D12_COMMAND_LIST_TYPE a_CommandType, ID3D12CommandQueue* a_CommandQueue)
{
	m_Queue = a_CommandQueue;
	m_QueueType = a_CommandType;
	DXASSERT(a_Device->CreateFence(0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_Fence)),
		"DX12: Failed to create fence in command queue.");

	m_FenceEvent = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
	BB_ASSERT(m_FenceEvent != NULL, "WIN, failed to create event.");

	m_LastCompleteValue = 0;
	m_NextFenceValue = 1;
}

DXCommandQueue::~DXCommandQueue()
{
	CloseHandle(m_FenceEvent);

	DXRelease(m_Queue);
	DXRelease(m_Fence);
}

uint64_t DXCommandQueue::PollFenceValue()
{
	m_LastCompleteValue = BB::Math::Max(m_LastCompleteValue, m_Fence->GetCompletedValue());
	return m_LastCompleteValue;
}

bool DXCommandQueue::IsFenceComplete(const uint64_t a_FenceValue)
{
	if (a_FenceValue > m_LastCompleteValue)
	{
		PollFenceValue();
	}

	return a_FenceValue <= m_LastCompleteValue;
}

void DXCommandQueue::WaitFenceCPU(const uint64_t a_FenceValue)
{
	if (IsFenceComplete(a_FenceValue))
	{
		return;
	}

	m_Fence->SetEventOnCompletion(a_FenceValue, m_FenceEvent);
	WaitForSingleObjectEx(m_FenceEvent, INFINITE, false);
	m_LastCompleteValue = a_FenceValue;
}

void DXCommandQueue::InsertWait(const uint64_t a_FenceValue)
{
	m_Queue->Wait(m_Fence, a_FenceValue);
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
	m_Queue->Signal(m_Fence, m_NextFenceValue);
	++m_NextFenceValue;
}

DXCommandAllocator::DXCommandAllocator(ID3D12Device* a_Device, const D3D12_COMMAND_LIST_TYPE a_QueueType, const uint32_t a_CommandListCount)
{
	m_ListSize = a_CommandListCount;
	m_Type = a_QueueType;
	a_Device->CreateCommandAllocator(m_Type, IID_PPV_ARGS(&m_Allocator));
	m_Lists.CreatePool(s_DX12Allocator, m_ListSize);
	//pre-reserve the commandlists, doing it here so it becomes easy to make a cross-API renderer with vulkan.
	for (size_t i = 0; i < m_ListSize; i++)
	{
		new (&m_Lists.data()[i]) DXCommandList(a_Device, *this);
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

DXCommandList::DXCommandList(ID3D12Device* a_Device, DXCommandAllocator& a_CmdAllocator)
	: m_CmdAllocator(a_CmdAllocator)
{
	DXASSERT(a_Device->CreateCommandList(0,
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

DescriptorHeap::DescriptorHeap(ID3D12Device* a_Device,
	D3D12_DESCRIPTOR_HEAP_TYPE a_HeapType,
	uint32_t a_DescriptorCount,
	bool a_ShaderVisible)
{
	m_HeapType = a_HeapType;
	m_MaxDescriptors = a_DescriptorCount;

	D3D12_DESCRIPTOR_HEAP_DESC t_HeapInfo;
	t_HeapInfo.Type = a_HeapType;
	t_HeapInfo.NumDescriptors = a_DescriptorCount;
	t_HeapInfo.Flags = a_ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	t_HeapInfo.NodeMask = 0;

	DXASSERT(a_Device->CreateDescriptorHeap(&t_HeapInfo, IID_PPV_ARGS(&m_DescriptorHeap)),
		"DX12, Failed to create descriptor heap.");

	m_HeapCPUStart = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	if (a_ShaderVisible)
	{
		m_HeapGPUStart = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	}

	m_IncrementSize = a_Device->GetDescriptorHandleIncrementSize(a_HeapType);
	
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

	t_AllocHandle.cpuHandle.ptr = m_HeapCPUStart.ptr + static_cast<uintptr_t>(m_InUse * m_IncrementSize);
	t_AllocHandle.gpuHandle.ptr = m_HeapGPUStart.ptr + static_cast<uintptr_t>(m_InUse * m_IncrementSize);
	t_AllocHandle.heapIndex = m_InUse;
	t_AllocHandle.count = a_Count;

	m_InUse += a_Count;

	return t_AllocHandle;
}

void DescriptorHeap::Reset()
{
	m_InUse = 0;
}
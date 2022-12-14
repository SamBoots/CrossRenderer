#include "DX12HelperTypes.h"
#include "Utils.h"

const D3D12_RESOURCE_STATES DXConv::ResourceStates(const RENDER_BUFFER_USAGE a_Usage)
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

const D3D12_HEAP_TYPE DXConv::HeapType(const RENDER_MEMORY_PROPERTIES a_Properties)
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

const D3D12_COMMAND_LIST_TYPE DXConv::CommandListType(const RENDER_QUEUE_TYPE a_RenderQueueType)
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

bool IsFenceComplete(const Fence a_Fence)
{
	return a_Fence.fence->GetCompletedValue() >= a_Fence.fenceValue;
}

void WaitFence(const Fence a_Fence, const uint64_t a_FenceValue)
{
	if (a_Fence.fence->GetCompletedValue() < a_Fence.fenceValue)
	{
		HANDLE t_Event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		BB_ASSERT(t_Event, "DX12, failed to create a fence event.");

		DXASSERT(a_Fence.fence->SetEventOnCompletion(a_FenceValue, t_Event),
			"DX12, failed to set event on completion");

		::CloseHandle(t_Event);
	}
}

DXCommandQueue::DXCommandQueue(ID3D12Device* a_Device, const D3D12_COMMAND_LIST_TYPE a_CommandType)
{
	D3D12_COMMAND_QUEUE_DESC t_QueueDesc{};
	t_QueueDesc.Type = a_CommandType;
	t_QueueDesc.NodeMask = 0;
	DXASSERT(a_Device->CreateCommandQueue(&t_QueueDesc, 
		IID_PPV_ARGS(&m_Queue)),
		"DX12: Failed to create queue.");

	DXASSERT(a_Device->CreateFence(0, 
		D3D12_FENCE_FLAG_NONE, 
		IID_PPV_ARGS(&m_Fence)),
		"DX12: Failed to create fence in command queue.");

	m_FenceEvent = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
	BB_ASSERT(m_FenceEvent != NULL, "WIN, failed to create event.");
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
	//Call the destructor before removing the pool memory.
	for (size_t i = 0; i < m_ListSize; i++)
	{
		m_Lists.data()[i].~DXCommandList();
	}

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

DXCommandList::DXCommandList(ID3D12Device* a_Device, struct DXCommandAllocator& a_CmdAllocator)
	: m_CmdAllocator(a_CmdAllocator)
{
	DXASSERT(a_Device->CreateCommandList(0,
		m_CmdAllocator.m_Type,
		m_CmdAllocator.m_Allocator,
		nullptr,
		IID_PPV_ARGS(&m_List)),
		"DX12: Failed to allocate commandlist.");

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

//Safely releases a type by setting it back to null
void DXRelease(IUnknown* a_Obj)
{
	if (a_Obj)
		a_Obj->Release();
}
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

	DX12Release(m_Queue);
	DX12Release(m_Fence);
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
	m_NextFenceValue++;
}


DXCommandList* GetCommandList(DXCommandAllocator& a_CmdAllocator, ID3D12Device* a_Device)
{
	DXCommandList* t_CommandList = lists.Get();
	DXASSERT(a_Device->CreateCommandList(0,
		a_CmdAllocator.type,
		a_CmdAllocator.allocator,
		nullptr,
		IID_PPV_ARGS(&t_CommandList->list)),
		"DX12: Failed to allocate commandlist.");

	t_CommandList->allocator = this;

	return t_CommandList;
}

void FreeCommandList(DXCommandAllocator& cmdAllocator, DXCommandList* t_CmdList)
{
	DX12Release(t_CmdList->list);
	//CHecking to see if the pointer is from the list is done in list.Free
	lists.Free(t_CmdList);
}

void ResetCommandLists(DXCommandList* a_CommandLists, const uint32_t a_Count)
{
	for (uint32_t i = 0; i < a_Count; i++)
	{
		a_CommandLists[i].list->Reset(a_CommandLists->allocator->allocator, 0);
	}
}

//Safely releases a type by setting it back to null
void DX12Release(IUnknown* a_Obj)
{
	if (a_Obj)
		a_Obj->Release();
}
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#ifdef _DEBUG
#define DXASSERT(a_HRESULT, a_Msg)\
	if (a_HRESULT != S_OK)\
		BB_ASSERT(false, a_Msg);\

#else
#define DXASSERT(a_HRESULT, a_Msg) a_HRESULT
#endif //_DEBUG

#include "RenderBackendCommon.h"

static FreelistAllocator_t s_DX12Allocator{ mbSize * 2 };

using namespace BB;

namespace DXConv
{
	const D3D12_RESOURCE_STATES ResourceStates(const RENDER_BUFFER_USAGE a_Usage);
	const D3D12_HEAP_TYPE HeapType(const RENDER_MEMORY_PROPERTIES a_Properties);
	const D3D12_COMMAND_LIST_TYPE CommandListType(const RENDER_QUEUE_TYPE a_RenderQueueType);
}

union DX12BufferView
{
	D3D12_VERTEX_BUFFER_VIEW vertexView;
	D3D12_INDEX_BUFFER_VIEW indexView;
	D3D12_CONSTANT_BUFFER_VIEW_DESC constantView;
};

struct DX12Device
{
	IDXGIAdapter1* adapter;
	ID3D12Device* logicalDevice;

	ID3D12DebugDevice1* debugDevice;
};

struct DX12Swapchain
{
	UINT width;
	UINT height;
	IDXGISwapChain3* swapchain;
};

struct DX12FrameBuffer
{
	ID3D12Resource** renderTargets; //dyn alloc

	ID3D12DescriptorHeap* rtvHeap;
	D3D12_VIEWPORT viewport;
	D3D12_RECT surfaceRect;
};

class DXCommandQueue
{
public:
	DXCommandQueue(ID3D12Device* a_Device, const D3D12_COMMAND_LIST_TYPE a_CommandType);
	~DXCommandQueue();

	uint64_t PollFenceValue();
	bool IsFenceComplete(const uint64_t a_FenceValue);

	void WaitFenceCPU(const uint64_t a_FenceValue);
	void WaitIdle() { WaitFenceCPU(m_NextFenceValue - 1); }

	void InsertWait(const uint64_t a_FenceValue);
	void InsertWaitQueue(const DXCommandQueue& a_WaitQueue);
	void InsertWaitQueueFence(const DXCommandQueue& a_WaitQueue, const uint64_t a_FenceValue);

	void ExecuteCommandlist(ID3D12CommandList** a_CommandLists, const uint32_t a_CommandListCount);

	ID3D12CommandQueue* GetQueue() const { return m_Queue; }
	ID3D12Fence* GetFence() const { return m_Fence; }
	uint64_t GetNextFenceValue() const { return m_NextFenceValue; }

private:
	ID3D12CommandQueue* m_Queue;
	D3D12_COMMAND_LIST_TYPE m_QueueType;
	ID3D12Fence* m_Fence;
	uint64_t m_NextFenceValue;
	uint64_t m_LastCompleteValue;
	HANDLE m_FenceEvent;

	friend class DXCommandAllocator; //Allocator should have access to the QueueType.
};

struct DXCommandAllocator* a_CmdAllocator;

class DXCommandList
{
public:
	DXCommandList(ID3D12Device* a_Device, DXCommandAllocator& a_CmdAllocator);
	~DXCommandList();

	//Possible caching for efficiency, might go for specific commandlist types.
	ID3D12Resource* rtv;

	ID3D12GraphicsCommandList* List() const { return m_List; }
	//Commandlist holds the allocator info, so use this instead of List()->Reset
	void Reset(ID3D12PipelineState* a_PipeState = nullptr);
	//Prefer to use this Close instead of List()->Close() for error testing purposes
	void Close();

private:
	union
	{
		ID3D12GraphicsCommandList* m_List;
	};
	DXCommandAllocator& m_CmdAllocator;
};

class DXCommandAllocator
{
public:
	DXCommandAllocator(ID3D12Device* a_Device, const D3D12_COMMAND_LIST_TYPE a_QueueType, const uint32_t a_CommandListCount);
	~DXCommandAllocator();

	DXCommandList* GetCommandList();
	void FreeCommandList(DXCommandList* a_CmdList);
	void ResetCommandAllocator();

private:
	ID3D12CommandAllocator* m_Allocator;
	D3D12_COMMAND_LIST_TYPE m_Type;
	Pool<DXCommandList> m_Lists;
	uint32_t m_ListSize;

	friend class DXCommandList; //The commandlist must be able to access the allocator for a reset.
};

struct Fence
{
	UINT64 fenceValue = 0;
	ID3D12Fence* fence{};
};

//Safely releases a DX type
void DXRelease(IUnknown* a_Obj);
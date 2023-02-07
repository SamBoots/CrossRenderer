#pragma once
#include "DX12Common.h"
#include "D3D12MemAlloc.h"

#ifdef _DEBUG
#define DXASSERT(a_HRESULT, a_Msg)\
	if (a_HRESULT != S_OK)\
		BB_ASSERT(false, a_Msg);\

#else
#define DXASSERT(a_HRESULT, a_Msg) a_HRESULT
#endif //_DEBUG

namespace BB
{

	static FreelistAllocator_t s_DX12Allocator{ mbSize * 2 };

	namespace DXConv
	{
		const D3D12_RESOURCE_STATES ResourceStates(const RENDER_BUFFER_USAGE a_Usage);
		const D3D12_HEAP_TYPE HeapType(const RENDER_MEMORY_PROPERTIES a_Properties);
		const D3D12_COMMAND_LIST_TYPE CommandListType(const RENDER_QUEUE_TYPE a_RenderQueueType);
	}

	//Safely releases a DX type
	void DXRelease(IUnknown* a_Obj);

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

	class DXResource
	{
	public:
		DXResource(D3D12MA::Allocator* a_ResourceAllocator, const RENDER_BUFFER_USAGE a_InitialState, const D3D12_HEAP_TYPE a_HeapType, const uint64_t a_Size);
		~DXResource();

		ID3D12Resource* GetResource() const { return m_Resource; };

	private:
		ID3D12Resource* m_Resource;
		D3D12MA::Allocation* m_Allocation;
		DX12BufferView m_View;

		D3D12_RESOURCE_STATES m_CurrentState;
		//Debug info
		D3D12_RESOURCE_STATES m_PreviousState;
	};

	class DXCommandQueue
	{
	public:
		DXCommandQueue(ID3D12Device* a_Device, const D3D12_COMMAND_LIST_TYPE a_CommandType);
		DXCommandQueue(ID3D12Device* a_Device, const D3D12_COMMAND_LIST_TYPE a_CommandType, ID3D12CommandQueue* a_CommandQueue);
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

	class DXCommandAllocator;

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

		//Puts the commandlist back into the command allocator, does not delete the ID3D12GraphicsCommandList.
		void Free();

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

	struct DescriptorHeapHandle
	{
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
		D3D12_GPU_DESCRIPTOR_HANDLE cpuHandle{};
		uint32_t heapIndex{};
		uint32_t count{};
	};

	class DescriptorHeap
	{
	public:
		DescriptorHeap(ID3D12Device* a_Device, D3D12_DESCRIPTOR_HEAP_TYPE a_HeapType, uint32_t a_DescriptorCount, bool a_ShaderVisible);
		~DescriptorHeap();

		DescriptorHeapHandle Allocate(const uint32_t a_Count);

		void Reset();

		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUStartPtr() const { return m_HeapCPUStart; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUStartPtr() const { return m_HeapGPUStart; }
		const UINT DescriptorsLeft() const { return m_MaxDescriptors - m_InUse; }
		ID3D12DescriptorHeap* GetHeap() const { return m_DescriptorHeap; }

	private:
		ID3D12DescriptorHeap* m_DescriptorHeap;
		D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
		D3D12_CPU_DESCRIPTOR_HANDLE m_HeapCPUStart;
		D3D12_GPU_DESCRIPTOR_HANDLE m_HeapGPUStart;
		uint32_t m_MaxDescriptors;
		uint32_t m_InUse;
		uint32_t m_IncrementSize;
	};
}
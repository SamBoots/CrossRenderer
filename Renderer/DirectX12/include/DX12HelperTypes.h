#pragma once
#include "DX12Common.h"
#include "D3D12MemAlloc.h"
#include "Allocators/RingAllocator.h"
#include "FixedArray.h"

#ifdef _DEBUG
#define DXASSERT(a_HRESULT, a_Msg)\
	if (a_HRESULT != S_OK)\
		BB_ASSERT(false, a_Msg);\

#else
#define DXASSERT(a_HRESULT, a_Msg) a_HRESULT
#endif //_DEBUG

namespace BB
{
	//Some globals
	constexpr DXGI_FORMAT DEPTH_FORMAT = DXGI_FORMAT_D32_FLOAT;
	constexpr uint64_t COMMAND_BUFFER_STANDARD_COUNT = 32;


	static FreelistAllocator_t s_DX12Allocator{ mbSize * 2 };
	static RingAllocator s_DX12TempAllocator{ s_DX12Allocator, kbSize * 64 };

	FixedArray<D3D12_INPUT_ELEMENT_DESC, 4> VertexInputElements();


	namespace DXConv
	{
		const D3D12_RESOURCE_STATES ResourceStates(const RENDER_BUFFER_USAGE a_Usage);
		const D3D12_RESOURCE_STATES ResourceStateImage(const RENDER_IMAGE_LAYOUT a_ImageLayout);
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

	//maybe make this a freelist, make sure to free it in DX12DestroyPipeline if I decide to add this.
	struct DescriptorHeapHandle
	{
		ID3D12DescriptorHeap* heap;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
		uint32_t heapIndex{};
		uint32_t count{};
		uint32_t incrementSize{};
	};
	
	class DescriptorHeap
	{
	public:
		DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE a_HeapType, uint32_t a_DescriptorCount, bool a_ShaderVisible);
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
		uint32_t m_MaxDescriptors = 0;
		uint32_t m_InUse = 0;
		uint32_t m_IncrementSize = 0;
	};

	class DXFence
	{
	public:
		DXFence();
		~DXFence();

		uint64_t PollFenceValue();
		bool IsFenceComplete(const uint64_t a_FenceValue);

		void WaitFenceCPU(const uint64_t a_FenceValue);
		void WaitIdle() { WaitFenceCPU(m_NextFenceValue - 1); }

		ID3D12Fence* GetFence() const { return m_Fence; }
		uint64_t GetNextFenceValue() const { return m_NextFenceValue; }

	private:
		ID3D12Fence* m_Fence;
		uint64_t m_NextFenceValue;
		uint64_t m_LastCompleteValue;
		HANDLE m_FenceEvent;

		friend class DXCommandQueue; //Commandqueue handles the Fence in a special way.
	};

	class DXResource
	{
	public:
		DXResource(const RENDER_BUFFER_USAGE a_BufferUsage, const RENDER_MEMORY_PROPERTIES a_MemProperties, const uint64_t a_Size);
		~DXResource();

		ID3D12Resource* GetResource() const { return m_Resource; };
		UINT GetResourceSize() const { return m_Size; };

	private:
		ID3D12Resource* m_Resource;
		D3D12MA::Allocation* m_Allocation;
		const UINT m_Size = 0;
	};

	class DXImage
	{
	public:
		struct DepthMetaData
		{
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
		};

		DXImage(const RenderImageCreateInfo& a_Info);
		~DXImage();


		ID3D12Resource* GetResource() const { return m_Resource; };
		//Optionally we can query the descriptor index if it has one. 
		//DEPTH STENCIL ONLY
		DXImage::DepthMetaData GetDepthMetaData() const { return m_DepthData; };

	private:
		ID3D12Resource* m_Resource;
		D3D12MA::Allocation* m_Allocation;
		//Some extra metadata.
		union
		{
			DepthMetaData m_DepthData;
		};
		
	};

	class DXCommandQueue
	{
	public:
		DXCommandQueue(const D3D12_COMMAND_LIST_TYPE a_CommandType);
		DXCommandQueue(const D3D12_COMMAND_LIST_TYPE a_CommandType, ID3D12CommandQueue* a_CommandQueue);
		~DXCommandQueue();

		uint64_t PollFenceValue()
		{
			return m_Fence.PollFenceValue();
		}
		bool IsFenceComplete(const uint64_t a_FenceValue)
		{
			IsFenceComplete(a_FenceValue);
		}

		void WaitFenceCPU(const uint64_t a_FenceValue)
		{
			m_Fence.WaitFenceCPU(a_FenceValue);
		};
		void WaitIdle() 
		{ 
			m_Fence.WaitIdle(); 
		}

		void InsertWait(const uint64_t a_FenceValue);
		void InsertWaitQueue(const DXCommandQueue& a_WaitQueue);
		void InsertWaitQueueFence(const DXCommandQueue& a_WaitQueue, const uint64_t a_FenceValue);

		void ExecuteCommandlist(ID3D12CommandList** a_CommandLists, const uint32_t a_CommandListCount);
		void SignalQueue();
		//Signal another fence with a queue.
		void SignalQueue(DXFence& a_Fence);

		ID3D12CommandQueue* GetQueue() const { return m_Queue; }
		ID3D12Fence* GetFence() const { return m_Fence.m_Fence; }
		uint64_t GetNextFenceValue() const { return m_Fence.m_NextFenceValue; }

	private:
		ID3D12CommandQueue* m_Queue;
		D3D12_COMMAND_LIST_TYPE m_QueueType;
		DXFence m_Fence;

		friend class DXCommandAllocator; //Allocator should have access to the QueueType.
	};

	class DXCommandAllocator;
	struct DXPipeline;

	class DXCommandList
	{
	public:
		DXCommandList(DXCommandAllocator& a_CmdAllocator);
		~DXCommandList();

		//Possible caching for efficiency, might go for specific commandlist types.
		ID3D12Resource* rtv;
		DXPipeline* boundPipeline;

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
		DXCommandAllocator(const D3D12_COMMAND_LIST_TYPE a_QueueType, const uint32_t a_CommandListCount);
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

	struct RootConstant
	{
		uint32_t dwordCount;
		UINT rootIndex{};
	};

	struct RootDescriptor 
	{
		D3D12_GPU_VIRTUAL_ADDRESS virtAddress{};
		UINT rootIndex{};
	};

	struct DescTable
	{
		DescriptorHeapHandle table{};
		UINT rootIndex{};
	};

	//This somewhat represents a vkDescriptorSet.
	struct DXDescriptor
	{
		//Maximum of 4 bindings.
		RENDER_BINDING_SET shaderSpace = {};

		DescTable tables;
		uint32_t tableDescRangeCount = 0;
		D3D12_DESCRIPTOR_RANGE1* tableDescRanges = nullptr;

		uint32_t rootConstantCount = 0;
		RootConstant rootConstant[4];

		uint32_t cbvCount = 0;
		RootDescriptor rootCBV[4];
		uint32_t srvCount = 0;
		RootDescriptor rootSRV[4];
	};

	//Maybe create a class and a builder for this?
	struct DXPipeline
	{
		//Optmize Rootsignature and pipelinestate to cache them somewhere and reuse them.
		ID3D12PipelineState* pipelineState;
		ID3D12RootSignature* rootSig{};

		//Each index indicates the start paramindex for a binding.
		UINT rootParamBindingOffset[BINDING_MAX]{};
	};

	struct DX12Backend_inst
	{
		FrameIndex currentFrame = 0;
		UINT backBufferCount = 3; //for now hardcode 3 backbuffers.
		DXFence* frameFences; //Equal amount of fences to backBufferCount.

		IDXGIFactory4* factory{};
		ID3D12Debug1* debugController{};

		DescriptorHeap* CBV_SRV_UAVHeap;
		DescriptorHeap* samplerHeap;
		DescriptorHeap* dsvHeap;

		IDXGIAdapter1* adapter;
		ID3D12Device* device;

		ID3D12DebugDevice1* debugDevice;

		UINT swapWidth;
		UINT swapHeight;
		IDXGISwapChain3* swapchain;
		ID3D12Resource** swapchainRenderTargets; //dyn alloc
		ID3D12DescriptorHeap* swapchainRTVHeap;

		D3D12MA::Allocator* DXMA;
		ID3D12CommandQueue* directpresentqueue;

		Pool<DXDescriptor> bindingSetPool;
		Pool<DXPipeline> pipelinePool;
		Pool<DXCommandQueue> cmdQueues;
		Pool<DXCommandAllocator> cmdAllocators;
		Pool<DXResource> renderResources;
		Pool<DXImage> renderImages;
		Pool<DXFence> fencePool;

		void CreatePools()
		{
			pipelinePool.CreatePool(s_DX12Allocator, 4);
			bindingSetPool.CreatePool(s_DX12Allocator, 16);
			cmdQueues.CreatePool(s_DX12Allocator, 4);
			cmdAllocators.CreatePool(s_DX12Allocator, 16);
			renderResources.CreatePool(s_DX12Allocator, 8);
			renderImages.CreatePool(s_DX12Allocator, 8);
			fencePool.CreatePool(s_DX12Allocator, 16);
		}

		void DestroyPools()
		{
			pipelinePool.DestroyPool(s_DX12Allocator);
			bindingSetPool.DestroyPool(s_DX12Allocator);
			cmdQueues.DestroyPool(s_DX12Allocator);
			cmdAllocators.DestroyPool(s_DX12Allocator);
			renderResources.DestroyPool(s_DX12Allocator);
			renderImages.DestroyPool(s_DX12Allocator);
			fencePool.DestroyPool(s_DX12Allocator);
		}
	};

	extern DX12Backend_inst s_DX12B;
}
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
	constexpr UINT INVALID_ROOT_INDEX = UINT_MAX;


	static FreelistAllocator_t s_DX12Allocator{ mbSize * 2 };
	static RingAllocator s_DX12TempAllocator{ s_DX12Allocator, kbSize * 64 };

	namespace DXConv
	{
		const D3D12_SHADER_VISIBILITY ShaderVisibility(const RENDER_SHADER_STAGE a_Stage);
		const D3D12_RESOURCE_STATES ResourceStates(const RENDER_BUFFER_USAGE a_Usage);
		const D3D12_RESOURCE_STATES ResourceStateImage(const RENDER_IMAGE_LAYOUT a_ImageLayout);
		const D3D12_HEAP_TYPE HeapType(const RENDER_MEMORY_PROPERTIES a_Properties);
		const D3D12_COMMAND_LIST_TYPE CommandListType(const RENDER_QUEUE_TYPE a_RenderQueueType);
		const D3D12_BLEND Blend(const RENDER_BLEND_FACTOR a_BlendFactor);
		const D3D12_BLEND_OP BlendOp(const RENDER_BLEND_OP a_BlendOp);
		const D3D12_LOGIC_OP LogicOp(const RENDER_LOGIC_OP a_LogicOp);
		static inline D3D12_TEXTURE_ADDRESS_MODE AddressMode(const SAMPLER_ADDRESS_MODE a_Mode)
		{
			switch (a_Mode)
			{
			case SAMPLER_ADDRESS_MODE::REPEAT:		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			case SAMPLER_ADDRESS_MODE::MIRROR:		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			case SAMPLER_ADDRESS_MODE::BORDER:		return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			case SAMPLER_ADDRESS_MODE::CLAMP:		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			}
		}
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
			DepthMetaData m_DepthData{};
		};
		
	};

	class DXSampler
	{
	public:
		DXSampler(const SamplerCreateInfo& a_Info);
		~DXSampler();

		void UpdateSamplerInfo(const SamplerCreateInfo& a_Info);
		const D3D12_SAMPLER_DESC* GetDesc() const { return &m_Desc; };

	private:
		D3D12_SAMPLER_DESC m_Desc{};
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
		uint32_t dwordCount = 0;
		UINT rootIndex = INVALID_ROOT_INDEX;
	};

	struct RootDescriptor 
	{
		D3D12_GPU_VIRTUAL_ADDRESS virtAddress{};
		UINT rootIndex = INVALID_ROOT_INDEX;
	};

	struct DescTable
	{
		DescriptorHeapHandle table{};
		UINT rootIndex = INVALID_ROOT_INDEX;
	};

	enum class DESC_ATTACHMENT_TYPE : uint32_t
	{
		CONSTANT,
		ROOT_CBV,
		ROOT_SRV,
		ROOT_UAV,
		TABLE,
		TABLE_SAMPLER
	};

	struct TableContent //8 bytes
	{
		D3D12_DESCRIPTOR_RANGE_TYPE rangeType{};
		uint32_t descriptorCount = UINT32_MAX;
		uint32_t tableIndex = 0;
	};
	struct RootContent //8 bytes
	{
		D3D12_DESCRIPTOR_RANGE_TYPE rangeType{};
		D3D12_GPU_VIRTUAL_ADDRESS virtAddress = 0;
		uint32_t rootIndex = UINT32_MAX;
	};

	struct DescriptorAttachment
	{
		DESC_ATTACHMENT_TYPE attachType;
		union
		{
			TableContent tableContent{};
			RootContent rootContent;
		};
	};

	//This somewhat represents a vkDescriptorSet.
	struct DXDescriptor
	{
		//Maximum of 4 bindings.
		RENDER_BINDING_SET shaderSpace = {};

		uint32_t tableParamCount = 0;
		DescTable tables;

		uint32_t samplerCount = 0;
		DescTable samplerTable;

		uint32_t descriptorAttachmentCount = 0;
		DescriptorAttachment* descriptorAttachments{};

		uint32_t dynamicBufferCount = 0;
		RootContent** dynamicBuffers{};
	};

	//Maybe create a class and a builder for this?
	struct DXPipeline
	{
		//Optmize Rootsignature and pipelinestate to cache them somewhere and reuse them.
		ID3D12PipelineState* pipelineState{};
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
		Pool<DXSampler> samplerPool;

		void CreatePools()
		{
			pipelinePool.CreatePool(s_DX12Allocator, 4);
			bindingSetPool.CreatePool(s_DX12Allocator, 16);
			cmdQueues.CreatePool(s_DX12Allocator, 4);
			cmdAllocators.CreatePool(s_DX12Allocator, 16);
			renderResources.CreatePool(s_DX12Allocator, 16);
			renderImages.CreatePool(s_DX12Allocator, 8);
			fencePool.CreatePool(s_DX12Allocator, 16);
			samplerPool.CreatePool(s_DX12Allocator, 16);
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
			samplerPool.DestroyPool(s_DX12Allocator);
		}
	};

	extern DX12Backend_inst s_DX12B;
}
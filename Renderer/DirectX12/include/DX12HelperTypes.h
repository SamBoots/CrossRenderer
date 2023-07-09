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
		inline D3D12_DESCRIPTOR_RANGE_TYPE DescriptorRangeType(const RENDER_DESCRIPTOR_TYPE a_Type)
		{
			switch (a_Type)
			{
			case BB::RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:	return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			case BB::RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:	return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			case BB::RENDER_DESCRIPTOR_TYPE::READWRITE:			return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			case BB::RENDER_DESCRIPTOR_TYPE::IMAGE:				return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			case BB::RENDER_DESCRIPTOR_TYPE::SAMPLER:			return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			default:
				BB_ASSERT(false, "DX12: RENDER_DESCRIPTOR_TYPE failed to convert to a D3D12_DESCRIPTOR_RANGE_TYPE.");
				return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				break;
			}
		}

		static inline D3D12_BARRIER_LAYOUT BarrierLayout(const RENDER_IMAGE_LAYOUT a_ImageLayout)
		{
			switch (a_ImageLayout)
			{
			case RENDER_IMAGE_LAYOUT::UNDEFINED:				return D3D12_BARRIER_LAYOUT_UNDEFINED;
			case RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL: return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
			case RENDER_IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT: return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
			case RENDER_IMAGE_LAYOUT::GENERAL:					return D3D12_BARRIER_LAYOUT_COMMON;
			case RENDER_IMAGE_LAYOUT::TRANSFER_SRC:				return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
			case RENDER_IMAGE_LAYOUT::TRANSFER_DST:				return D3D12_BARRIER_LAYOUT_COPY_DEST;
			case RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY:			return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
			case RENDER_IMAGE_LAYOUT::PRESENT:					return D3D12_BARRIER_LAYOUT_PRESENT;
			default:
				BB_ASSERT(false, "DX12: RENDER_IMAGE_LAYOUT failed to convert to a D3D12_BARRIER_LAYOUT.");
				return D3D12_BARRIER_LAYOUT_UNDEFINED;
				break;
			}
		}

		static inline D3D12_BARRIER_ACCESS BarrierAccess(const RENDER_ACCESS_MASK a_Type)
		{
			switch (a_Type)
			{
			case RENDER_ACCESS_MASK::NONE:						return D3D12_BARRIER_ACCESS_NO_ACCESS;
			case RENDER_ACCESS_MASK::TRANSFER_WRITE:			return D3D12_BARRIER_ACCESS_COPY_DEST;
			case RENDER_ACCESS_MASK::DEPTH_STENCIL_READ_WRITE:	return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ | D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
			case RENDER_ACCESS_MASK::SHADER_READ:				return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
			default:
				BB_ASSERT(false, "DX12: RENDER_ACCESS_MASK failed to convert to a D3D12_BARRIER_ACCESS.");
				return D3D12_BARRIER_ACCESS_NO_ACCESS;
				break;
			}
		}

		static inline D3D12_BARRIER_SYNC BarrierSync(const RENDER_PIPELINE_STAGE a_Stage)
		{
			switch (a_Stage)
			{
			case RENDER_PIPELINE_STAGE::TOP_OF_PIPELINE:		return D3D12_BARRIER_SYNC_ALL;
			case RENDER_PIPELINE_STAGE::TRANSFER:				return D3D12_BARRIER_SYNC_COPY;
			case RENDER_PIPELINE_STAGE::VERTEX_INPUT:			return D3D12_BARRIER_SYNC_VERTEX_SHADING;
			case RENDER_PIPELINE_STAGE::VERTEX_SHADER:			return D3D12_BARRIER_SYNC_VERTEX_SHADING;
			case RENDER_PIPELINE_STAGE::EARLY_FRAG_TEST:		return D3D12_BARRIER_SYNC_DEPTH_STENCIL;
			case RENDER_PIPELINE_STAGE::FRAGMENT_SHADER:		return D3D12_BARRIER_SYNC_PIXEL_SHADING;
			case RENDER_PIPELINE_STAGE::END_OF_PIPELINE:		return D3D12_BARRIER_SYNC_ALL;
			default:
				BB_ASSERT(false, "DX12: RENDER_PIPELINE_STAGE failed to convert to a D3D12_BARRIER_SYNC.");
				return D3D12_BARRIER_SYNC_ALL;
				break;
			}
		}

		const D3D12_SHADER_VISIBILITY ShaderVisibility(const RENDER_SHADER_STAGE a_Stage);

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
		inline const D3D12_RESOURCE_STATES ResourceStates(const RENDER_IMAGE_LAYOUT a_Layout)
		{
			switch (a_Layout)
			{
			case RENDER_IMAGE_LAYOUT::UNDEFINED:				return D3D12_RESOURCE_STATE_COMMON;
			case RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL: return D3D12_RESOURCE_STATE_RENDER_TARGET;
			case RENDER_IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
			case RENDER_IMAGE_LAYOUT::GENERAL:					return D3D12_RESOURCE_STATE_COMMON;
			case RENDER_IMAGE_LAYOUT::TRANSFER_SRC:				return D3D12_RESOURCE_STATE_COPY_SOURCE;
			case RENDER_IMAGE_LAYOUT::TRANSFER_DST:				return D3D12_RESOURCE_STATE_COPY_DEST;
			case RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY:			return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			case RENDER_IMAGE_LAYOUT::PRESENT:					return D3D12_RESOURCE_STATE_PRESENT;
			default:
				BB_ASSERT(false, "DX12: RENDER_IMAGE_LAYOUT failed to convert to a D3D12_RESOURCE_STATES.");
				return D3D12_RESOURCE_STATE_COMMON;
				break;
			}
		}
	}

	static inline wchar* UTF8ToUnicodeString(Allocator a_Allocator, const char* a_Char)
	{
		//arbitrary limit of 256.
		const size_t t_CharSize = strnlen_s(a_Char, 256);
		wchar* t_Wchar = reinterpret_cast<wchar*>(BBalloc(a_Allocator, t_CharSize * 2 + 2)); //add null terminated string.
		mbstowcs(t_Wchar, a_Char, t_CharSize);
		t_Wchar[t_CharSize] = NULL;
		return t_Wchar;
	}

	//Safely releases a DX type
	inline void DXRelease(IUnknown* a_Obj)
	{
		if (a_Obj)
			a_Obj->Release();
	}

	union DX12BufferView
	{
		D3D12_VERTEX_BUFFER_VIEW vertexView;
		D3D12_INDEX_BUFFER_VIEW indexView;
		D3D12_CONSTANT_BUFFER_VIEW_DESC constantView;
	};
	
	class DXDescriptorHeap
	{
	public:
		DXDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE a_HeapType, uint32_t a_DescriptorCount, bool a_ShaderVisible, const char* a_Name);
		~DXDescriptorHeap();

		DescriptorAllocation Allocate(const RDescriptor a_Layout, const uint32_t a_HeapOffset);

		void Reset()
		{
			m_InUse = 0;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUStartPtr() const { return m_HeapCPUStart; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUStartPtr() const { return m_HeapGPUStart; }
		const UINT DescriptorsLeft() const { return m_MaxDescriptors - m_InUse; }
		ID3D12DescriptorHeap* GetHeap() const { return m_DescriptorHeap; }
		const D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() const { return m_HeapType; }
		const UINT GetIncrementSize() const { return m_IncrementSize; }

	private:
		ID3D12DescriptorHeap* m_DescriptorHeap;
		const D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
		D3D12_CPU_DESCRIPTOR_HANDLE m_HeapCPUStart;
		D3D12_GPU_DESCRIPTOR_HANDLE m_HeapGPUStart;
		const UINT m_MaxDescriptors = 0;
		UINT m_InUse = 0;
		const UINT m_IncrementSize = 0;
	};

	class DXFence
	{
	public:
		DXFence(const char* a_Name);
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

		//Commandqueue handles the Fence in a special way.
		friend class DXCommandQueue; 
	};

	class DXResource
	{
	public:
		DXResource(const RenderBufferCreateInfo& a_CreateInfo);
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

		struct TextureData
		{
			DXGI_FORMAT format;
		};

		DXImage(const RenderImageCreateInfo& a_Info);
		~DXImage();


		ID3D12Resource* GetResource() const { return m_Resource; };
		//Optionally we can query the descriptor index if it has one. 
		//DEPTH STENCIL ONLY
		DXImage::DepthMetaData GetDepthMetaData() const { return m_DepthData; };
		DXImage::TextureData GetTextureData() const { return m_TextureData; }
	private:
		ID3D12Resource* m_Resource;
		D3D12MA::Allocation* m_Allocation;
		//Some extra metadata.
		union
		{
			TextureData m_TextureData{};
			DepthMetaData m_DepthData;
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
		DXCommandQueue(const RenderCommandQueueCreateInfo& a_CreateInfo);
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

		ID3D12GraphicsCommandList7* List() const { return m_List; }
		//Commandlist holds the allocator info, so use this instead of List()->Reset
		void Reset(ID3D12PipelineState* a_PipeState = nullptr);
		//Prefer to use this Close instead of List()->Close() for error testing purposes
		void Close();

		//Puts the commandlist back into the command allocator, does not delete the ID3D12GraphicsCommandList.
		void Free();

		DXDescriptorHeap* heaps[2]{};

	private:
		union
		{
			ID3D12GraphicsCommandList7* m_List;
		};
		DXCommandAllocator& m_CmdAllocator;
	};

	class DXCommandAllocator
	{
	public:
		DXCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
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

	//Maybe create a class and a builder for this?
	struct DXPipeline
	{
		~DXPipeline()
		{
			memset(this, 0, sizeof(*this));
		}
		//Optmize Rootsignature and pipelinestate to cache them somewhere and reuse them.
		ID3D12PipelineState* pipelineState{};
		ID3D12RootSignature* rootSig{};

		//Each index indicates the start paramindex for a binding.
		UINT rootParamBindingOffset[BINDING_MAX]{};
	};

	struct DXDescriptor
	{
		~DXDescriptor()
		{
			memset(this, 0, sizeof(*this));
		}
		D3D12_DESCRIPTOR_RANGE1* tableEntries = nullptr;
		uint32_t tableEntryCount = 0;
		uint32_t descriptorCount = 0;
	};

	//lazy to make it work for depth.
	class DepthDescriptorHeap
	{
	public:
		~DepthDescriptorHeap()
		{
			DXRelease(heap);
			memset(this, 0, sizeof(*this));
		}
		D3D12_CPU_DESCRIPTOR_HANDLE Allocate();

		ID3D12DescriptorHeap* heap = nullptr;
		UINT pos = 0;
		UINT max = 0;
	};

	struct DX12Backend_inst
	{
		FrameIndex currentFrame = 0;
		UINT backBufferCount = 3; //for now hardcode 3 backbuffers.
		DXFence* frameFences; //Equal amount of fences to backBufferCount.

		UINT heap_cbv_srv_uav_increment_size;
		UINT heap_sampler_size;

		IDXGIFactory4* factory{};
		ID3D12Debug1* debugController{};

		DepthDescriptorHeap dsvHeap;

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

		Pool<DXDescriptor> descriptorPool;
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
			descriptorPool.CreatePool(s_DX12Allocator, 16);
			cmdQueues.CreatePool(s_DX12Allocator, 4);
			cmdAllocators.CreatePool(s_DX12Allocator, 16);
			renderResources.CreatePool(s_DX12Allocator, 32);
			renderImages.CreatePool(s_DX12Allocator, 8);
			fencePool.CreatePool(s_DX12Allocator, 16);
			samplerPool.CreatePool(s_DX12Allocator, 16);
		}

		void DestroyPools()
		{
			pipelinePool.DestroyPool(s_DX12Allocator);
			descriptorPool.DestroyPool(s_DX12Allocator);
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
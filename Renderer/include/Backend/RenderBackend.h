#pragma once
#include "TemporaryAllocator.h"
#include "RenderBackendCommon.h"

namespace BB
{
	class PipelineBuilder
	{
	public:
		PipelineBuilder(const PipelineInitInfo& a_InitInfo);
		~PipelineBuilder();
		
		void BindDescriptor(const RDescriptor a_Handle);
		void BindShaders(const Slice<BB::ShaderCreateInfo> a_ShaderInfo);
		void BindAttributes(const PipelineAttributes& a_AttributeInfo);
		PipelineHandle BuildPipeline();

	private:
		const char* m_Name = nullptr;
#ifdef _DEBUG
		PipelineDebugInfo m_DebugInfo{};
#endif _DEBUG
		PipelineBuilderHandle m_BuilderHandle;
	};

	struct UploadBufferChunk
	{
		void* memory;
		uint32_t offset;
		uint32_t size;
	};

	class UploadBuffer
	{
	public:
		UploadBuffer(const size_t a_Size, const char* a_Name = nullptr);
		~UploadBuffer();

		const UploadBufferChunk Alloc(const size_t a_Size);
		void Clear();

		inline const uint32_t GetCurrentOffset() const { return m_Offset; }
		inline const RBufferHandle Buffer() const { return m_Buffer; }
		inline void* GetStart() const { return m_Start; }

	private:
		RBufferHandle m_Buffer;
		const uint32_t m_Size;
		uint32_t m_Offset;
		void* m_Start;
	};

	class LinearRenderBuffer
	{
	public:
		LinearRenderBuffer(const RenderBufferCreateInfo& a_CreateInfo);
		~LinearRenderBuffer();

		RenderBufferPart SubAllocate(const uint64_t a_Size, const uint32_t a_Alignment = sizeof(size_t));

		void MapBuffer() const;
		void UnmapBuffer() const;

		inline const RBufferHandle GetBuffer() const { return m_Buffer; }
		inline const uint64_t GetMaxSize() const { return m_Size; }
		inline const uint64_t GetRemainingSize() const { return m_Size - m_Used; }

	private:
		const RENDER_MEMORY_PROPERTIES m_MemoryProperties;
		const RBufferHandle m_Buffer;
		const uint64_t m_Size;
		uint64_t m_Used;
	};

	//Linear allocator type descriptor manager. Writes to a CPU descriptor heap and can copy to local GPU visible descriptors.
	class DescriptorManager
	{
	public:
		DescriptorManager(Allocator a_SystemAllocator, const DescriptorHeapCreateInfo& a_CreateInfo, const uint32_t a_BackbufferCount);
		~DescriptorManager();

		DescriptorAllocation Allocate(const RDescriptor a_Descriptor);
		void UploadToGPUHeap(const uint32_t a_FrameNum) const;

		const uint32_t GetCPUOffsetFlag() const;
		void SetCPUOffsetFlag(const uint32_t a_Offset);

		//prefer to use SetCPUOffsetFlag so that you do not need to keep setting descriptors.
		void ClearCPUHeap();

		const RDescriptorHeap GetGPUHeap(const uint32_t a_FrameNum) const;
		const uint32_t GetHeapOffset() const;
		const uint32_t GetHeapSize() const;

	private:
		struct DescriptorManager_inst* m_Inst;
	};

	struct RenderFence
	{
		RFenceHandle fence{};
		uint64_t nextFenceValue = 0;
		uint64_t lastCompleteValue = 0;
	};

	struct CommandList
	{
		CommandAllocatorHandle cmdAllocator;
		CommandListHandle list;
		uint64_t queueFenceValue;
		CommandList* next = nullptr;
		//debug
		RENDER_QUEUE_TYPE type;
		const CommandListHandle operator -> () { return list; }
		operator const CommandListHandle () { return list; }
	};

	namespace RenderBackend
	{
		void DisplayDebugInfo();

		const uint32_t GetFrameBufferAmount();
		const FrameIndex GetCurrentFrameBufferIndex();

		void InitBackend(const RenderBackendCreateInfo& a_CreateInfo, Allocator a_SystemAllocator);
		RDescriptor CreateDescriptor(const RenderDescriptorCreateInfo& a_CreateInfo);
		CommandQueueHandle CreateCommandQueue(const RenderCommandQueueCreateInfo& a_CreateInfo);
		CommandAllocatorHandle CreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
		CommandListHandle CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo);
		RBufferHandle CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo);
		RImageHandle CreateImage(const RenderImageCreateInfo& a_CreateInfo);
		RSamplerHandle CreateSampler(const SamplerCreateInfo& a_CreateInfo);
		RFenceHandle CreateFence(const FenceCreateInfo& a_CreateInfo);

		void SetResourceName(const SetResourceNameInfo& a_Info);

		void WriteDescriptors(const WriteDescriptorInfos& a_WriteInfo);
		void CopyDescriptors(const CopyDescriptorsInfo& a_CopyInfo);
		ImageReturnInfo GetImageInfo(const RImageHandle a_Handle);

		void ResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle);

		void StartCommandList(const CommandListHandle a_CmdHandle);
		void EndCommandList(const CommandListHandle a_RecordingCmdHandle);
		void StartRendering(const CommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_StartInfo);
		void SetScissor(const CommandListHandle a_RecordingCmdHandle, const ScissorInfo& a_ScissorInfo);
		void EndRendering(const CommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo);
		
		void CopyBuffer(const CommandListHandle a_RecordingCmdHandle, const RenderCopyBufferInfo& a_CopyInfo);
		void CopyBufferImage(const CommandListHandle a_RecordingCmdHandle, const RenderCopyBufferImageInfo& a_CopyInfo);
		void SetPipelineBarriers(const CommandListHandle a_RecordingCmdHandle, const PipelineBarrierInfo& a_BarrierInfo);

		void BindDescriptorHeaps(const CommandListHandle a_RecordingCmdHandle, const RDescriptorHeap a_ResourceHeap, const RDescriptorHeap a_SamplerHeap);
		void BindPipeline(const CommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
		void SetDescriptorHeapOffsets(const CommandListHandle a_RecordingCmdHandle, const RENDER_DESCRIPTOR_SET a_FirstSet, const uint32_t a_SetCount, const uint32_t* a_HeapIndex, const size_t* a_Offsets);
		void BindVertexBuffers(const CommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
		void BindIndexBuffer(const CommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
		void BindConstant(const CommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_DwordOffset, const void* a_Data);

		void DrawVertex(const CommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
		void DrawIndexed(const CommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);

		void BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);
		void* MapMemory(const RBufferHandle a_Handle);
		void UnmapMemory(const RBufferHandle a_Handle);

		void StartFrame(const StartFrameInfo& a_StartInfo);
		void ExecuteCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount);
		void ExecutePresentCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo);
		FrameIndex PresentFrame(const PresentFrameInfo& a_PresentInfo);

		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);

		void WaitCommands(const RenderWaitCommandsInfo& a_WaitInfo);

		void DestroyBackend();
		void DestroyDescriptor(const RDescriptor a_Handle);
		void DestroyPipeline(const PipelineHandle a_Handle);
		void DestroyCommandQueue(const CommandQueueHandle a_Handle);
		void DestroyCommandAllocator(const CommandAllocatorHandle a_Handle);
		void DestroyCommandList(const CommandListHandle a_Handle);
		void DestroyBuffer(const RBufferHandle a_Handle);
		void DestroyImage(const RImageHandle a_Handle);
		void DestroySampler(const RSamplerHandle a_Handle);
		void DestroyFence(const RFenceHandle a_Handle);
	};
}
#include "RenderBackend.h"
#include "RenderResourceTracker.h"

#include "Editor.h"
#include "Utils/Slice.h"
#include "Slotmap.h"
#include "BBString.h"
#include "Program.h"
#include <malloc.h>

using namespace BB;

static RenderAPIFunctions s_ApiFunc;

static BackendInfo s_BackendInfo;

static RenderResourceTracker s_ResourceTracker;

PipelineBuilder::PipelineBuilder(const PipelineInitInfo& a_InitInfo)
{
	BB_ASSERT(a_InitInfo.renderTargetBlendCount > 0, "No blending targets given!")
	BB_ASSERT(a_InitInfo.renderTargetBlendCount < 8, "More then 8 blending targets! This will not work with directx12.")
	m_BuilderHandle = s_ApiFunc.pipelineBuilderInit(a_InitInfo);
#ifdef _DEBUG
	//set some debug info.
	m_Name = a_InitInfo.name;
	m_DebugInfo.enableDepthTest = a_InitInfo.enableDepthTest;
	m_DebugInfo.constantData = a_InitInfo.constantData;
	m_DebugInfo.rasterState = a_InitInfo.rasterizerState;
	m_DebugInfo.renderTargetBlendCount = a_InitInfo.renderTargetBlendCount;
	for (size_t i = 0; i < m_DebugInfo.renderTargetBlendCount; i++)
	{
		m_DebugInfo.renderTargetBlends[i] = a_InitInfo.renderTargetBlends[i];
	}

	m_DebugInfo.immutableSamplerCount = static_cast<uint32_t>(a_InitInfo.immutableSamplers.size());
	m_DebugInfo.immutableSamplers = (SamplerCreateInfo*)(_malloca(a_InitInfo.immutableSamplers.sizeInBytes()));
	for (size_t i = 0; i < m_DebugInfo.immutableSamplerCount; i++)
	{
		m_DebugInfo.immutableSamplers[i] = a_InitInfo.immutableSamplers[i];
	}
#endif //_DEBUG
}

PipelineBuilder::~PipelineBuilder()
{
	BB_ASSERT(m_BuilderHandle.handle == 0, "Unfinished pipeline destructed! Big memory leak and improper graphics API usage.");
#ifdef _DEBUG
	if (m_DebugInfo.shaderInfo)
	{
		_freea(m_DebugInfo.shaderInfo);
	}
	if (m_DebugInfo.attributes)
	{
		_freea(m_DebugInfo.attributes);
	}
	if (m_DebugInfo.immutableSamplerCount)
	{
		_freea(m_DebugInfo.immutableSamplers);
	}
#endif //_DEBUG
	memset(this, 0, sizeof(*this));
}

void PipelineBuilder::BindDescriptor(const RDescriptor a_Handle)
{
	s_ApiFunc.pipelineBuilderBindDescriptor(m_BuilderHandle, a_Handle);
}

void PipelineBuilder::BindShaders(const Slice<BB::ShaderCreateInfo> a_ShaderInfo)
{
	s_ApiFunc.pipelineBuilderBindShaders(m_BuilderHandle, a_ShaderInfo);
#ifdef _DEBUG
	m_DebugInfo.shaderCount = static_cast<uint32_t>(a_ShaderInfo.size());
	m_DebugInfo.shaderInfo = (PipelineDebugInfo::ShaderInfo*)(_malloca(a_ShaderInfo.sizeInBytes()));
	for (size_t i = 0; i < m_DebugInfo.shaderCount; i++)
	{
		m_DebugInfo.shaderInfo[i].optionalShaderpath = a_ShaderInfo[i].optionalShaderpath;
		m_DebugInfo.shaderInfo[i].shaderStage = a_ShaderInfo[i].shaderStage;
	}
#endif //_DEBUG
}

void PipelineBuilder::BindAttributes(const PipelineAttributes& a_AttributeInfo)
{
	s_ApiFunc.pipelineBuilderBindAttributes(m_BuilderHandle, a_AttributeInfo);
#ifdef _DEBUG
	m_DebugInfo.attributeCount = static_cast<uint32_t>(a_AttributeInfo.attributes.size());
	m_DebugInfo.attributes = (VertexAttributeDesc*)(_malloca(a_AttributeInfo.attributes.sizeInBytes()));
	for (size_t i = 0; i < m_DebugInfo.attributeCount; i++)
	{
		m_DebugInfo.attributes[i] = a_AttributeInfo.attributes[i];
	}
#endif //_DEBUG
}

PipelineHandle PipelineBuilder::BuildPipeline()
{
	//Buildpipeline will also destroy the builder information. 
	const PipelineHandle t_ReturnHandle = s_ApiFunc.pipelineBuilderBuildPipeline(m_BuilderHandle);
	m_BuilderHandle.handle = 0; //Set the handle to 0 to indicate we can safely destruct the class.

	//send to the editor.
#ifdef _DEBUG
	s_ResourceTracker.AddPipeline(m_DebugInfo, m_Name, t_ReturnHandle.handle);

	if (m_DebugInfo.shaderInfo)
	{
		_freea(m_DebugInfo.shaderInfo);
		m_DebugInfo.shaderInfo = nullptr;
	}
	if (m_DebugInfo.attributes)
	{
		_freea(m_DebugInfo.attributes);
		m_DebugInfo.attributes = nullptr;
	}
	if (m_DebugInfo.immutableSamplerCount)
	{
		_freea(m_DebugInfo.immutableSamplers);
		m_DebugInfo.immutableSamplers = nullptr;
	}
#endif //_DEBUG

	return t_ReturnHandle;
}

UploadBuffer::UploadBuffer(const size_t a_Size, const char* a_Name)
	: m_Size(static_cast<uint32_t>(a_Size))
{
	BB_ASSERT(a_Size < UINT32_MAX, "upload buffer size is larger then UINT32_MAX, this is not supported");
	RenderBufferCreateInfo t_UploadBufferInfo{};
	t_UploadBufferInfo.name = a_Name;
	t_UploadBufferInfo.size = m_Size;
	t_UploadBufferInfo.usage = RENDER_BUFFER_USAGE::STAGING;
	t_UploadBufferInfo.memProperties = RENDER_MEMORY_PROPERTIES::HOST_VISIBLE;
	m_Buffer = RenderBackend::CreateBuffer(t_UploadBufferInfo);

	m_Offset = 0;
	m_Start = RenderBackend::MapMemory(m_Buffer);
}

UploadBuffer::~UploadBuffer()
{
	RenderBackend::UnmapMemory(m_Buffer);
	RenderBackend::DestroyBuffer(m_Buffer);
}

const UploadBufferChunk UploadBuffer::Alloc(const size_t a_Size)
{
	BB_ASSERT(a_Size < UINT32_MAX, "upload buffer alloc size is larger then UINT32_MAX, this is not supported");
	BB_ASSERT(m_Size >= m_Offset + a_Size, "Now enough space to alloc in the uploadbuffer.");
	UploadBufferChunk t_Chunk{};
	t_Chunk.memory = Pointer::Add(m_Start, m_Offset);
	t_Chunk.offset = m_Offset;
	t_Chunk.size = static_cast<uint32_t>(a_Size);
	m_Offset += static_cast<uint32_t>(a_Size);
	return t_Chunk;
}

void UploadBuffer::Clear()
{
	m_Offset = 0;
	memset(m_Start, 0, m_Size);
}

class DescriptorHeap
{
public:
	DescriptorHeap(const DescriptorHeapCreateInfo& a_CreateInfo, const bool a_GpuVisible)
		: m_DescriptorMax(a_CreateInfo.descriptorCount), m_DescriptorStartOffset(0), m_GpuVisible(a_GpuVisible)
	{
		m_DescriptorHeapPos = 0;
		m_Heap = s_ApiFunc.createDescriptorHeap(a_CreateInfo, a_GpuVisible);
	}
	~DescriptorHeap()
	{
		//only delete the heap if it's from the start offset.
		if (m_DescriptorStartOffset == 0)
			s_ApiFunc.destroyDescriptorHeap(m_Heap);
		memset(this, 0, sizeof(DescriptorHeap));
	}

	const DescriptorAllocation Allocate(const RDescriptor a_Descriptor)
	{
		AllocateDescriptorInfo t_Info;
		t_Info.heap = m_Heap;
		t_Info.descriptor = a_Descriptor;
		t_Info.heapOffset = m_DescriptorStartOffset + m_DescriptorHeapPos;
		DescriptorAllocation t_Allocation = s_ApiFunc.allocateDescriptor(t_Info);
		m_DescriptorHeapPos += t_Allocation.descriptorCount;
		BB_ASSERT(m_DescriptorMax > m_DescriptorHeapPos,
			"Descriptor Heap, over allocating descriptor memory!");
		return t_Allocation;
	}

	//do not use this, descriptorheapmanager handles this now with seperate heaps per backbuffer.
	inline DescriptorHeap SubAllocate(const uint32_t a_DescriptorCount)
	{
		const uint32_t t_NewHeapOffset = m_DescriptorHeapPos;
		m_DescriptorHeapPos += a_DescriptorCount;
		BB_ASSERT(m_DescriptorHeapPos > m_DescriptorMax,
			"Descriptor Heap, over allocating descriptor memory!");
		return DescriptorHeap(m_Heap, t_NewHeapOffset, a_DescriptorCount, m_GpuVisible);
	}

	inline void Reset()
	{
		m_DescriptorHeapPos = 0;
	}

	inline RDescriptorHeap GetHeap() const { return m_Heap; }
	inline const uint32_t GetHeapOffset() const { return m_DescriptorStartOffset; }
	inline void SetHeapOffset(const uint32_t a_DescriptorOffset)
	{ 
		m_DescriptorHeapPos = a_DescriptorOffset; 
	}
	inline const uint32_t GetHeapSize() const { return m_DescriptorMax; }

private:
	//Special constructor that is a suballocated heap.
	//DescriptorHeap::SubAllocate uses this.
	DescriptorHeap(const RDescriptorHeap& a_Heap, const uint32_t a_DescriptorCount, const uint32_t a_HeapOffset, const bool a_GpuVisible)
		: m_Heap(a_Heap), m_DescriptorMax(a_DescriptorCount), m_DescriptorStartOffset(a_HeapOffset), m_GpuVisible(a_GpuVisible) 
	{
		m_DescriptorHeapPos = 0;
	}
	RDescriptorHeap m_Heap;
	const uint32_t m_DescriptorMax;
	const uint32_t m_DescriptorStartOffset;
	uint32_t m_DescriptorHeapPos;
	const bool m_GpuVisible;
};

struct BB::DescriptorManager_inst
{
	DescriptorManager_inst(Allocator a_SystemAllocator, const DescriptorHeapCreateInfo& a_CreateInfo, const uint32_t a_BackbufferCount)
		: backBufferCount(a_BackbufferCount), uploadHeap(a_CreateInfo, false)
	{
		//allocate all the GPU visible heaps, we create equal per frame.
		gpuHeaps = BBnewArr(a_SystemAllocator, backBufferCount, DescriptorHeap);
		for (size_t i = 0; i < a_BackbufferCount; i++)
		{
			new (&gpuHeaps[i])DescriptorHeap(a_CreateInfo, true);
		}
	}
	~DescriptorManager_inst()
	{
		for (size_t i = 0; i < backBufferCount; i++)
		{
			//leak lmao I'm lazy you shouldn't be destroying this without refreshing the entire render system allocator anyway.
			gpuHeaps[i].~DescriptorHeap();
		}
	}
	DescriptorHeap uploadHeap;
	//maximum of 3 frames
	const uint32_t backBufferCount;
	DescriptorHeap* gpuHeaps;
};

//Will leak memory if you destruct it, but you should realistically only make this once.
DescriptorManager::DescriptorManager(Allocator a_SystemAllocator, const DescriptorHeapCreateInfo& a_CreateInfo, const uint32_t a_BackbufferCount)
{
	m_Inst = BBnew(a_SystemAllocator, DescriptorManager_inst)(a_SystemAllocator, a_CreateInfo, a_BackbufferCount);
}

//MEMORY LEAK! m_Inst is leaked.
DescriptorManager::~DescriptorManager()
{
	m_Inst->~DescriptorManager_inst();
}

DescriptorAllocation DescriptorManager::Allocate(const RDescriptor a_Descriptor)
{
	return m_Inst->uploadHeap.Allocate(a_Descriptor);
}

void DescriptorManager::UploadToGPUHeap(const uint32_t a_FrameNum) const
{
	BB_ASSERT(a_FrameNum < m_Inst->backBufferCount,
		"Trying to get a GPU descriptor heap that goes over the amount of backbuffers the renderer has");
	CopyDescriptorsInfo t_Info;
	t_Info.descriptorCount = m_Inst->uploadHeap.GetHeapSize();
	t_Info.dstHeap = m_Inst->gpuHeaps[a_FrameNum].GetHeap();
	t_Info.dstOffset = m_Inst->gpuHeaps[a_FrameNum].GetHeapOffset();
	t_Info.srcHeap = m_Inst->uploadHeap.GetHeap();
	t_Info.srcOffset = m_Inst->uploadHeap.GetHeapOffset();
	s_ApiFunc.copyDescriptors(t_Info);
}

const uint32_t DescriptorManager::GetCPUOffsetFlag() const
{
	return m_Inst->uploadHeap.GetHeapOffset();
}

void DescriptorManager::SetCPUOffsetFlag(const uint32_t a_Offset)
{
	m_Inst->uploadHeap.SetHeapOffset(a_Offset);
}

void DescriptorManager::ClearCPUHeap()
{ 
	m_Inst->uploadHeap.Reset();
};

const RDescriptorHeap DescriptorManager::GetGPUHeap(const uint32_t a_FrameNum) const
{ 
	BB_ASSERT(a_FrameNum < m_Inst->backBufferCount, 
		"Trying to get a GPU descriptor heap that goes over the amount of backbuffers the renderer has");
	return m_Inst->gpuHeaps[a_FrameNum].GetHeap();
}

const uint32_t DescriptorManager::GetHeapOffset() const
{ 
	return m_Inst->uploadHeap.GetHeapOffset();
}

const uint32_t DescriptorManager::GetHeapSize() const
{ 
	return m_Inst->uploadHeap.GetHeapSize(); 
}

LinearRenderBuffer::LinearRenderBuffer(const RenderBufferCreateInfo& a_CreateInfo)
	: m_Size(a_CreateInfo.size), m_MemoryProperties(a_CreateInfo.memProperties), m_Buffer(RenderBackend::CreateBuffer(a_CreateInfo))
{
	m_Used = 0;
}
LinearRenderBuffer::~LinearRenderBuffer()
{
	RenderBackend::DestroyBuffer(m_Buffer);
}

RenderBufferPart LinearRenderBuffer::SubAllocate(const uint64_t a_Size, const uint32_t a_Alignment)
{
	//Align the m_Used variable, as it works as the buffer offset.
	const size_t t_AdjustSize = static_cast<uint32_t>(Pointer::AlignPad(a_Size, a_Alignment));
	BB_ASSERT(m_Size >= static_cast<uint64_t>(m_Used + t_AdjustSize), "Not enough memory for a linear render buffer!");

	const RenderBufferPart t_Part{ m_Buffer,
	static_cast<uint32_t>(t_AdjustSize),
	static_cast<uint32_t>(m_Used) };

	m_Used += t_AdjustSize;

	return t_Part;
}

void LinearRenderBuffer::MapBuffer() const
{
	BB_ASSERT(m_MemoryProperties == RENDER_MEMORY_PROPERTIES::HOST_VISIBLE,
		"Trying to map a GPU device local memory region! Create the buffer with HOST_VISIBLE instead!");
	RenderBackend::MapMemory(m_Buffer);
}

void LinearRenderBuffer::UnmapBuffer() const
{
	BB_ASSERT(m_MemoryProperties == RENDER_MEMORY_PROPERTIES::HOST_VISIBLE,
		"Trying to unmap a GPU device local memory region! Create the buffer with HOST_VISIBLE instead!");
	RenderBackend::UnmapMemory(m_Buffer);
}

#pragma region API_Backend
void BB::RenderBackend::DisplayDebugInfo()
{
	s_ResourceTracker.Editor();	
}

const uint32_t BB::RenderBackend::GetFrameBufferAmount()
{
	return s_BackendInfo.framebufferCount;
}

//Preferably use the value that you get from BB::RenderBackend::StartFrame
const FrameIndex BB::RenderBackend::GetCurrentFrameBufferIndex()
{
	return s_BackendInfo.currentFrame;
}

void BB::RenderBackend::InitBackend(const RenderBackendCreateInfo& a_CreateInfo, Allocator a_SystemAllocator)
{
	a_CreateInfo.getApiFuncPtr(s_ApiFunc);

	s_BackendInfo = s_ApiFunc.createBackend(a_CreateInfo);
}

RDescriptor BB::RenderBackend::CreateDescriptor(const RenderDescriptorCreateInfo& a_CreateInfo)
{
	RDescriptor t_Desc = s_ApiFunc.createDescriptor(a_CreateInfo);
#ifdef _DEBUG
	s_ResourceTracker.AddDescriptor(a_CreateInfo, a_CreateInfo.name, t_Desc.handle);
#endif //_DEBUG

	return t_Desc;
}

CommandQueueHandle BB::RenderBackend::CreateCommandQueue(const RenderCommandQueueCreateInfo& a_CreateInfo)
{
	CommandQueueHandle t_Queue = s_ApiFunc.createCommandQueue(a_CreateInfo);
#ifdef _DEBUG
	s_ResourceTracker.AddQueue(a_CreateInfo, a_CreateInfo.name, t_Queue.handle);
#endif //_DEBUG
	return t_Queue;
}

CommandAllocatorHandle BB::RenderBackend::CreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo)
{
	CommandAllocatorHandle t_CmdAllocator = s_ApiFunc.createCommandAllocator(a_CreateInfo);
#ifdef _DEBUG
	s_ResourceTracker.AddCommandAllocator(a_CreateInfo, a_CreateInfo.name, t_CmdAllocator.handle);
#endif //_DEBUG
	return t_CmdAllocator;
}

CommandListHandle BB::RenderBackend::CreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo)
{
	CommandListHandle t_CmdList = s_ApiFunc.createCommandList(a_CreateInfo);
#ifdef _DEBUG
	s_ResourceTracker.AddCommandList(a_CreateInfo, a_CreateInfo.name, t_CmdList.handle);
#endif //_DEBUG
	return t_CmdList;
}

RBufferHandle BB::RenderBackend::CreateBuffer(const RenderBufferCreateInfo& a_CreateInfo)
{
	RBufferHandle t_Buffer = s_ApiFunc.createBuffer(a_CreateInfo);
#ifdef _DEBUG
	s_ResourceTracker.AddBuffer(a_CreateInfo, a_CreateInfo.name, t_Buffer.handle);
#endif //_DEBUG
	return t_Buffer;
}

RImageHandle BB::RenderBackend::CreateImage(const RenderImageCreateInfo& a_CreateInfo)
{
	BB_ASSERT(a_CreateInfo.width != 0, "Image width is 0! Choose a correct width for an image.");
	BB_ASSERT(a_CreateInfo.height != 0, "Image height is 0! Choose a correct height for an image.");
	BB_ASSERT(a_CreateInfo.depth != 0, "Image depth is 0! Standard 2d texture should have a depth of 1.");
	BB_ASSERT(a_CreateInfo.arrayLayers != 0, "Image arrayLayers is 0! Standard should be 1 if you do not do anything special for a 2d image.");
	BB_ASSERT(a_CreateInfo.mipLevels != 0, "Image mipLevels is 0! Standard should be 1 if you do not do mips for an image.");
	
	RImageHandle t_Image = s_ApiFunc.createImage(a_CreateInfo);
#ifdef _DEBUG
	TrackerImageInfo t_TrackInfo{ RENDER_IMAGE_LAYOUT::UNDEFINED, RENDER_IMAGE_LAYOUT::UNDEFINED, a_CreateInfo };
	s_ResourceTracker.AddImage(t_TrackInfo, a_CreateInfo.name, t_Image.handle);
#endif //_DEBUG
	
	return t_Image;
}

RSamplerHandle BB::RenderBackend::CreateSampler(const SamplerCreateInfo& a_CreateInfo)
{
	RSamplerHandle t_Sampler = s_ApiFunc.createSampler(a_CreateInfo);
#ifdef _DEBUG
	s_ResourceTracker.AddSampler(a_CreateInfo, a_CreateInfo.name, t_Sampler.handle);
#endif //_DEBUG
	return t_Sampler;
}

RFenceHandle BB::RenderBackend::CreateFence(const FenceCreateInfo& a_CreateInfo)
{
	RFenceHandle t_Fence = s_ApiFunc.createFence(a_CreateInfo);
#ifdef _DEBUG
	s_ResourceTracker.AddFence(a_CreateInfo, a_CreateInfo.name, t_Fence.handle);
#endif //_DEBUG
	return t_Fence;
}

void BB::RenderBackend::SetResourceName(const SetResourceNameInfo& a_Info)
{
#ifdef _DEBUG
	s_ResourceTracker.ChangeName(a_Info.resouceHandle, a_Info.name);
#endif //_DEBUG
	s_ApiFunc.setResourceName(a_Info);
}

void BB::RenderBackend::WriteDescriptors(const WriteDescriptorInfos& a_WriteInfo)
{
	s_ApiFunc.writeDescriptors(a_WriteInfo);
}

void BB::RenderBackend::CopyDescriptors(const CopyDescriptorsInfo& a_CopyInfo)
{
	s_ApiFunc.copyDescriptors(a_CopyInfo);
}

void BB::RenderBackend::ResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle)
{
	s_ApiFunc.resetCommandAllocator(a_CmdAllocatorHandle);
}

void BB::RenderBackend::StartCommandList(const CommandListHandle a_CmdHandle)
{
	s_ApiFunc.startCommandList(a_CmdHandle);
}

void BB::RenderBackend::EndCommandList(const CommandListHandle a_RecordingCmdHandle)
{
	s_ApiFunc.endCommandList(a_RecordingCmdHandle);
}

void BB::RenderBackend::StartRendering(const CommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_StartInfo)
{
	s_ApiFunc.startRendering(a_RecordingCmdHandle, a_StartInfo);
}

void BB::RenderBackend::SetScissor(const CommandListHandle a_RecordingCmdHandle, const ScissorInfo& a_ScissorInfo)
{
	s_ApiFunc.setScissor(a_RecordingCmdHandle, a_ScissorInfo);
}

void BB::RenderBackend::EndRendering(const CommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo)
{
	s_ApiFunc.endRendering(a_RecordingCmdHandle, a_EndInfo);
}

ImageReturnInfo BB::RenderBackend::GetImageInfo(const RImageHandle a_Handle)
{
	return s_ApiFunc.getImageInfo(a_Handle);
}

void BB::RenderBackend::CopyBuffer(const CommandListHandle a_RecordingCmdHandle, const RenderCopyBufferInfo& a_CopyInfo)
{
	s_ApiFunc.copyBuffer(a_RecordingCmdHandle, a_CopyInfo);
}

void BB::RenderBackend::CopyBufferImage(const CommandListHandle a_RecordingCmdHandle, const RenderCopyBufferImageInfo& a_CopyInfo)
{
	s_ApiFunc.copyBufferImage(a_RecordingCmdHandle, a_CopyInfo);
}

void BB::RenderBackend::SetPipelineBarriers(const CommandListHandle a_RecordingCmdHandle, const PipelineBarrierInfo& a_BarrierInfo)
{
#ifdef _DEBUG
	for (size_t i = 0; i < a_BarrierInfo.imageInfoCount; i++)
	{
		const PipelineBarrierImageInfo& t_ImageInfo = a_BarrierInfo.imageInfos[i];
		TrackerImageInfo* t_EditorData = reinterpret_cast<TrackerImageInfo*>(s_ResourceTracker.GetData(t_ImageInfo.image.handle, RESOURCE_TYPE::IMAGE));
		BB_ASSERT(t_EditorData->currentLayout == t_ImageInfo.oldLayout, "Old image layout not the same in the tracked info!");
		t_EditorData->oldLayout = t_EditorData->currentLayout;
		t_EditorData->currentLayout = t_ImageInfo.newLayout;
	}
#endif //_DEBUG
	s_ApiFunc.setPipelineBarriers(a_RecordingCmdHandle, a_BarrierInfo);
}

void BB::RenderBackend::BindDescriptorHeaps(const CommandListHandle a_RecordingCmdHandle, const RDescriptorHeap a_ResourceHeap, const RDescriptorHeap a_SamplerHeap)
{
	s_ApiFunc.bindDescriptorHeaps(a_RecordingCmdHandle, a_ResourceHeap, a_SamplerHeap);
}

void BB::RenderBackend::BindPipeline(const CommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline)
{
	s_ApiFunc.bindPipeline(a_RecordingCmdHandle, a_Pipeline);
}

void BB::RenderBackend::SetDescriptorHeapOffsets(const CommandListHandle a_RecordingCmdHandle, const RENDER_DESCRIPTOR_SET a_FirstSet, const uint32_t a_SetCount, const uint32_t* a_HeapIndex, const size_t* a_Offsets)
{
	s_ApiFunc.setDescriptorHeapOffsets(a_RecordingCmdHandle, a_FirstSet, a_SetCount, a_HeapIndex, a_Offsets);
}

void BB::RenderBackend::BindVertexBuffers(const CommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount)
{
	s_ApiFunc.bindVertBuffers(a_RecordingCmdHandle, a_Buffers, a_BufferOffsets, a_BufferCount);
}

void BB::RenderBackend::BindIndexBuffer(const CommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset)
{
	s_ApiFunc.bindIndexBuffer(a_RecordingCmdHandle, a_Buffer, a_Offset);
}

void BB::RenderBackend::BindConstant(const CommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_DwordOffset, const void* a_Data)
{
	BB_WARNING(static_cast<size_t>(a_DwordCount + a_DwordOffset) * sizeof(uint32_t) < 128, "Constant size is bigger then 128, this might not work on all hardware for Vulkan!", WarningType::HIGH);
	s_ApiFunc.bindConstant(a_RecordingCmdHandle, a_ConstantIndex, a_DwordCount, a_DwordOffset, a_Data);
}

void BB::RenderBackend::DrawVertex(const CommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance)
{
	s_ApiFunc.drawVertex(a_RecordingCmdHandle, a_VertexCount, a_InstanceCount, a_FirstVertex, a_FirstInstance);
}

void BB::RenderBackend::DrawIndexed(const CommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance)
{
	s_ApiFunc.drawIndex(a_RecordingCmdHandle, a_IndexCount, a_InstanceCount, a_FirstIndex, a_VertexOffset, a_FirstInstance);
}

void BB::RenderBackend::BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset)
{
	s_ApiFunc.bufferCopyData(a_Handle, a_Data, a_Size, a_Offset);
}

void* BB::RenderBackend::MapMemory(const RBufferHandle a_Handle)
{
	return s_ApiFunc.mapMemory(a_Handle);
}

void BB::RenderBackend::UnmapMemory(const RBufferHandle a_Handle)
{
	s_ApiFunc.unmapMemory(a_Handle);
}

void BB::RenderBackend::StartFrame(const StartFrameInfo& a_StartInfo)
{
	s_ApiFunc.startFrame(a_StartInfo);
}

void BB::RenderBackend::ExecuteCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount)
{
	s_ApiFunc.executeCommands(a_ExecuteQueue, a_ExecuteInfos, a_ExecuteInfoCount);
}

void BB::RenderBackend::ExecutePresentCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo)
{
	s_ApiFunc.executePresentCommands(a_ExecuteQueue, a_ExecuteInfo);
}

FrameIndex BB::RenderBackend::PresentFrame(const PresentFrameInfo& a_PresentInfo)
{
	return s_BackendInfo.currentFrame = s_ApiFunc.presentFrame(a_PresentInfo);
}

void BB::RenderBackend::WaitCommands(const RenderWaitCommandsInfo& a_WaitInfo)
{
	s_ApiFunc.waitCommands(a_WaitInfo);
}

void BB::RenderBackend::ResizeWindow(uint32_t a_X, uint32_t a_Y)
{
	s_ApiFunc.resizeWindow(a_X, a_Y);
}

void BB::RenderBackend::DestroyFence(const RFenceHandle a_Handle)
{
#ifdef _DEBUG
	s_ResourceTracker.RemoveEntry(a_Handle.handle);
#endif //_DEBUG
	s_ApiFunc.destroyFence(a_Handle);
}

void BB::RenderBackend::DestroySampler(const RSamplerHandle a_Handle)
{
#ifdef _DEBUG
	s_ResourceTracker.RemoveEntry(a_Handle.handle);
#endif //_DEBUG
	s_ApiFunc.destroySampler(a_Handle);
}

void BB::RenderBackend::DestroyImage(const RImageHandle a_Handle)
{
#ifdef _DEBUG
	s_ResourceTracker.RemoveEntry(a_Handle.handle);
#endif //_DEBUG
	s_ApiFunc.destroyImage(a_Handle);
}

void BB::RenderBackend::DestroyBuffer(const RBufferHandle a_Handle)
{
#ifdef _DEBUG
	s_ResourceTracker.RemoveEntry(a_Handle.handle);
#endif //_DEBUG
	s_ApiFunc.destroyBuffer(a_Handle);
}

void BB::RenderBackend::DestroyCommandList(const CommandListHandle a_Handle)
{
#ifdef _DEBUG
	s_ResourceTracker.RemoveEntry(a_Handle.handle);
#endif //_DEBUG
	s_ApiFunc.destroyCommandList(a_Handle);
}

void BB::RenderBackend::DestroyCommandAllocator(const CommandAllocatorHandle a_Handle)
{
#ifdef _DEBUG
	s_ResourceTracker.RemoveEntry(a_Handle.handle);
#endif //_DEBUG
	s_ApiFunc.destroyCommandAllocator(a_Handle);
}

void BB::RenderBackend::DestroyCommandQueue(const CommandQueueHandle a_Handle)
{
#ifdef _DEBUG
	s_ResourceTracker.RemoveEntry(a_Handle.handle);
#endif //_DEBUG
	s_ApiFunc.destroyCommandQueue(a_Handle);
}

void BB::RenderBackend::DestroyPipeline(const PipelineHandle a_Handle)
{
#ifdef _DEBUG
	s_ResourceTracker.RemoveEntry(a_Handle.handle);
#endif //_DEBUG
	s_ApiFunc.destroyPipeline(a_Handle);
}

void BB::RenderBackend::DestroyDescriptor(const RDescriptor a_Handle)
{
#ifdef _DEBUG
	s_ResourceTracker.RemoveEntry(a_Handle.handle);
#endif //_DEBUG
	s_ApiFunc.destroyDescriptor(a_Handle);
}

void BB::RenderBackend::DestroyBackend()
{
	s_ApiFunc.destroyBackend();
}
#pragma endregion //API_Backend
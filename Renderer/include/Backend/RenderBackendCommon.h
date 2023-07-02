#pragma once
#include <cstdint>
#include "Common.h"
#include "Utils/Slice.h"
#include "Storage/FixedArray.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif //_WIN32

namespace BB
{
	//Hardware minimally supports 4 binding sets on Vulkan. So we make the hard limit for VK and DX12.
	constexpr uint32_t BINDING_MAX = 4;
	constexpr uint32_t DESCRIPTOR_IMAGE_MAX = 1024;
	//arbritrary for now, but handy for array creation.
	constexpr size_t STATIC_SAMPLER_MAX = 8;

	using FrameIndex = uint32_t;
	
	//Index is the start index, Index 
	using PipelineBuilderHandle = FrameworkHandle<struct PipelineBuilderHandleTag>;
	using PipelineHandle = FrameworkHandle<struct PipelineHandleTag>;
	using RDescriptor = FrameworkHandle<struct RDescriptorHandleTag>;
	using CommandQueueHandle = FrameworkHandle<struct CommandQueueHandleTag>;
	using CommandAllocatorHandle = FrameworkHandle<struct CommandAllocatorHandleTag>;
	using CommandListHandle = FrameworkHandle<struct CommandListHandleTag>;
	using RecordingCommandListHandle = FrameworkHandle<struct RecordingCommandListHandleTag>;

	using RDescriptorHeap = FrameworkHandle<struct RDescriptorHeapTag>;
	using RDescriptorAllocation = FrameworkHandle<struct RDescriptorAllocationTag>;

	using RFenceHandle = FrameworkHandle<struct RFenceHandleTag>;
	using RBufferHandle = FrameworkHandle<struct RBufferHandleTag>;
	using RImageHandle = FrameworkHandle<struct RImageHandleTag>;
	using RSamplerHandle = FrameworkHandle<struct RSamplerHandleTag>;
	using RShaderHandle = FrameworkHandle<struct RShaderHandleTag>;

	using ShaderCodeHandle = FrameworkHandle<struct ShaderCodeHandleTag>;

	enum class RENDER_API
	{
		NONE, //None means that the renderer is destroyed or not initialized.
		VULKAN,
		DX12
	};

	enum class RENDER_BUFFER_USAGE : uint32_t
	{
		VERTEX,
		INDEX,
		UNIFORM,
		STORAGE,
		STAGING
	};

	enum class RENDER_DESCRIPTOR_SET : uint32_t
	{
		SCENE_SET = 0,
		PER_FRAME_SET = 1,
		PER_MESH_SET = 2,
		PER_MATERIAL_SET = 3
	};

	enum class RENDER_DESCRIPTOR_TYPE : uint32_t
	{
		READONLY_CONSTANT, //CBV or uniform buffer
		READONLY_BUFFER, //SRV or Storage buffer
		READWRITE, //UAV or readwrite storage buffer(?)
		IMAGE,
		SAMPLER,
		ENUM_SIZE
	};

	enum class RENDER_DESCRIPTOR_FLAG : uint32_t
	{
		NONE,
		BINDLESS
	}; 

	enum class RENDER_MEMORY_PROPERTIES : uint32_t
	{
		DEVICE_LOCAL,
		HOST_VISIBLE
	};

	enum class RENDER_IMAGE_TYPE : uint32_t
	{
		TYPE_2D
	};

	enum class RENDER_IMAGE_FORMAT : uint32_t
	{
		DEPTH_STENCIL,
		RGBA8_SRGB,
		RGBA8_UNORM
	};

	enum class RENDER_IMAGE_TILING : uint32_t
	{
		LINEAR,
		OPTIMAL
	};

	enum class RENDER_IMAGE_LAYOUT : uint32_t
	{
		UNDEFINED,
		GENERAL,
		TRANSFER_SRC,
		TRANSFER_DST,
		COLOR_ATTACHMENT_OPTIMAL,
		DEPTH_STENCIL_ATTACHMENT,
		SHADER_READ_ONLY,
		PRESENT
	};

	enum class RENDER_PIPELINE_STAGE : uint32_t
	{
		TOP_OF_PIPELINE,
		TRANSFER,
		VERTEX_INPUT,
		VERTEX_SHADER,
		EARLY_FRAG_TEST,
		FRAGMENT_SHADER,
		END_OF_PIPELINE
	};

	enum class RENDER_ACCESS_MASK : uint32_t
	{
		NONE = 0,
		TRANSFER_WRITE,
		DEPTH_STENCIL_READ_WRITE,
		SHADER_READ
	};

	enum class RENDER_SHADER_STAGE : uint32_t
	{
		ALL,
		VERTEX,
		FRAGMENT_PIXEL
	};

	enum class RENDER_LOAD_OP : uint32_t
	{
		LOAD,
		CLEAR,
		DONT_CARE
	};

	enum class RENDER_STORE_OP : uint32_t
	{
		STORE,
		DONT_CARE
	};

	enum class RENDER_LOGIC_OP : uint32_t
	{
		CLEAR,
		COPY
	};

	enum class RENDER_EXTENSIONS : uint32_t
	{
		STANDARD_VULKAN_INSTANCE,
		STANDARD_VULKAN_DEVICE, //VK Device Property.
		STANDARD_DX12,
		DEBUG,
		PIPELINE_EXTENDED_DYNAMIC_STATE //VK Device Property.
	};

	enum class RENDER_QUEUE_TYPE : uint32_t
	{
		GRAPHICS,
		TRANSFER_COPY,
		COMPUTE
	};

	enum class RENDER_FENCE_FLAGS : uint32_t
	{
		NONE = 0,
		CREATE_SIGNALED
	};

	enum class RENDER_BINDING_SET : uint32_t
	{
		PER_FRAME = 0,
		PER_PASS = 1,
		PER_MATERIAL = 2,
		PER_OBJECT = 3
	};

	enum class RENDER_INPUT_FORMAT : uint32_t
	{
		RGBA32,
		RGB32,
		RG32,
		R32,
		RGBA8,
		RG8
	};

	enum class RENDER_BLEND_FACTOR
	{
		ZERO,
		ONE,
		SRC_ALPHA,
		ONE_MINUS_SRC_ALPHA
	};

	enum class RENDER_BLEND_OP
	{
		ADD,
		SUBTRACT
	};

	enum class RENDER_CULL_MODE
	{
		NONE,
		FRONT,
		BACK
	};

	enum class SAMPLER_ADDRESS_MODE
	{
		REPEAT,
		MIRROR,
		BORDER,
		CLAMP
	};

	enum class SAMPLER_FILTER
	{
		NEAREST,
		LINEAR
	};

	struct RenderInitInfo
	{
		RENDER_API renderAPI = RENDER_API::NONE;
		WindowHandle windowHandle = {};
		LibHandle renderDll = {};
		bool debug = false;
	};

	struct RenderAPIFunctions;
	typedef void (*PFN_RenderGetAPIFunctions)(RenderAPIFunctions&);

	struct RenderBackendCreateInfo
	{
		PFN_RenderGetAPIFunctions getApiFuncPtr = nullptr;
		Slice<RENDER_EXTENSIONS> extensions{};
		Slice<RENDER_EXTENSIONS> deviceExtensions{};
		WindowHandle windowHandle{};
		const char* appName = nullptr;
		const char* engineName = nullptr;
		uint32_t windowWidth = 0;
		uint32_t windowHeight = 0;
		bool validationLayers = false;
	};

	struct SamplerCreateInfo
	{
		const char* name = nullptr;
		SAMPLER_ADDRESS_MODE addressModeU{};
		SAMPLER_ADDRESS_MODE addressModeV{};
		SAMPLER_ADDRESS_MODE addressModeW{};

		SAMPLER_FILTER filter{};
		float maxAnistoropy = 0;

		float minLod = 0;
		float maxLod = 0;
	};

	struct DescriptorBinding
	{
		uint32_t binding = 0;
		uint32_t descriptorCount = 0;
		RENDER_DESCRIPTOR_TYPE type{};
		RENDER_SHADER_STAGE stage{};
		RENDER_DESCRIPTOR_FLAG flags{};
	};

	struct DescriptorAllocation
	{
		uint32_t offset = 0;
		RDescriptor descriptor{};
		void* bufferStart = nullptr;
		//can be size in bytes, or the amount of descriptors.
		uint32_t descriptorCount = 0;
	};

	struct DescriptorHeapCreateInfo
	{
		const char* name;
		uint32_t descriptorCount;
		bool isSampler;
	};

	struct CopyDescriptorsInfo
	{
		uint32_t descriptorCount;
		bool isSamplerHeap;
		RDescriptorHeap srcHeap;
		uint32_t srcOffset;
		RDescriptorHeap dstHeap;
		uint32_t dstOffset;
	};

	struct WriteDescriptorBuffer
	{
		RBufferHandle buffer;
		size_t range;
		size_t offset;
	};

	struct WriteDescriptorImage
	{
		RImageHandle image;
		RSamplerHandle sampler;
		RENDER_IMAGE_LAYOUT layout;
	};

	struct WriteDescriptorData
	{
		uint32_t binding = 0;
		uint32_t descriptorIndex = 0;
		RENDER_DESCRIPTOR_TYPE type{};
		union
		{
			WriteDescriptorBuffer buffer{};
			WriteDescriptorImage image;
		};
	};

	struct AllocateDescriptorInfo
	{
		RDescriptorHeap heap;
		RDescriptor descriptor;
		uint32_t heapOffset = 0;
	};

	struct WriteDescriptorInfos
	{
		RDescriptor descriptorHandle{};
		DescriptorAllocation allocation;

		BB::Slice<WriteDescriptorData> data;
	};

	struct RenderDescriptorCreateInfo
	{
		const char* name = nullptr;
		BB::Slice<DescriptorBinding> bindings{};
	};

	struct RenderCommandQueueCreateInfo
	{
		const char* name = nullptr;
		RENDER_QUEUE_TYPE queue{};
		RENDER_FENCE_FLAGS flags{};
	};

	struct RenderCommandAllocatorCreateInfo
	{
		const char* name = nullptr;
		RENDER_QUEUE_TYPE queueType{};
		uint32_t commandListCount = 0;
	};

	struct RenderCommandListCreateInfo
	{
		const char* name = nullptr;
		CommandAllocatorHandle commandAllocator{};
	};

	struct RenderWaitCommandsInfo
	{
		BB::Slice<CommandQueueHandle> queues{};
		BB::Slice<RFenceHandle> fences{};
	};

	struct RenderBufferCreateInfo
	{
		const char* name = nullptr;
		uint64_t size = 0;
		RENDER_BUFFER_USAGE usage{};
		RENDER_MEMORY_PROPERTIES memProperties{};
	};

	struct RenderImageCreateInfo
	{
		const char* name = nullptr;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0;

		uint16_t arrayLayers = 0;
		uint16_t mipLevels = 0;
		RENDER_IMAGE_TYPE type{};
		RENDER_IMAGE_FORMAT format{};
		RENDER_IMAGE_TILING tiling{};
	};

	struct FenceCreateInfo
	{
		const char* name = nullptr;
		RENDER_FENCE_FLAGS flags{};
	};

	struct RenderTransitionImageInfo
	{
		RImageHandle image{};
		RENDER_IMAGE_LAYOUT oldLayout{};
		RENDER_IMAGE_LAYOUT newLayout{};
		RENDER_PIPELINE_STAGE srcStage{};
		RENDER_PIPELINE_STAGE dstStage{};
		RENDER_ACCESS_MASK srcMask{};
		RENDER_ACCESS_MASK dstMask{};

		uint32_t baseMipLevel = 0;
		uint32_t levelCount = 0;
		uint32_t baseArrayLayer = 0;
		uint32_t layerCount = 0;
	};

	struct StartFrameInfo
	{
		RFenceHandle imageWait{};
		RFenceHandle* fences = nullptr;
		uint32_t fenceCount = 0;
	};

	struct ImageReturnInfo
	{
		struct AllocInfo
		{
			uint64_t imageAllocByteSize = 0;
			uint32_t footRowPitch = 0;
			uint32_t footHeight = 0;
		} allocInfo{};

		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0;
		uint16_t mips = 0;
		uint16_t arrayLayers = 0;
	};

	struct RenderCopyBufferInfo
	{
		uint64_t size = 0;
		RBufferHandle src{};
		uint64_t srcOffset = 0;
		RBufferHandle dst{};
		uint64_t dstOffset = 0;
	};

	struct ImageCopyInfo
	{
		uint32_t sizeX = 0;
		uint32_t sizeY = 0;
		uint32_t sizeZ = 0;

		int32_t offsetX = 0;
		int32_t offsetY = 0;
		int32_t offsetZ = 0;

		uint16_t mipLevel = 0;
		uint16_t baseArrayLayer = 0;
		uint16_t layerCount = 0;

		RENDER_IMAGE_LAYOUT layout{};
	};

	struct RenderCopyBufferImageInfo
	{
		RBufferHandle srcBuffer{};
		uint32_t srcBufferOffset = 0;

		RImageHandle dstImage{};
		ImageCopyInfo dstImageInfo{};
	};

	struct StartRenderingInfo
	{
		uint32_t viewportWidth = 0;
		uint32_t viewportHeight = 0;

		RENDER_LOAD_OP colorLoadOp{};
		RENDER_STORE_OP colorStoreOp{};
		RENDER_IMAGE_LAYOUT colorInitialLayout{};
		RENDER_IMAGE_LAYOUT colorFinalLayout{};

		RImageHandle depthStencil{};

		//RGBA
		float clearColor[4]{};
	};

	struct ScissorInfo
	{
		int2 offset{};
		uint2 extent{};
	};

	struct EndRenderingInfo
	{
		RENDER_IMAGE_LAYOUT colorInitialLayout{};
		RENDER_IMAGE_LAYOUT colorFinalLayout{};
	};

	struct PresentFrameInfo
	{

	};

	struct PipelineRenderTargetBlend
	{
		bool blendEnable = false;
		RENDER_BLEND_FACTOR srcBlend{};
		RENDER_BLEND_FACTOR dstBlend{};
		RENDER_BLEND_OP blendOp{};
		RENDER_BLEND_FACTOR srcBlendAlpha{};
		RENDER_BLEND_FACTOR dstBlendAlpha{};
		RENDER_BLEND_OP blendOpAlpha{};
		//uint8_t renderTargetWriteMask;
	};

	struct PipelineRasterState
	{
		bool frontCounterClockwise = false;
		RENDER_CULL_MODE cullMode{};
	};

	struct PipelineConstantData
	{
		//0 means this pipeline does not use push constants/root constants.
		uint32_t dwordSize = 0;
		RENDER_SHADER_STAGE shaderStage{};
	};

	struct PipelineInitInfo
	{
		const char* name = nullptr;
		PipelineRasterState rasterizerState{};
		PipelineConstantData constantData{};

		bool blendLogicOpEnable = false;
		bool enableDepthTest = false;
		RENDER_LOGIC_OP blendLogicOp;
		uint32_t renderTargetBlendCount = 0;
		PipelineRenderTargetBlend* renderTargetBlends = nullptr;

		BB::Slice<SamplerCreateInfo> immutableSamplers{};
	};

	struct VertexAttributeDesc
	{
		uint32_t location = 0;
		RENDER_INPUT_FORMAT format{};
		uint32_t offset = 0;
		char* semanticName = nullptr;
	};

	struct PipelineAttributes
	{
		uint32_t stride = 0;
		BB::Slice<VertexAttributeDesc> attributes{};
	};

#ifdef _DEBUG
	struct PipelineDebugInfo
	{
		struct ShaderInfo
		{
			const char* optionalShaderpath = nullptr;
			RENDER_SHADER_STAGE shaderStage{};
		};
		bool enableDepthTest = false;
		PipelineConstantData constantData{};
		PipelineRasterState rasterState{};
		uint32_t renderTargetBlendCount = 0;
		PipelineRenderTargetBlend renderTargetBlends[8]{};
		uint32_t shaderCount = 0;
		ShaderInfo* shaderInfo = nullptr;
		uint32_t attributeCount = 0;
		VertexAttributeDesc* attributes = nullptr;
	};
#endif _DEBUG

	struct ShaderCreateInfo
	{
		//Optional for debug purposes.
		const char* optionalShaderpath = nullptr;
		Buffer buffer{};
		RENDER_SHADER_STAGE shaderStage{};
	};

	struct ExecuteCommandsInfo
	{
		CommandListHandle* commands = nullptr;
		uint32_t commandCount = 0;
		CommandQueueHandle* waitQueues = nullptr;
		uint64_t* waitValues = 0;
		uint32_t waitQueueCount = 0;
		RENDER_PIPELINE_STAGE* waitStages = nullptr;
		CommandQueueHandle* signalQueues = nullptr;
		uint32_t signalQueueCount = 0;
	};

	//This struct gets returned and has the signal values of the send queue's fences.
	struct ExecuteCommandSignalValues
	{
		uint64_t* signalValues = nullptr;
	};

	struct Vertex
	{
		float3 pos{};
		float3 normal{};
		float2 uv{};
		float3 color{};
	};

	struct BackendInfo
	{
		uint32_t framebufferCount = 0;
		FrameIndex currentFrame = 0;

		uint32_t minReadonlyConstantOffset;
		uint32_t minReadonlyBufferOffset;
		uint32_t minReadWriteBufferOffset;
	};

	//construction
	typedef BackendInfo				(*PFN_RenderAPICreateBackend)(const RenderBackendCreateInfo& a_CreateInfo);
	typedef RDescriptorHeap			(*PFN_RenderAPICreateDescriptorHeap)(const DescriptorHeapCreateInfo& a_CreateInfo, const bool a_GpuVisible);
	typedef RDescriptor				(*PFN_RenderAPICreateDescriptor)(const RenderDescriptorCreateInfo& a_Info);
	typedef CommandQueueHandle		(*PFN_RenderAPICreateCommandQueue)(const RenderCommandQueueCreateInfo& a_Info);
	typedef CommandAllocatorHandle	(*PFN_RenderAPICreateCommandAllocator)(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
	typedef CommandListHandle		(*PFN_RenderAPICreateCommandList)(const RenderCommandListCreateInfo& a_CreateInfo);
	typedef RBufferHandle			(*PFN_RenderAPICreateBuffer)(const RenderBufferCreateInfo& a_Info);
	typedef RImageHandle			(*PFN_RenderAPICreateImage)(const RenderImageCreateInfo& a_CreateInfo);
	typedef RSamplerHandle			(*PFN_RenderAPICreateSampler)(const SamplerCreateInfo& a_Info);
	typedef RFenceHandle			(*PFN_RenderAPICreateFence)(const FenceCreateInfo& a_Info);
	
	typedef DescriptorAllocation	(*PFN_RenderAPIAllocateDescriptor)(const AllocateDescriptorInfo& a_AllocateInfo);
	typedef void					(*PFN_RenderAPICopyDescriptors)(const CopyDescriptorsInfo& a_CopyInfo);
	typedef void					(*PFN_RenderAPIWriteDescriptors)(const WriteDescriptorInfos& a_WriteInfo);
	typedef ImageReturnInfo			(*PFN_RenderAPIGetImageInfo)(RImageHandle a_Handle);

	//PipelineBuilder
	typedef PipelineBuilderHandle	(*PFN_RenderAPIPipelineBuilderInit)(const PipelineInitInfo& a_InitInfo);
	typedef void					(*PFN_RenderAPIDX12PipelineBuilderBindDescriptor)(const PipelineBuilderHandle a_Handle, const RDescriptor a_Descriptor);
	typedef void					(*PFN_RenderAPIPipelineBuilderBindShaders)(const PipelineBuilderHandle a_Handle, const Slice<BB::ShaderCreateInfo> a_ShaderInfo);
	typedef void					(*PFN_RenderAPIPipelineBuilderBindAttributes)(const PipelineBuilderHandle a_Handle, const PipelineAttributes& a_AttributeInfo);
	typedef PipelineHandle			(*PFN_RenderAPIBuildPipeline)(const PipelineBuilderHandle a_Handle);

	//Commandlist handling
	typedef void (*PFN_RenderAPIResetCommandAllocator)(const CommandAllocatorHandle a_CmdAllocatorHandle);
	typedef RecordingCommandListHandle(*PFN_RenderAPIStartCommandList)(const CommandListHandle a_CmdHandle);
	typedef void (*PFN_RenderAPIEndCommandList)(const RecordingCommandListHandle a_CmdHandle);
	typedef void (*PFN_RenderAPIStartRendering)(const RecordingCommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_StartInfo);
	typedef void (*PFN_RenderAPISetScissor)(const RecordingCommandListHandle a_RecordingCmdHandle, const ScissorInfo& a_ScissorInfo);
	typedef void (*PFN_RenderAPIEndRendering)(const RecordingCommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo);
	
	typedef void (*PFN_RenderAPICopyBuffer)(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferInfo& a_CopyInfo);
	typedef void (*PFN_RenderAPICopyBufferImage)(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferImageInfo& a_CopyInfo);
	typedef void (*PFN_RenderAPITransitionImage)(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderTransitionImageInfo& a_TransitionInfo);

	typedef void (*PFN_RenderAPIBindDescriptorHeaps)(const RecordingCommandListHandle a_RecordingCmdHandle, const RDescriptorHeap a_ResourceHeap, const RDescriptorHeap a_SamplerHeap);
	typedef void (*PFN_RenderAPIBindPipeline)(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
	typedef void (*PFN_RenderAPISetDescriptorHeapOffsets)(const RecordingCommandListHandle a_RecordingCmdHandle, const RENDER_DESCRIPTOR_SET a_FirstSet, const uint32_t a_SetCount, const uint32_t* a_HeapIndex, const size_t* a_Offsets);
	typedef void (*PFN_RenderAPIBindVertexBuffers)(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
	typedef void (*PFN_RenderAPIBindIndexBuffer)(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
	typedef void (*PFN_REnderAPIBindConstant)(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_DwordOffset, const void* a_Data);

	typedef void (*PFN_RenderAPIDrawVertex)(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
	typedef void (*PFN_RenderAPIDrawIndex)(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);

	typedef void (*PFN_RenderAPIBuffer_CopyData)(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_View, const uint64_t a_Offset);
	typedef void* (*PFN_RenderAPIMapMemory)(const RBufferHandle a_Handle);
	typedef void (*PFN_RenderAPIUnmapMemory)(const RBufferHandle a_Handle);

	typedef void (*PFN_RenderAPIResizeWindow)(const uint32_t a_X, const uint32_t a_Y);
	
	typedef void (*PFN_RenderAPIStartFrame)(const StartFrameInfo& a_StartInfo);
	typedef void (*PFN_RenderAPIExecuteCommands)(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount);
	typedef void (*PFN_RenderAPIExecutePresentCommands)(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo);
	typedef FrameIndex(*PFN_RenderAPIPresentFrame)(const PresentFrameInfo& a_PresentInfo);

	typedef uint64_t (*PFN_RenderAPINextQueueFenceValue)(const CommandQueueHandle a_Handle);
	typedef uint64_t (*PFN_RenderAPINextFenceValue)(const RFenceHandle a_Handle);

	typedef void (*PFN_RenderAPIWaitCommands)(const RenderWaitCommandsInfo& a_WaitInfo);

	//Deletion
	typedef void (*PFN_RenderAPIDestroyBackend)();
	typedef void (*PFN_RenderAPIDestroyDescriptor)(const RDescriptor a_Handle);
	typedef void (*PFN_RenderAPIDestroyDescriptorHeap)(const RDescriptorHeap a_Handle);
	typedef void (*PFN_RenderAPIDestroyPipeline)(const PipelineHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandQueue)(const CommandQueueHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandAllocator)(const CommandAllocatorHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandList)(const CommandListHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyBuffer)(const RBufferHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyImage)(const RImageHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroySampler)(const RSamplerHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyFence)(const RFenceHandle a_Handle);

	struct RenderAPIFunctions
	{
		PFN_RenderAPICreateBackend createBackend;
		PFN_RenderAPICreateDescriptorHeap createDescriptorHeap;
		PFN_RenderAPICreateDescriptor createDescriptor;
		PFN_RenderAPICreateCommandQueue createCommandQueue;
		PFN_RenderAPICreateCommandAllocator createCommandAllocator;
		PFN_RenderAPICreateCommandList createCommandList;
		PFN_RenderAPICreateBuffer createBuffer;
		PFN_RenderAPICreateImage createImage;
		PFN_RenderAPICreateSampler createSampler;
		PFN_RenderAPICreateFence createFence;

		PFN_RenderAPIAllocateDescriptor allocateDescriptor;
		PFN_RenderAPICopyDescriptors copyDescriptors;
		PFN_RenderAPIWriteDescriptors writeDescriptors;
		PFN_RenderAPIGetImageInfo getImageInfo;

		PFN_RenderAPIPipelineBuilderInit pipelineBuilderInit;
		PFN_RenderAPIDX12PipelineBuilderBindDescriptor pipelineBuilderBindDescriptor;
		PFN_RenderAPIPipelineBuilderBindShaders pipelineBuilderBindShaders;
		PFN_RenderAPIPipelineBuilderBindAttributes pipelineBuilderBindAttributes;
		PFN_RenderAPIBuildPipeline pipelineBuilderBuildPipeline;

		PFN_RenderAPIStartCommandList startCommandList;
		PFN_RenderAPIResetCommandAllocator resetCommandAllocator;
		PFN_RenderAPIEndCommandList endCommandList;
		PFN_RenderAPIStartRendering startRendering;
		PFN_RenderAPISetScissor setScissor;
		PFN_RenderAPIEndRendering endRendering;

		PFN_RenderAPICopyBuffer copyBuffer;
		PFN_RenderAPICopyBufferImage copyBufferImage;
		PFN_RenderAPITransitionImage transitionImage;

		PFN_RenderAPIBindDescriptorHeaps bindDescriptorHeaps;
		PFN_RenderAPIBindPipeline bindPipeline;
		PFN_RenderAPISetDescriptorHeapOffsets setDescriptorHeapOffsets;
		PFN_RenderAPIBindVertexBuffers bindVertBuffers;
		PFN_RenderAPIBindIndexBuffer bindIndexBuffer;
		PFN_REnderAPIBindConstant bindConstant;

		PFN_RenderAPIDrawVertex drawVertex;
		PFN_RenderAPIDrawIndex drawIndex;

		PFN_RenderAPIBuffer_CopyData bufferCopyData;
		PFN_RenderAPIMapMemory mapMemory;
		PFN_RenderAPIUnmapMemory unmapMemory;

		PFN_RenderAPIResizeWindow resizeWindow;

		PFN_RenderAPIStartFrame startFrame;
		PFN_RenderAPIExecuteCommands executeCommands;
		PFN_RenderAPIExecutePresentCommands executePresentCommands;
		PFN_RenderAPIPresentFrame presentFrame;

		PFN_RenderAPINextQueueFenceValue nextQueueFenceValue;
		PFN_RenderAPINextFenceValue nextFenceValue;

		PFN_RenderAPIWaitCommands waitCommands;

		PFN_RenderAPIDestroyBackend destroyBackend;
		PFN_RenderAPIDestroyDescriptor destroyDescriptor;
		PFN_RenderAPIDestroyDescriptorHeap destroyDescriptorHeap;
		PFN_RenderAPIDestroyPipeline destroyPipeline;
		PFN_RenderAPIDestroyCommandQueue destroyCommandQueue;
		PFN_RenderAPIDestroyCommandAllocator destroyCommandAllocator;
		PFN_RenderAPIDestroyCommandList destroyCommandList;
		PFN_RenderAPIDestroyBuffer destroyBuffer;
		PFN_RenderAPIDestroyImage destroyImage;
		PFN_RenderAPIDestroySampler destroySampler;
		PFN_RenderAPIDestroyFence destroyFence;
	};
}
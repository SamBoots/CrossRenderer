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

	using FrameIndex = uint32_t;
	
	//Index is the start index, Index 
	using PipelineBuilderHandle = FrameworkHandle<struct PipelineBuilderHandleTag>;
	using PipelineHandle = FrameworkHandle<struct PipelineHandleTag>;
	
	using RDescriptorHandle = FrameworkHandle<struct RDescriptorHandleTag>;
	using CommandQueueHandle = FrameworkHandle<struct CommandQueueHandleTag>;
	using CommandAllocatorHandle = FrameworkHandle<struct CommandAllocatorHandleTag>;
	using CommandListHandle = FrameworkHandle<struct CommandListHandleTag>;
	using RecordingCommandListHandle = FrameworkHandle<struct RecordingCommandListHandleTag>;

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

	enum class RENDER_DESCRIPTOR_TYPE : uint32_t
	{
		READONLY_CONSTANT, //CBV or uniform buffer
		READONLY_BUFFER, //SRV or Storage buffer
		READWRITE, //UAV or readwrite storage buffer(?)
		READONLY_CONSTANT_DYNAMIC, //Root CBV or dynamic constant buffer
		READONLY_BUFFER_DYNAMIC, //Root SRV or dynamic storage buffer
		READWRITE_DYNAMIC, //Root UAV or readwrite dynamic storage buffer(?)
		IMAGE,
		SAMPLER
	};

	enum class RENDER_DESCRIPTOR_SET : uint32_t
	{
		SCENE_SET = 0,
		PER_FRAME_SET = 1,
		PER_MESH_SET = 2,
		PER_MATERIAL_SET = 3
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

	struct UpdateDescriptorImageInfo
	{
		RDescriptorHandle set;
		uint32_t binding;
		uint32_t descriptorIndex;
		RENDER_DESCRIPTOR_TYPE type;

		RImageHandle image;
		RENDER_IMAGE_LAYOUT imageLayout;
	};

	struct UpdateDescriptorBufferInfo
	{
		RDescriptorHandle set;
		uint32_t binding;
		uint32_t descriptorIndex;
		RENDER_DESCRIPTOR_TYPE type;

		RBufferHandle buffer;
		uint32_t bufferSize;
		uint32_t bufferOffset;
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
		PFN_RenderGetAPIFunctions getApiFuncPtr;
		Slice<RENDER_EXTENSIONS> extensions{};
		Slice<RENDER_EXTENSIONS> deviceExtensions{};
		WindowHandle windowHandle;
		const char* appName{};
		const char* engineName{};
		uint32_t windowWidth{};
		uint32_t windowHeight{};
		bool validationLayers{};
	};

	struct DescriptorBinding
	{
		uint32_t binding;
		uint32_t descriptorCount;
		RENDER_DESCRIPTOR_TYPE type;
		RENDER_SHADER_STAGE stage;
		RENDER_DESCRIPTOR_FLAG flags;
	};

	struct RenderDescriptorCreateInfo
	{
		RENDER_BINDING_SET bindingSet;
		BB::Slice<DescriptorBinding> bindings;
	};

	struct RenderCommandQueueCreateInfo
	{
		RENDER_QUEUE_TYPE queue;
		RENDER_FENCE_FLAGS flags;
	};

	struct RenderCommandAllocatorCreateInfo
	{
		RENDER_QUEUE_TYPE queueType;
		uint32_t commandListCount;
	};

	struct RenderCommandListCreateInfo
	{
		CommandAllocatorHandle commandAllocator;
	};

	struct RenderBufferCreateInfo
	{
		uint64_t size = 0;
		const void* data = nullptr; //Optional, if provided it will also upload the data to the buffer if it can.
		RENDER_BUFFER_USAGE usage;
		RENDER_MEMORY_PROPERTIES memProperties;
	};

	struct RenderImageCreateInfo
	{
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0;

		uint16_t arrayLayers = 0;
		uint16_t mipLevels = 0;
		RENDER_IMAGE_TYPE type;
		RENDER_IMAGE_FORMAT format;
		RENDER_IMAGE_TILING tiling;
	};

	struct SamplerCreateInfo
	{
		SAMPLER_ADDRESS_MODE addressModeU{};
		SAMPLER_ADDRESS_MODE addressModeV{};
		SAMPLER_ADDRESS_MODE addressModeW{};

		SAMPLER_FILTER filter{};
		float maxAnistoropy = 0;

		float minLod = 0;
		float maxLod = 0;
	};

	struct RenderTransitionImageInfo
	{
		RImageHandle image;
		RENDER_IMAGE_LAYOUT oldLayout;
		RENDER_IMAGE_LAYOUT newLayout;
		RENDER_PIPELINE_STAGE srcStage;
		RENDER_PIPELINE_STAGE dstStage;
		RENDER_ACCESS_MASK srcMask;
		RENDER_ACCESS_MASK dstMask;

		uint32_t baseMipLevel;
		uint32_t levelCount;
		uint32_t baseArrayLayer;
		uint32_t layerCount;
	};

	struct FenceCreateInfo
	{
		RENDER_FENCE_FLAGS flags;
	};

	struct StartFrameInfo
	{
		RFenceHandle imageWait;
		RFenceHandle* fences;
		uint32_t fenceCount;
	};

	struct ImageReturnInfo
	{
		struct AllocInfo
		{
			uint64_t imageAllocByteSize;
			uint32_t footRowPitch;
			uint32_t footHeight;
		} allocInfo;

		uint32_t width;
		uint32_t height;
		uint32_t depth;
		uint16_t mips;
		uint16_t arrayLayers;
	};

	struct RenderCopyBufferInfo
	{
		RBufferHandle src;
		RBufferHandle dst;

		uint64_t srcOffset;
		uint64_t dstOffset;
		uint64_t size;
	};

	struct ImageCopyInfo
	{
		uint32_t sizeX;
		uint32_t sizeY;
		uint32_t sizeZ;

		int32_t offsetX;
		int32_t offsetY;
		int32_t offsetZ;

		uint16_t mipLevel;
		uint16_t baseArrayLayer;
		uint16_t layerCount;

		RENDER_IMAGE_LAYOUT layout;
	};

	struct RenderCopyBufferImageInfo
	{
		RBufferHandle srcBuffer;
		uint32_t srcBufferOffset;

		RImageHandle dstImage;
		ImageCopyInfo dstImageInfo;
	};

	struct StartRenderingInfo
	{
		uint32_t viewportWidth = 0;
		uint32_t viewportHeight = 0;

		RENDER_LOAD_OP colorLoadOp{};
		RENDER_STORE_OP colorStoreOp{};
		RENDER_IMAGE_LAYOUT colorInitialLayout{};
		RENDER_IMAGE_LAYOUT colorFinalLayout{};

		RImageHandle depthStencil;

		//RGBA
		float clearColor[4]{};
	};

	struct ScissorInfo
	{
		int2 offset;
		uint2 extent;
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
		RENDER_BLEND_FACTOR srcBlend;
		RENDER_BLEND_FACTOR dstBlend;
		RENDER_BLEND_OP blendOp;
		RENDER_BLEND_FACTOR srcBlendAlpha;
		RENDER_BLEND_FACTOR dstBlendAlpha;
		RENDER_BLEND_OP blendOpAlpha;
		//uint8_t renderTargetWriteMask;
	};

	struct PipelineRasterState
	{
		bool frontCounterClockwise = false;
		RENDER_CULL_MODE cullMode;
	};

	struct PipelineConstantData
	{
		//0 means this pipeline does not use push constants/root constants.
		uint32_t dwordSize;
		RENDER_SHADER_STAGE shaderStage;
	};

	struct PipelineInitInfo
	{
		PipelineRasterState rasterizerState{};
		PipelineConstantData constantData{};

		bool blendLogicOpEnable = false;
		bool enableDepthTest = false;
		RENDER_LOGIC_OP blendLogicOp;
		uint32_t renderTargetBlendCount = 0;
		PipelineRenderTargetBlend* renderTargetBlends = nullptr;
	};

	struct VertexAttributeDesc
	{
		uint32_t location = 0;
		RENDER_INPUT_FORMAT format;
		uint32_t offset = 0;
		char* semanticName = nullptr;
	};

	struct PipelineAttributes
	{
		uint32_t stride = 0;
		//Maybe add input rate.
		BB::Slice<VertexAttributeDesc> attributes;
	};

	struct ShaderCreateInfo
	{
		Buffer buffer{};
		RENDER_SHADER_STAGE shaderStage{};
	};

	struct ExecuteCommandsInfo
	{
		CommandListHandle* commands;
		uint32_t commandCount;
		CommandQueueHandle* waitQueues;
		uint64_t* waitValues;
		uint32_t waitQueueCount;
		RENDER_PIPELINE_STAGE* waitStages;
		CommandQueueHandle* signalQueues;
		uint32_t signalQueueCount;
	};

	//This struct gets returned and has the signal values of the send queue's fences.
	struct ExecuteCommandSignalValues
	{
		uint64_t* signalValues;
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
	};

	//construction
	typedef BackendInfo				(*PFN_RenderAPICreateBackend)(const RenderBackendCreateInfo& a_CreateInfo);
	typedef RDescriptorHandle		(*PFN_RenderAPICreateDescriptor)(const RenderDescriptorCreateInfo& a_Info);
	typedef CommandQueueHandle		(*PFN_RenderAPICreateCommandQueue)(const RenderCommandQueueCreateInfo& a_Info);
	typedef CommandAllocatorHandle	(*PFN_RenderAPICreateCommandAllocator)(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
	typedef CommandListHandle		(*PFN_RenderAPICreateCommandList)(const RenderCommandListCreateInfo& a_CreateInfo);
	typedef RBufferHandle			(*PFN_RenderAPICreateBuffer)(const RenderBufferCreateInfo& a_Info);
	typedef RImageHandle			(*PFN_RenderAPICreateImage)(const RenderImageCreateInfo& a_CreateInfo);
	typedef RFenceHandle			(*PFN_RenderAPICreateFence)(const FenceCreateInfo& a_Info);

	typedef void (*PFN_RenderAPIUpdateDescriptorBuffer)(const UpdateDescriptorBufferInfo& a_Info);
	typedef void (*PFN_RenderAPIUpdateDescriptorImage)(const UpdateDescriptorImageInfo& a_Info);

	typedef ImageReturnInfo		(*PFN_RenderAPIGetImageInfo)(RImageHandle a_Handle);

	//PipelineBuilder
	typedef PipelineBuilderHandle	(*PFN_RenderAPIPipelineBuilderInit)(const PipelineInitInfo& a_InitInfo);
	typedef void					(*PFN_RenderAPIDX12PipelineBuilderBindDescriptor)(const PipelineBuilderHandle a_Handle, const RDescriptorHandle a_Descriptor);
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

	typedef void (*PFN_RenderAPIBindPipeline)(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
	typedef void (*PFN_RenderAPIBindVertexBuffers)(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
	typedef void (*PFN_RenderAPIBindIndexBuffer)(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
	typedef void (*PFN_RenderAPIBindDescriptors)(const RecordingCommandListHandle a_RecordingCmdHandle, const RDescriptorHandle* a_Sets, const uint32_t a_SetCount, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets);
	typedef void (*PFN_REnderAPIBindConstant)(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_Offset, const void* a_Data);

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

	typedef void (*PFN_RenderAPIWaitDeviceReady)();

	//Deletion
	typedef void (*PFN_RenderAPIDestroyBackend)();
	typedef void (*PFN_RenderAPIDestroyDescriptor)(const RDescriptorHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyPipeline)(const PipelineHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandQueue)(const CommandQueueHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandAllocator)(const CommandAllocatorHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandList)(const CommandListHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyBuffer)(const RBufferHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyImage)(const RImageHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyFence)(const RFenceHandle a_Handle);

	struct RenderAPIFunctions
	{
		PFN_RenderAPICreateBackend createBackend;
		PFN_RenderAPICreateDescriptor createDescriptor;
		PFN_RenderAPICreateCommandQueue createCommandQueue;
		PFN_RenderAPICreateCommandAllocator createCommandAllocator;
		PFN_RenderAPICreateCommandList createCommandList;
		PFN_RenderAPICreateBuffer createBuffer;
		PFN_RenderAPICreateImage createImage;
		PFN_RenderAPICreateFence createFence;

		PFN_RenderAPIUpdateDescriptorBuffer updateDescriptorBuffer;
		PFN_RenderAPIUpdateDescriptorImage updateDescriptorImage;
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

		PFN_RenderAPIBindPipeline bindPipeline;
		PFN_RenderAPIBindVertexBuffers bindVertBuffers;
		PFN_RenderAPIBindIndexBuffer bindIndexBuffer;
		PFN_RenderAPIBindDescriptors bindDescriptors;
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

		PFN_RenderAPIWaitDeviceReady waitDevice;

		PFN_RenderAPIDestroyBackend destroyBackend;
		PFN_RenderAPIDestroyDescriptor destroyDescriptor;
		PFN_RenderAPIDestroyPipeline destroyPipeline;
		PFN_RenderAPIDestroyCommandQueue destroyCommandQueue;
		PFN_RenderAPIDestroyCommandAllocator destroyCommandAllocator;
		PFN_RenderAPIDestroyCommandList destroyCommandList;
		PFN_RenderAPIDestroyBuffer destroyBuffer;
		PFN_RenderAPIDestroyImage destroyImage;
		PFN_RenderAPIDestroyFence destroyFence;
	};
}
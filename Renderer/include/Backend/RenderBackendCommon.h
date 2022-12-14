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
	using FrameIndex = uint32_t;
	
	//Common handles
	using FrameBufferHandle = FrameworkHandle<struct FrameBufferHandleTag>;
	using RDescriptorLayoutHandle = FrameworkHandle<struct RDescriptorHandleTag>;

	//Index is the start index, Index 
	using RDescriptorHandle = FrameworkHandle<struct RDescriptorHandleTag>;
	using PipelineHandle = FrameworkHandle<struct PipelineHandleTag>;
	using CommandQueueHandle = FrameworkHandle<struct CommandQueueHandleTag>;
	using CommandAllocatorHandle = FrameworkHandle<struct CommandAllocatorHandleTag>;
	using CommandListHandle = FrameworkHandle<struct CommandListHandleTag>;
	using RecordingCommandListHandle = FrameworkHandle<struct RecordingCommandListHandleTag>;

	using RFenceHandle = FrameworkHandle<struct RFenceHandleTag>;
	using RBufferHandle = FrameworkHandle<struct RBufferHandleTag>;
	using RImageHandle = FrameworkHandle<struct RImageHandleTag>;
	using RShaderHandle = FrameworkHandle<struct RShaderHandleTag>;

	enum class RenderAPI
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

	enum class DESCRIPTOR_BUFFER_TYPE : uint32_t
	{
		UNIFORM_BUFFER,
		STORAGE_BUFFER,
		UNIFORM_BUFFER_DYNAMIC,
		STORAGE_BUFFER_DYNAMIC,
		INPUT_ATTACHMENT
	};

	enum class DESCRIPTOR_IMAGE_TYPE : uint32_t
	{
		SAMPLER,
		COMBINED_IMAGE_SAMPLER,
		SAMPLED_IMAGE,
		STORAGE_IMAGE,
		UNIFORM_TEXEL_BUFFER,
		STORAGE_TEXEL_BUFFER
	};

	enum class RENDER_DESCRIPTOR_SET : uint32_t
	{
		SCENE_SET = 0,
		PER_FRAME_SET = 1,
		PER_MESH_SET = 2,
		PER_MATERIAL_SET = 3
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

	enum class RENDER_IMAGE_USAGE : uint32_t
	{
		SAMPLER
	};

	enum class RENDER_IMAGE_FORMAT : uint32_t
	{
		DEPTH_STENCIL,
		SRGB
	};

	enum class RENDER_IMAGE_VIEWTYPE : uint32_t
	{
		TYPE_2D,
		TYPE_2D_ARRAY
	};

	enum class RENDER_SHADER_STAGE : uint32_t
	{
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

	enum class RENDER_IMAGE_LAYOUT : uint32_t
	{
		UNDEFINED,
		GENERAL,
		TRANSFER_SRC,
		TRANSFER_DST,
		PRESENT
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

	struct RenderBufferCreateInfo
	{
		uint64_t size = 0;
		const void* data = nullptr; //Optional, if provided it will also upload the data to the buffer if it can.
		RENDER_BUFFER_USAGE usage;
		RENDER_MEMORY_PROPERTIES memProperties;

	};

	struct FenceCreateInfo
	{
		RENDER_FENCE_FLAGS flags;
	};

	struct RenderCopyBufferInfo
	{
		RecordingCommandListHandle transferCommandHandle;
		RBufferHandle src;
		RBufferHandle dst;

		struct CopyRegions
		{
			uint64_t srcOffset;
			uint64_t dstOffset;
			uint64_t size;
		};
		CopyRegions* copyRegions;
		uint64_t CopyRegionCount;
	};

	struct RDeviceBufferView
	{
		uint32_t size{};
		uint32_t offset{};
	};

	struct RenderImageCreateInfo
	{
		// The width in texels.
		uint32_t width = 0;
		// The height in texels.
		uint32_t height = 0;

		uint32_t arrayLayers = 0;
		RENDER_IMAGE_TYPE type;
		RENDER_IMAGE_USAGE usage;
		// The format of the image's texels.
		RENDER_IMAGE_FORMAT format;

		RENDER_IMAGE_VIEWTYPE viewtype;
	};

	struct ShaderCreateInfo
	{
		Buffer buffer{};
		RENDER_SHADER_STAGE shaderStage{};
	};

	struct RenderAPIFunctions;
	typedef void (*PFN_RenderGetAPIFunctions)(RenderAPIFunctions&);

	struct RenderBackendCreateInfo
	{
		PFN_RenderGetAPIFunctions getApiFuncPtr;
		Slice<RENDER_EXTENSIONS> extensions{};
		Slice<RENDER_EXTENSIONS> deviceExtensions{};
#ifdef _WIN32
		HWND hwnd{};
#endif //_WIN32
		const char* appName{};
		const char* engineName{};
		uint32_t windowWidth{};
		uint32_t windowHeight{};
		int version{};
		bool validationLayers{};
	};

	struct RenderDescriptorCreateInfo
	{
		struct ConstantBind
		{
			uint32_t offset;
			uint32_t size;
			RENDER_SHADER_STAGE stage;
		};

		struct BufferBind
		{
			RBufferHandle buffer;
			uint64_t bufferSize;
			uint64_t bufferOffset;
			uint32_t binding;
			DESCRIPTOR_BUFFER_TYPE type;
			RENDER_SHADER_STAGE stage;
		};

		struct ImageBind
		{
			uint32_t binding;
			DESCRIPTOR_IMAGE_TYPE type;
			RENDER_SHADER_STAGE stage;
		};

		BB::Slice<ConstantBind> constantBinds;
		BB::Slice<BufferBind> bufferBinds;
		BB::Slice<ImageBind> ImageBinds;
	};

	struct ConstantBufferInfo
	{
		uint32_t size;
		uint32_t offset;
		RENDER_SHADER_STAGE stage;
	};

	struct RenderPipelineCreateInfo
	{
		FrameBufferHandle framebufferHandle{};
		//Required for Vulkan
		Slice<BB::ShaderCreateInfo> shaderCreateInfos{};
		//Use the layouts to a maximum of RENDER_DESCRIPTOR_BINDING
		RDescriptorLayoutHandle* descLayoutHandles;
		uint32_t descLayoutCount;
		ConstantBufferInfo* constantBuffers;
		uint32_t constantBufferCount;
	};

	struct RenderFrameBufferCreateInfo
	{
		RENDER_LOAD_OP colorLoadOp{};
		RENDER_STORE_OP colorStoreOp{};
		RENDER_IMAGE_LAYOUT colorInitialLayout{};
		RENDER_IMAGE_LAYOUT colorFinalLayout{};
		uint32_t width{};
		uint32_t height{};
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

	struct ExecuteCommandsInfo
	{
		CommandListHandle* commands;
		uint32_t commandCount;
		CommandQueueHandle* waitQueues;
		uint64_t* waitValues;
		uint32_t waitQueueCount;
		CommandQueueHandle* signalQueues;
		uint32_t signalQueueCount;
	};

	//This struct gets returned and has the signal values of the send queue's fences.
	struct ExecuteCommandSignalValues
	{
		uint64_t* signalValues;
	};

	struct StartFrameInfo
	{
		RFenceHandle imageWait;
		RFenceHandle* fences;
		uint32_t fenceCount;
	};

	struct PresentFrameInfo
	{

	};

	struct Vertex
	{
		float pos[2]{};
		float color[3]{};
	};

	struct BackendInfo
	{
		uint32_t framebufferCount;
		FrameIndex currentFrame = 0;
	};

	//construction
	typedef BackendInfo			(*PFN_RenderAPICreateBackend)(Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo);
	typedef RDescriptorHandle	(*PFN_RenderAPICreateDescriptor)(Allocator a_TempAllocator, RDescriptorLayoutHandle& a_Layout, const RenderDescriptorCreateInfo& a_CreateInfo);
	typedef PipelineHandle		(*PFN_RenderAPICreatePipeline)(Allocator a_TempAllocator, const RenderPipelineCreateInfo& a_CreateInfo);
	typedef FrameBufferHandle	(*PFN_RenderAPICreateFrameBuffer)(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo);
	typedef CommandQueueHandle(*PFN_RenderAPICreateCommandQueue)(const RenderCommandQueueCreateInfo& a_Info);
	typedef CommandAllocatorHandle(*PFN_RenderAPICreateCommandAllocator)(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
	typedef CommandListHandle	(*PFN_RenderAPICreateCommandList)(Allocator a_TempAllocator, const RenderCommandListCreateInfo& a_CreateInfo);
	typedef RBufferHandle		(*PFN_RenderAPICreateBuffer)(const RenderBufferCreateInfo& a_Info);
	typedef RFenceHandle		(*PFN_RenderAPICreateFence)(const FenceCreateInfo& a_Info);

	typedef void (*PFN_RenderAPIResetCommandAllocator)(const CommandAllocatorHandle a_CmdAllocatorHandle);

	//Commandlist handling
	typedef RecordingCommandListHandle(*PFN_RenderAPIStartCommandList)(const CommandListHandle a_CmdHandle);
	typedef void (*PFN_RenderAPIEndCommandList)(const RecordingCommandListHandle a_CmdHandle);
	typedef void (*PFN_RenderAPIStartRenderPass)(const RecordingCommandListHandle a_RecordingCmdHandle, const FrameBufferHandle a_Framebuffer);
	typedef void (*PFN_RenderAPIEndRenderPass)(const RecordingCommandListHandle a_RecordingCmdHandle);
	typedef void (*PFN_RenderAPIBindPipeline)(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
	typedef void (*PFN_RenderAPIBindVertexBuffers)(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
	typedef void (*PFN_RenderAPIBindIndexBuffer)(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
	typedef void (*PFN_RenderAPIBindDescriptors)(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_FirstSet, const uint32_t a_SetCount, const RDescriptorHandle* a_Sets, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets);
	typedef void (*PFN_REnderAPIBindConstant)(const RecordingCommandListHandle a_RecordingCmdHandle, const RENDER_SHADER_STAGE a_Stage , const uint32_t a_Offset, const uint32_t a_Size, const void* a_Data);

	typedef void (*PFN_RenderAPIDrawVertex)(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
	typedef void (*PFN_RenderAPIDrawIndex)(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);

	//Utility
	typedef void (*PFN_RenderAPIBuffer_CopyData)(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_View, const uint64_t a_Offset);
	typedef void (*PFN_RenderAPICopyBuffer)(Allocator a_TempAllocator, const RenderCopyBufferInfo& a_CopyInfo);
	typedef void* (*PFN_RenderAPIMapMemory)(const RBufferHandle a_Handle);
	typedef void (*PFN_RenderAPIUnmapMemory)(const RBufferHandle a_Handle);

	typedef void (*PFN_RenderAPIResizeWindow)(Allocator a_TempAllocator, const uint32_t a_X, const uint32_t a_Y);
	
	typedef void (*PFN_RenderAPIStartFrame)(Allocator a_TempAllocator, const StartFrameInfo& a_StartInfo);
	typedef void (*PFN_RenderAPIExecuteCommands)(Allocator a_TempAllocator, CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount);
	typedef void (*PFN_RenderAPIExecutePresentCommands)(Allocator a_TempAllocator, CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo);
	typedef FrameIndex(*PFN_RenderAPIPresentFrame)(Allocator a_TempAllocator, const PresentFrameInfo& a_PresentInfo);

	typedef uint64_t (*PFN_RenderAPINextQueueFenceValue)(const CommandQueueHandle a_Handle);
	typedef uint64_t (*PFN_RenderAPINextFenceValue)(const RFenceHandle a_Handle);

	typedef void (*PFN_RenderAPIWaitDeviceReady)();

	//Deletion
	typedef void (*PFN_RenderAPIDestroyBackend)();
	typedef void (*PFN_RenderAPIDestroyDescriptorLayout)(const RDescriptorLayoutHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyDescriptor)(const RDescriptorHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyFrameBuffer)(const FrameBufferHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyPipeline)(const PipelineHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandQueue)(const CommandQueueHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandAllocator)(const CommandAllocatorHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandList)(const CommandListHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyBuffer)(const RBufferHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyFence)(const RFenceHandle a_Handle);

	struct RenderAPIFunctions
	{
		PFN_RenderAPICreateBackend createBackend;
		PFN_RenderAPICreateDescriptor createDescriptor;
		PFN_RenderAPICreatePipeline createPipeline;
		PFN_RenderAPICreateFrameBuffer createFrameBuffer;
		PFN_RenderAPICreateCommandQueue createCommandQueue;
		PFN_RenderAPICreateCommandAllocator createCommandAllocator;
		PFN_RenderAPICreateCommandList createCommandList;
		PFN_RenderAPICreateBuffer createBuffer;

		PFN_RenderAPICreateFence createFence;

		PFN_RenderAPIStartCommandList startCommandList;
		PFN_RenderAPIResetCommandAllocator resetCommandAllocator;
		PFN_RenderAPIEndCommandList endCommandList;
		PFN_RenderAPIStartRenderPass startRenderPass;
		PFN_RenderAPIEndRenderPass endRenderPass;
		PFN_RenderAPIBindPipeline bindPipeline;
		PFN_RenderAPIBindVertexBuffers bindVertBuffers;
		PFN_RenderAPIBindIndexBuffer bindIndexBuffer;
		PFN_RenderAPIBindDescriptors bindDescriptor;
		PFN_REnderAPIBindConstant bindConstant;

		PFN_RenderAPIDrawVertex drawVertex;
		PFN_RenderAPIDrawIndex drawIndex;

		PFN_RenderAPIBuffer_CopyData bufferCopyData;
		PFN_RenderAPICopyBuffer copyBuffer;
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
		PFN_RenderAPIDestroyDescriptorLayout destroyDescriptorLayout;
		PFN_RenderAPIDestroyFrameBuffer destroyFrameBuffer;
		PFN_RenderAPIDestroyPipeline destroyPipeline;
		PFN_RenderAPIDestroyCommandQueue destroyCommandQueue;
		PFN_RenderAPIDestroyCommandAllocator destroyCommandAllocator;
		PFN_RenderAPIDestroyCommandList destroyCommandList;
		PFN_RenderAPIDestroyBuffer destroyBuffer;
		PFN_RenderAPIDestroyFence destroyFence;
	};
}
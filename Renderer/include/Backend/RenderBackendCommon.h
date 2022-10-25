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
	using APIRenderBackend = FrameworkHandle<struct APIRenderBackendTag>;
	//Common handles
	using FrameBufferHandle = FrameworkHandle<struct FrameBufferHandleTag>;
	using PipelineHandle = FrameworkHandle<struct PipelineHandleTag>;
	using CommandListHandle = FrameworkHandle<struct CommandListHandleTag>;
	using RecordingCommandListHandle = FrameworkHandle<struct RecordingCommandListHandleTag>;

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
		FRAGMENT
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
		PHYSICAL_DEVICE_EXTRA_PROPERTIES, 
		PIPELINE_EXTENDED_DYNAMIC_STATE //VK Device Property.
	};

	enum class RENDER_QUEUE_TYPE : uint32_t
	{
		GRAPHICS,
		TRANSFER
	};

	struct RenderBufferCreateInfo
	{
		uint64_t size = 0;
		void* data = nullptr; //Optional, if provided it will also upload the data to the buffer if it can.
		RENDER_BUFFER_USAGE usage;
		RENDER_MEMORY_PROPERTIES memProperties;
	};

	struct RenderCopyBufferInfo
	{
		CommandListHandle transferCommandHandle;
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
		uint64_t size{};
		uint64_t offset{};
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

	struct RenderPipelineCreateInfo
	{
		FrameBufferHandle framebufferHandle{};
		//Required for Vulkan
		Slice<BB::ShaderCreateInfo> shaderCreateInfos{};

		//Required for DX12
		const wchar_t** shaderPaths;
		size_t shaderPathCount;
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

	struct RenderCommandListCreateInfo
	{
		RENDER_QUEUE_TYPE queueType;
		uint32_t bufferCount;
	};

	struct Vertex
	{
		float pos[2]{};
		float color[3]{};
	};

	//construction
	typedef APIRenderBackend(*PFN_RenderAPICreateBackend)(Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo);
	typedef PipelineHandle(*PFN_RenderAPICreatePipeline)(Allocator a_TempAllocator, const RenderPipelineCreateInfo& a_CreateInfo);
	typedef FrameBufferHandle(*PFN_RenderAPICreateFrameBuffer)(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo);
	typedef CommandListHandle(*PFN_RenderAPICreateCommandList)(Allocator a_TempAllocator, const RenderCommandListCreateInfo& a_CreateInfo);
	typedef RBufferHandle(*PFN_RenderAPICreateBuffer)(const RenderBufferCreateInfo& a_Info);

	//Commandlist handling
	typedef RecordingCommandListHandle(*PFN_RenderAPIStartCommandList)(const CommandListHandle a_CmdHandle, const FrameBufferHandle a_Framebuffer);
	typedef void (*PFN_RenderAPIEndCommandList)(const RecordingCommandListHandle a_CmdHandle);
	typedef void (*PFN_RenderAPIBindPipeline)(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
	typedef void (*PFN_RenderAPIDrawBuffers)(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_BufferHandles, const size_t a_BufferCount);

	//Utility
	typedef void (*PFN_RenderAPIBuffer_CopyData)(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_View, const uint64_t a_Offset);
	typedef void (*PFN_RenderAPICCopyBuffer)(Allocator a_TempAllocator, const RenderCopyBufferInfo& a_CopyInfo);

	typedef void (*PFN_RenderAPIResizeWindow)(Allocator a_TempAllocator, const uint32_t a_X, const uint32_t a_Y);
	
	typedef void (*PFN_RenderAPIStartFrame)();
	typedef void (*PFN_RenderAPIRenderFrame)(Allocator a_TempAllocator, const CommandListHandle a_CommandHandle, const FrameBufferHandle a_FrameBufferHandle, const PipelineHandle a_PipeHandle);
	typedef void (*PFN_RenderAPIWaitDeviceReady)();

	//Deletion
	typedef void (*PFN_RenderAPIDestroyBackend)();
	typedef void (*PFN_RenderAPIDestroyFrameBuffer)(const FrameBufferHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyPipeline)(const PipelineHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandList)(const CommandListHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyBuffer)(const RBufferHandle a_Handle);

	struct RenderAPIFunctions
	{
		PFN_RenderAPICreateBackend createBackend;
		PFN_RenderAPICreatePipeline createPipeline;
		PFN_RenderAPICreateFrameBuffer createFrameBuffer;
		PFN_RenderAPICreateCommandList createCommandList;
		PFN_RenderAPICreateBuffer createBuffer;

		PFN_RenderAPIStartCommandList startCommandList;
		PFN_RenderAPIEndCommandList endCommandList;
		PFN_RenderAPIBindPipeline bindPipeline;
		PFN_RenderAPIDrawBuffers drawBuffers;

		PFN_RenderAPIBuffer_CopyData bufferCopyData;
		PFN_RenderAPICCopyBuffer copyBuffer;

		PFN_RenderAPIResizeWindow resizeWindow;

		PFN_RenderAPIStartFrame startFrame;
		PFN_RenderAPIRenderFrame renderFrame;
		PFN_RenderAPIWaitDeviceReady waitDevice;

		PFN_RenderAPIDestroyBuffer destroyBuffer;
		PFN_RenderAPIDestroyBackend destroyBackend;
		PFN_RenderAPIDestroyFrameBuffer destroyFrameBuffer;
		PFN_RenderAPIDestroyPipeline destroyPipeline;
		PFN_RenderAPIDestroyCommandList destroyCommandList;
	};
}
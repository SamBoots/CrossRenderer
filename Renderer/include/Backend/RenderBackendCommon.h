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

	using GBufferHandle = FrameworkHandle<struct GBufferHandleTag>;
	using GImageHandle = FrameworkHandle<struct GImageHandleTag>;
	using GShaderHandle = FrameworkHandle<struct GShaderHandleTag>;

	enum class RENDER_BUFFER_USAGE : uint32_t
	{
		VERTEX,
		INDEX,
		UNIFORM,
		STORAGE,
		STAGING
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

	struct RenderBufferCreateInfo
	{
		uint64_t size = 0;
		RENDER_BUFFER_USAGE usage;
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
		Buffer buffer;
		RENDER_SHADER_STAGE shaderStage;
	};

	inline RenderBufferCreateInfo CreateRenderBufferInfo(uint64_t a_Size, RENDER_BUFFER_USAGE a_Usage)
	{
		RenderBufferCreateInfo t_Info;
		t_Info.size = a_Size;
		t_Info.usage = a_Usage;
		return t_Info;
	}

	inline RenderImageCreateInfo CreateRenderImageInfo(uint32_t a_Width, uint32_t a_Height, uint32_t a_ArrayLayers,
		RENDER_IMAGE_TYPE a_Type,
		RENDER_IMAGE_USAGE a_Usage,
		RENDER_IMAGE_FORMAT a_Format,
		RENDER_IMAGE_VIEWTYPE a_ViewType)
	{
		RenderImageCreateInfo t_Info;
		t_Info.width = a_Width;
		t_Info.height = a_Height;
		t_Info.arrayLayers = a_ArrayLayers;
		t_Info.type = a_Type;
		t_Info.usage = a_Usage;
		t_Info.format = a_Format;
		t_Info.viewtype = a_ViewType;
		return t_Info;
	}

	inline ShaderCreateInfo CreateShaderInfo(Buffer a_Buffer, RENDER_SHADER_STAGE a_Shaderstage)
	{
		ShaderCreateInfo t_Info;
		t_Info.buffer = a_Buffer;
		t_Info.shaderStage = a_Shaderstage;
		return t_Info;
	}

	struct RenderBackendCreateInfo
	{
		Slice<RENDER_EXTENSIONS> extensions;
		Slice<RENDER_EXTENSIONS> deviceExtensions;
#ifdef _WIN32
		HWND hwnd;
#endif //_WIN32
		const char* appName;
		const char* engineName;
		uint32_t windowWidth;
		uint32_t windowHeight;
		int version;
		bool validationLayers;
	};

	struct RenderPipelineCreateInfo
	{
		FrameBufferHandle framebufferHandle;
		Slice<BB::ShaderCreateInfo> shaderCreateInfos;
	};

	struct RenderFrameBufferCreateInfo
	{
		RENDER_LOAD_OP colorLoadOp;
		RENDER_STORE_OP colorStoreOp;
		RENDER_IMAGE_LAYOUT colorInitialLayout;
		RENDER_IMAGE_LAYOUT colorFinalLayout;
		uint32_t width;
		uint32_t height;
	};

	struct Vertex
	{
		float pos[2];
		float color[3];
	};


	//construction
	typedef APIRenderBackend (*PFN_RenderAPICreateBackend)(
		Allocator a_SysAllocator,
		Allocator a_TempAllocator,
		const RenderBackendCreateInfo& a_CreateInfo);

	typedef PipelineHandle (*PFN_RenderAPICreatePipeline)(
		Allocator a_TempAllocator,
		const RenderPipelineCreateInfo& a_CreateInfo);

	typedef FrameBufferHandle (*PFN_RenderAPICreateFrameBuffer)(
		Allocator a_TempAllocator,
		const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo);

	typedef CommandListHandle (*PFN_RenderAPICreateCommandList)(
		Allocator a_TempAllocator,
		const uint32_t a_BufferCount);

	//Utility
	typedef void (*PFN_RenderAPIResizeWindow)(
		Allocator a_TempAllocator,
		APIRenderBackend,
		uint32_t a_X,
		uint32_t a_Y);
	typedef void (*PFN_RenderAPIRenderFrame)(
		Allocator a_TempAllocator,
		CommandListHandle a_CommandHandle,
		FrameBufferHandle a_FrameBufferHandle,
		PipelineHandle a_PipeHandle);
	typedef void (*PFN_RenderAPIWaitDeviceReady)();

	//Deletion
	typedef void (*PFN_RenderAPIDestroyBackend)(APIRenderBackend a_Handle);
	typedef void (*PFN_RenderAPIDestroyFrameBuffer)(FrameBufferHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyPipeline)(PipelineHandle a_Handle);
	typedef void (*PFN_RenderAPIDestroyCommandList)(CommandListHandle a_Handle);

	struct APIBackendFunctionPointersCreateInfo
	{
		PFN_RenderAPICreateBackend* createBackend;
		PFN_RenderAPICreatePipeline* createPipeline;
		PFN_RenderAPICreateFrameBuffer* createFrameBuffer;
		PFN_RenderAPICreateCommandList* createCommandList;

		PFN_RenderAPIResizeWindow* resizeWindow;
		PFN_RenderAPIRenderFrame* renderFrame;
		PFN_RenderAPIWaitDeviceReady* waitDevice;

		PFN_RenderAPIDestroyBackend* destroyBackend;
		PFN_RenderAPIDestroyFrameBuffer* destroyFrameBuffer;
		PFN_RenderAPIDestroyPipeline* destroyPipeline;
		PFN_RenderAPIDestroyCommandList* destroyCommandList;
	};
}
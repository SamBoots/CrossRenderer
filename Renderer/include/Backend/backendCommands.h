#pragma once
#include <cstdint>
#include "Common.h"

namespace BB
{
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
}
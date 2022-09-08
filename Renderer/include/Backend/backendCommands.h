#pragma once
#include <cstdint>
#include "Common.h"

namespace BB
{
	namespace Render
	{
		using PipelineHandle = FrameworkHandle<struct PipelineHandleTag>;

		enum class BB_RENDER_BUFFER_USAGE : uint32_t
		{
			VERTEX,
			INDEX,
			UNIFORM,
			STORAGE,
			STAGING
		};

		enum class BB_RENDER_IMAGE_TYPE : uint32_t
		{
			TYPE_2D
		};

		enum class BB_RENDER_IMAGE_USAGE : uint32_t
		{
			SAMPLER
		};

		enum class BB_RENDER_IMAGE_FORMAT : uint32_t
		{
			DEPTH_STENCIL,
			SRGB
		};

		enum class BB_RENDER_IMAGE_VIEWTYPE : uint32_t
		{
			TYPE_2D,
			TYPE_2D_ARRAY
		};

		enum class BB_RENDER_SHADER_STAGE : uint32_t
		{
			VERTEX,
			FRAGMENT
		};

		struct BBRenderBufferInfo
		{
			uint64_t size = 0;
			BB_RENDER_BUFFER_USAGE usage = 0;
		};

		struct BBRenderImageInfo
		{
			// The width in texels.
			uint32_t width = 0;
			// The height in texels.
			uint32_t height = 0;

			uint32_t arrayLayers = 0;
			BB_RENDER_IMAGE_TYPE type = 0;
			BB_RENDER_IMAGE_USAGE usage = 0;
			// The format of the image's texels.
			BB_RENDER_IMAGE_FORMAT format = 0;

			BB_RENDER_IMAGE_VIEWTYPE viewtype = 0;
		};

		struct RLShaderInfo
		{
			const char* bytecode;
			uint64_t size;
			BB_RENDER_SHADER_STAGE shaderStage;
		};

		inline BBRenderBufferInfo RenderBufferInfo(uint64_t a_Size, BB_RENDER_BUFFER_USAGE a_Usage)
		{
			BBRenderBufferInfo t_Info;
			t_Info.size = a_Size;
			t_Info.usage = a_Usage;
			return t_Info;
		}

		inline BBRenderImageInfo RenderImageInfo(uint32_t a_Width, uint32_t a_Height, uint32_t a_ArrayLayers,
			BB_RENDER_IMAGE_TYPE a_Type,
			BB_RENDER_IMAGE_USAGE a_Usage,
			BB_RENDER_IMAGE_FORMAT a_Format,
			BB_RENDER_IMAGE_VIEWTYPE a_ViewType)
		{
			RLRenderImageInfo t_Info;
			t_Info.width = a_Width;
			t_Info.height = a_Height;
			t_Info.arrayLayers = a_ArrayLayers;
			t_Info.type = a_Type;
			t_Info.usage = a_Usage;
			t_Info.format = a_Format;
			t_Info.viewtype = a_ViewType;
			return t_Info;
		}

		inline RLShaderInfo ShaderInfo(const char* a_ByteCode, uint64_t a_Size, BB_RENDER_SHADER_STAGE a_Shaderstage)
		{
			RLShaderInfo t_Info;
			t_Info.bytecode = a_ByteCode;
			t_Info.size = a_Size;
			t_Info.shaderStage = a_Shaderstage;
			return t_Info;
		}
	}
}
#include "RenderBackendCommon.h"

namespace BB
{
	static inline const char* DescriptorTypeStr(const RENDER_DESCRIPTOR_TYPE a_Type)
	{
		switch (a_Type)
		{
		case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:		return "READONLY_CONSTANT";
		case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:		return "READONLY_BUFFER";
		case RENDER_DESCRIPTOR_TYPE::READWRITE:				return "READWRITE";
		case RENDER_DESCRIPTOR_TYPE::IMAGE:					return "IMAGE";
		case RENDER_DESCRIPTOR_TYPE::SAMPLER:				return "SAMPLER";
		default:
			BB_ASSERT(false, "RENDER_DESCRIPTOR_TYPE unknown in resource tracker!");
			return "error";
			break;
		}
	}

	static inline const char* ShaderStageStr(const RENDER_SHADER_STAGE a_Stage)
	{
		switch (a_Stage)
		{
		case RENDER_SHADER_STAGE::ALL:				return "ALL";
		case RENDER_SHADER_STAGE::VERTEX:			return "VERTEX";
		case RENDER_SHADER_STAGE::FRAGMENT_PIXEL:	return "FRAGMENT_PIXEL";
		default:
			BB_ASSERT(false, "RENDER_SHADER_STAGE unknown in resource tracker!");
			return "error";
			break;
		}
	}

	static inline const char* InputFormatStr(const RENDER_INPUT_FORMAT a_Format)
	{
		switch (a_Format)
		{
		case RENDER_INPUT_FORMAT::RGBA32:	return "RGBA32";
		case RENDER_INPUT_FORMAT::RGB32:	return "RGB32";
		case RENDER_INPUT_FORMAT::RG32:		return "RG32";
		case RENDER_INPUT_FORMAT::R32:		return "R32";
		case RENDER_INPUT_FORMAT::RGBA8:	return "RGBA8";
		case RENDER_INPUT_FORMAT::RG8:		return "RG8";

		default:
			BB_ASSERT(false, "RENDER_INPUT_FORMAT unknown in resource tracker!");
			return "error";
			break;
		}
	}

	static inline const char* BlendFactorStr(const RENDER_BLEND_FACTOR a_BlendFac)
	{
		switch (a_BlendFac)
		{
		case RENDER_BLEND_FACTOR::ZERO:					return "ZERO";
		case RENDER_BLEND_FACTOR::ONE:					return "ONE";
		case RENDER_BLEND_FACTOR::SRC_ALPHA:			return "SRC_ALPHA";
		case RENDER_BLEND_FACTOR::ONE_MINUS_SRC_ALPHA:	return "ONE_MINUS_SRC_ALPHA";

		default:
			BB_ASSERT(false, "RENDER_BLEND_FACTOR unknown in resource tracker!");
			return "error";
			break;
		}
	}

	static inline const char* BlendOpStr(const RENDER_BLEND_OP a_BlendOp)
	{
		switch (a_BlendOp)
		{
		case RENDER_BLEND_OP::ADD:		return "Blend Op: ADD";
		case RENDER_BLEND_OP::SUBTRACT:	return "Blend Op: SUBTRACT";

		default:
			BB_ASSERT(false, "RENDER_BLEND_OP unknown in resource tracker!");
			return "error";
			break;
		}
	}

	static inline const char* ImageTypeStr(const RENDER_IMAGE_TYPE a_ImageType)
	{
		switch (a_ImageType)
		{
		case RENDER_IMAGE_TYPE::TYPE_2D:	return "TYPE_2D";

		default:
			BB_ASSERT(false, "RENDER_IMAGE_TYPE unknown in resource tracker!");
			return "error";
			break;
		}
	}

	static inline const char* ImageFormatStr(const RENDER_IMAGE_FORMAT a_ImageFormat)
	{
		switch (a_ImageFormat)
		{
		case RENDER_IMAGE_FORMAT::DEPTH_STENCIL:		return "DEPTH_STENCIL";
		case RENDER_IMAGE_FORMAT::RGBA8_SRGB:			return "RGBA8_SRGB";
		case RENDER_IMAGE_FORMAT::RGBA8_UNORM:			return "RGBA8_UNORM";
		default:
			BB_ASSERT(false, "RENDER_IMAGE_FORMAT unknown in resource tracker!");
			return "error";
			break;
		}
	}

	static inline const char* ImageTilingStr(const RENDER_IMAGE_TILING a_Tiling)
	{
		switch (a_Tiling)
		{
		case RENDER_IMAGE_TILING::LINEAR:		return "LINEAR";
		case RENDER_IMAGE_TILING::OPTIMAL:		return "OPTIMAL";
		default:
			BB_ASSERT(false, "RENDER_IMAGE_TILING unknown in resource tracker!");
			return "error";
			break;
		}
	}

	static inline const char* ImageLayoutStr(const RENDER_IMAGE_LAYOUT a_Layout)
	{
		switch (a_Layout)
		{
		case RENDER_IMAGE_LAYOUT::UNDEFINED:				return "UNDEFINED";
		case RENDER_IMAGE_LAYOUT::GENERAL:					return "GENERAL";
		case RENDER_IMAGE_LAYOUT::TRANSFER_SRC:				return "TRANSFER_SRC";
		case RENDER_IMAGE_LAYOUT::TRANSFER_DST:				return "TRANSFER_DST";
		case RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL:	return "COLOR_ATTACHMENT_OPTIMAL";
		case RENDER_IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT:	return "DEPTH_STENCIL_ATTACHMENT";
		case RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY:			return "SHADER_READ_ONLY";
		case RENDER_IMAGE_LAYOUT::PRESENT:					return "PRESENT";
		default:
			BB_ASSERT(false, "RENDER_IMAGE_LAYOUT unknown in resource tracker!");
			return "error";
			break;
		}
	}

	static inline const char* SamplerAddressStr(const SAMPLER_ADDRESS_MODE a_Mode)
	{
		switch (a_Mode)
		{
		case SAMPLER_ADDRESS_MODE::REPEAT:	return "REPEAT";
		case SAMPLER_ADDRESS_MODE::MIRROR:	return "MIRROR";
		case SAMPLER_ADDRESS_MODE::BORDER:	return "BORDER";
		case SAMPLER_ADDRESS_MODE::CLAMP:	return "CLAMP";
		default:
			BB_ASSERT(false, "SAMPLER_ADDRESS_MODE unknown in resource tracker!");
			return "error";
			break;
		}
	}

	static inline const char* SamplerFilterStr(const SAMPLER_FILTER a_Filter)
	{
		switch (a_Filter)
		{
		case SAMPLER_FILTER::NEAREST:		return "NEAREST";
		case SAMPLER_FILTER::LINEAR:		return "LINEAR";
		default:
			BB_ASSERT(false, "SAMPLER_FILTER unknown in resource tracker!");
			return "error";
			break;
		}
	}
}
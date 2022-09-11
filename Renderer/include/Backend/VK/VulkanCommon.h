#pragma once
#include "Utils/Logger.h"
#include "BackendCommands.h"
#include <vulkan/vulkan.h>

namespace BB
{
#define VKASSERT(a_VKResult, a_Msg)\
	if (a_VKResult != VK_SUCCESS)\
		BB_ASSERT(false, a_Msg);\

	constexpr uint32_t EMPTY_FAMILY_INDICES = UINT32_MAX; 
	namespace VKConv
	{
		inline VkShaderStageFlagBits ShaderStageBits(RENDER_SHADER_STAGE a_Stage)
		{
			switch (a_Stage)
			{
			case BB::RENDER_SHADER_STAGE::VERTEX:
				return VK_SHADER_STAGE_VERTEX_BIT;
				break;
			case BB::RENDER_SHADER_STAGE::FRAGMENT:
				return VK_SHADER_STAGE_FRAGMENT_BIT;
				break;
			default:
				BB_ASSERT(false,
					"Vulkan: RENDER_SHADER_STAGE failed to convert to a VkShaderStageFlagBits.");
				return VK_SHADER_STAGE_ALL;
				break;
			}
		}
	}
}

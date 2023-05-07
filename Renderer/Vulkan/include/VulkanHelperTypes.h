#pragma once
#include "Utils/Logger.h"
#include "Utils/Slice.h"

#include "RenderBackendCommon.h"
#include <vulkan/vulkan.h>

namespace BB
{
#ifdef _DEBUG
#define VKASSERT(a_VKResult, a_Msg)\
	if (a_VKResult != VK_SUCCESS)\
		BB_ASSERT(false, a_Msg)\

#else
#define VKASSERT(a_VKResult, a_Msg) a_VKResult
#endif //_DEBUG

	namespace VKConv
	{
		static inline VkShaderStageFlags ShaderVisibility(const RENDER_SHADER_STAGE a_Stage)
		{
			switch (a_Stage)
			{
			case RENDER_SHADER_STAGE::ALL:						return VK_SHADER_STAGE_ALL;
			case RENDER_SHADER_STAGE::FRAGMENT_PIXEL:			return VK_SHADER_STAGE_FRAGMENT_BIT;
			case RENDER_SHADER_STAGE::VERTEX:					return VK_SHADER_STAGE_VERTEX_BIT;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_SHADER_STAGE failed to convert to a VkShaderStageFlags.");
				return VK_SHADER_STAGE_ALL;
				break;
			}
		}

		static inline VkBufferUsageFlags RenderBufferUsage(const RENDER_BUFFER_USAGE a_Usage)
		{
			switch (a_Usage)
			{
			case RENDER_BUFFER_USAGE::VERTEX:					return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			case RENDER_BUFFER_USAGE::INDEX:					return VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			case RENDER_BUFFER_USAGE::UNIFORM:					return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			case RENDER_BUFFER_USAGE::STORAGE:					return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			case RENDER_BUFFER_USAGE::STAGING:					return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_BUFFER_USAGE failed to convert to a VkBufferUsageFlags.");
				return 0;
				break;
			}
		}

		static inline VkDescriptorType DescriptorBufferType(const RENDER_DESCRIPTOR_TYPE a_Type)
		{
			switch (a_Type)
			{
			case BB::RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			case BB::RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			case BB::RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT_DYNAMIC:	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			case BB::RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER_DYNAMIC:	return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
			case BB::RENDER_DESCRIPTOR_TYPE::READWRITE:					return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
			case BB::RENDER_DESCRIPTOR_TYPE::IMAGE:						return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			case BB::RENDER_DESCRIPTOR_TYPE::SAMPLER:					return VK_DESCRIPTOR_TYPE_SAMPLER;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_DESCRIPTOR_TYPE failed to convert to a VkDescriptorType.");
				return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				break;
			}
		}

		static inline VkShaderStageFlagBits ShaderStageBits(const RENDER_SHADER_STAGE a_Stage)
		{
			switch (a_Stage)
			{
			case BB::RENDER_SHADER_STAGE::ALL:					return VK_SHADER_STAGE_ALL;
			case BB::RENDER_SHADER_STAGE::VERTEX:				return VK_SHADER_STAGE_VERTEX_BIT;
			case BB::RENDER_SHADER_STAGE::FRAGMENT_PIXEL:		return VK_SHADER_STAGE_FRAGMENT_BIT;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_SHADER_STAGE failed to convert to a VkShaderStageFlagBits.");
				return VK_SHADER_STAGE_ALL;
				break;
			}
		}

		static inline VkAttachmentLoadOp LoadOP(const RENDER_LOAD_OP a_LoadOp)
		{
			switch (a_LoadOp)
			{
			case BB::RENDER_LOAD_OP::LOAD:						return VK_ATTACHMENT_LOAD_OP_LOAD;
			case BB::RENDER_LOAD_OP::CLEAR:						return VK_ATTACHMENT_LOAD_OP_CLEAR;
			case BB::RENDER_LOAD_OP::DONT_CARE:					return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_LOAD_OP failed to convert to a VkAttachmentLoadOp.");
				return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				break;
			}
		}

		static inline VkAttachmentStoreOp StoreOp(const RENDER_STORE_OP a_StoreOp)
		{
			switch (a_StoreOp)
			{
			case BB::RENDER_STORE_OP::STORE:					return VK_ATTACHMENT_STORE_OP_STORE;
			case BB::RENDER_STORE_OP::DONT_CARE:				return VK_ATTACHMENT_STORE_OP_DONT_CARE;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_STORE_OP failed to convert to a VkAttachmentStoreOp.");
				return VK_ATTACHMENT_STORE_OP_DONT_CARE;
				break;
			}
		}

		static  VkLogicOp LogicOp(const RENDER_LOGIC_OP a_LogicOp)
		{
			switch (a_LogicOp)
			{
			case BB::RENDER_LOGIC_OP::CLEAR:					return VK_LOGIC_OP_CLEAR;
			case BB::RENDER_LOGIC_OP::COPY:						return VK_LOGIC_OP_COPY;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_LOGIC_OP failed to convert to a VkLogicOp.");
				return VK_LOGIC_OP_CLEAR;
				break;
			}
		}

		static inline VkImageLayout ImageLayout(const RENDER_IMAGE_LAYOUT a_ImageLayout)
		{
			switch (a_ImageLayout)
			{
			case RENDER_IMAGE_LAYOUT::UNDEFINED:				return VK_IMAGE_LAYOUT_UNDEFINED;
			case RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			case RENDER_IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			case RENDER_IMAGE_LAYOUT::GENERAL:					return VK_IMAGE_LAYOUT_GENERAL;
			case RENDER_IMAGE_LAYOUT::TRANSFER_SRC:				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			case RENDER_IMAGE_LAYOUT::TRANSFER_DST:				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			case RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY:			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			case RENDER_IMAGE_LAYOUT::PRESENT:					return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_IMAGE_LAYOUT failed to convert to a VkImageLayout.");
				return VK_IMAGE_LAYOUT_UNDEFINED;
				break;
			}
		}

		static inline VkPipelineStageFlags PipelineStage(const RENDER_PIPELINE_STAGE a_Stage)
		{
			switch (a_Stage)
			{
			case RENDER_PIPELINE_STAGE::TOP_OF_PIPELINE:		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			case RENDER_PIPELINE_STAGE::TRANSFER:				return VK_PIPELINE_STAGE_TRANSFER_BIT;
			case RENDER_PIPELINE_STAGE::VERTEX_INPUT:			return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
			case RENDER_PIPELINE_STAGE::VERTEX_SHADER:			return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
			case RENDER_PIPELINE_STAGE::EARLY_FRAG_TEST:		return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			case RENDER_PIPELINE_STAGE::FRAGMENT_SHADER:		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			case RENDER_PIPELINE_STAGE::END_OF_PIPELINE:		return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_PIPELINE_STAGE failed to convert to a VkPipelineStageFlags.");
				return VK_PIPELINE_STAGE_NONE;
				break;
			}
		}

		static inline VkAccessFlags AccessMask(const RENDER_ACCESS_MASK a_Type)
		{
			switch (a_Type)
			{
			case RENDER_ACCESS_MASK::NONE:						return VK_ACCESS_NONE;
			case RENDER_ACCESS_MASK::TRANSFER_WRITE:			return VK_ACCESS_TRANSFER_WRITE_BIT;
			case RENDER_ACCESS_MASK::DEPTH_STENCIL_READ_WRITE:	return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			case RENDER_ACCESS_MASK::SHADER_READ:				return VK_ACCESS_SHADER_READ_BIT;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_IMAGE_TYPE failed to convert to a VkAccessFlags.");
				return VK_IMAGE_TYPE_1D;
				break;
			}
		}

		static inline VkBlendFactor BlendFactors(const RENDER_BLEND_FACTOR a_Factor)
		{
			switch (a_Factor)
			{
			case RENDER_BLEND_FACTOR::ZERO:						return VK_BLEND_FACTOR_ZERO;
			case RENDER_BLEND_FACTOR::ONE:						return VK_BLEND_FACTOR_ONE;
			case RENDER_BLEND_FACTOR::SRC_ALPHA:				return VK_BLEND_FACTOR_SRC_ALPHA;
			case RENDER_BLEND_FACTOR::ONE_MINUS_SRC_ALPHA:		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_BLEND_FACTOR failed to convert to a VkBlendFactor.");
				return VK_BLEND_FACTOR_ZERO;
				break;
			}
		}

		static inline VkBlendOp BlendOp(const RENDER_BLEND_OP a_BlendOp)
		{
			switch (a_BlendOp)
			{
			case RENDER_BLEND_OP::ADD:						return VK_BLEND_OP_ADD;
			case RENDER_BLEND_OP::SUBTRACT:					return VK_BLEND_OP_SUBTRACT;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_BLEND_OP failed to convert to a VkBlendOp.");
				return VK_BLEND_OP_ADD;
				break;
			}
		}

		static inline VkCullModeFlags CullMode(const RENDER_CULL_MODE a_BlendOp)
		{
			switch (a_BlendOp)
			{
			case RENDER_CULL_MODE::NONE:					return VK_CULL_MODE_NONE;
			case RENDER_CULL_MODE::FRONT:					return VK_CULL_MODE_FRONT_BIT;
			case RENDER_CULL_MODE::BACK:					return VK_CULL_MODE_BACK_BIT;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_CULL_MODE failed to convert to a VkCullModeFlags.");
				return VK_CULL_MODE_NONE;
				break;
			}
		}

		static inline VkSamplerAddressMode AddressMode(const SAMPLER_ADDRESS_MODE a_Mode)
		{
			switch (a_Mode)
			{
			case SAMPLER_ADDRESS_MODE::REPEAT:		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case SAMPLER_ADDRESS_MODE::MIRROR:		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case SAMPLER_ADDRESS_MODE::BORDER:		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			case SAMPLER_ADDRESS_MODE::CLAMP:		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			}
		}

		struct ExtensionResult
		{
			const char** extensions;
			uint32_t count;
		};

		//Maxiumum of 32 extensions at the moment.
		inline ExtensionResult TranslateExtensions(const Allocator a_TempAllocator, BB::Slice<RENDER_EXTENSIONS> a_Extensions)
		{
			ExtensionResult t_Result;
			t_Result.extensions = BBnewArr(a_TempAllocator, 32, const char*);
			t_Result.count = 0;
			for (size_t i = 0; i < a_Extensions.size(); i++)
			{
				switch (a_Extensions[i])
				{
				case BB::RENDER_EXTENSIONS::STANDARD_VULKAN_INSTANCE:
					t_Result.extensions[t_Result.count++] = VK_KHR_SURFACE_EXTENSION_NAME;
					t_Result.extensions[t_Result.count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
					t_Result.extensions[t_Result.count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
					break;
				case BB::RENDER_EXTENSIONS::STANDARD_VULKAN_DEVICE:
					t_Result.extensions[t_Result.count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
					t_Result.extensions[t_Result.count++] = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
					t_Result.extensions[t_Result.count++] = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
					
					break;
				case BB::RENDER_EXTENSIONS::DEBUG:
					t_Result.extensions[t_Result.count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
					break;
				case BB::RENDER_EXTENSIONS::PIPELINE_EXTENDED_DYNAMIC_STATE:
					t_Result.extensions[t_Result.count++] = VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME;
					break;
				default:
					BB_WARNING(false,
						"Vulkan: Tried to get an extensions that vulkan does not have.",
						WarningType::HIGH);
					break;
				}
			}
			return t_Result;
		}
	}

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		VkSurfaceFormatKHR* formats;
		VkPresentModeKHR* presentModes;
		uint32_t formatCount;
		uint32_t presentModeCount;
	};

	constexpr uint32_t EMPTY_FAMILY_INDICES = UINT32_MAX;

	struct SwapchainFrame
	{
		VkImage image;
		VkImageView imageView;

		VkSemaphore imageAvailableSem; //The window presenting API requires a binary semaphore, it cannot use timelime that we use.
		VkSemaphore imageRenderFinishedSem; //The window presenting API requires a binary semaphore, it cannot use timelime that we use normally.
		VkSemaphore frameTimelineSemaphore; //A special timeline semaphore incorperated inside the backend for ease of use.
		uint64_t frameWaitValue; //The previous frame it's timelime semaphore value. Used to check if we can use it.
	};

	struct VulkanSwapChain
	{
		VkSwapchainKHR swapChain;
		VkFormat imageFormat;
		VkExtent2D extent;
		SwapchainFrame* frames;
	};

	//this struct stores data to check our device limits.
	struct VulkanPhysicalDeviceInfo
	{
		float maxAnisotropy;
	};

	struct VulkanPipeline
	{
		VkDescriptorSet sets[4];
		uint32_t setCount;

		VkDescriptorSetLayout setLayout;

		VkPipeline pipeline;
		VkPipelineLayout layout;
	};

	struct VulkanQueuesIndices
	{
		uint32_t graphics;
		uint32_t present; //Is currently always same as graphics.
		uint32_t compute;
		uint32_t transfer;
	};

	struct VulkanDebug
	{
		VkDebugUtilsMessengerEXT debugMessenger;
		const char** extensions;
		size_t extensionCount;
	};

	struct VulkanQueueDeviceInfo
	{
		uint32_t index;
		uint32_t queueCount;
	};

	VulkanQueueDeviceInfo FindQueueIndex(VkQueueFamilyProperties* a_QueueProperties, uint32_t a_FamilyPropertyCount, VkQueueFlags a_QueueFlags);

	struct VulkanCommandQueue
	{
		VkQueue queue;

		VkSemaphore timelineSemaphore;
		uint64_t nextSemValue;
		uint64_t lastCompleteValue;
	};

	struct VulkanFence
	{
		VkSemaphore timelineSem;
		uint64_t nextFenceValue;
		uint64_t lastCompleteValue;
	};

	struct VulkanConstant
	{
		VkShaderStageFlags shaderStage;
		uint32_t offset;
	};

	struct VulkanBindingSet
	{
		//Maximum of 4 bindings.
		RENDER_BINDING_SET bindingSet = {};
		VkDescriptorSet set;
		VkDescriptorSetLayout setLayout;

		uint32_t pushConstantCount = 0;
		VulkanConstant pushConstants[4];
	};
}
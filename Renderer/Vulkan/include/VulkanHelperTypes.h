#pragma once
#include "Utils/Logger.h"
#include "Utils/Slice.h"
#include "BBMemory.h"
#include "TemporaryAllocator.h"

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

#pragma region BufferData
	static VkVertexInputBindingDescription VertexBindingDescription()
	{
		VkVertexInputBindingDescription t_BindingDescription{};
		t_BindingDescription.binding = 0;
		t_BindingDescription.stride = sizeof(Vertex);
		t_BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return t_BindingDescription;
	}

	static FixedArray<VkVertexInputAttributeDescription, 2> VertexAttributeDescriptions()
	{
		FixedArray<VkVertexInputAttributeDescription, 2> t_AttributeDescriptions;
		t_AttributeDescriptions[0].binding = 0;
		t_AttributeDescriptions[0].location = 0;
		t_AttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		t_AttributeDescriptions[0].offset = offsetof(Vertex, pos);

		t_AttributeDescriptions[1].binding = 0;
		t_AttributeDescriptions[1].location = 1;
		t_AttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		t_AttributeDescriptions[1].offset = offsetof(Vertex, color);

		return t_AttributeDescriptions;
	}
#pragma endregion

	namespace VKConv
	{
		inline VkBufferUsageFlags RenderBufferUsage(const RENDER_BUFFER_USAGE a_Usage)
		{
			switch (a_Usage)
			{
			case RENDER_BUFFER_USAGE::VERTEX:
				return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				break;
			case RENDER_BUFFER_USAGE::INDEX:
				return VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				break;
			case RENDER_BUFFER_USAGE::UNIFORM:
				return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				break;
			case RENDER_BUFFER_USAGE::STORAGE:
				return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				break;
			case RENDER_BUFFER_USAGE::STAGING:
				return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				break;
			default:
				BB_ASSERT(false, "this buffer usage is not supported by the vulkan backend!");
				return 0;
				break;
			}
		}

		inline VkDescriptorType DescriptorBufferType(const DESCRIPTOR_BUFFER_TYPE a_Type)
		{
			switch (a_Type)
			{
			case BB::DESCRIPTOR_BUFFER_TYPE::READONLY_CONSTANT:
				return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				break;
			case BB::DESCRIPTOR_BUFFER_TYPE::READONLY_BUFFER:
				return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
				break;
			case BB::DESCRIPTOR_BUFFER_TYPE::INPUT_ATTACHMENT:
				return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				break;
			default:
				BB_ASSERT(false, "this descriptor_type usage is not supported by the vulkan backend!");
				return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				break;
			}
		}

		inline VkShaderStageFlagBits ShaderStageBits(const RENDER_SHADER_STAGE a_Stage)
		{
			switch (a_Stage)
			{
			case BB::RENDER_SHADER_STAGE::VERTEX:
				return VK_SHADER_STAGE_VERTEX_BIT;
				break;
			case BB::RENDER_SHADER_STAGE::FRAGMENT_PIXEL:
				return VK_SHADER_STAGE_FRAGMENT_BIT;
				break;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_SHADER_STAGE failed to convert to a VkShaderStageFlagBits.");
				return VK_SHADER_STAGE_ALL;
				break;
			}
		}

		inline VkAttachmentLoadOp LoadOP(const RENDER_LOAD_OP a_LoadOp)
		{
			switch (a_LoadOp)
			{
			case BB::RENDER_LOAD_OP::LOAD:
				return VK_ATTACHMENT_LOAD_OP_LOAD;
				break;
			case BB::RENDER_LOAD_OP::CLEAR:
				return VK_ATTACHMENT_LOAD_OP_CLEAR;
				break;
			case BB::RENDER_LOAD_OP::DONT_CARE:
				return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				break;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_LOAD_OP failed to convert to a VkAttachmentLoadOp.");
				return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				break;
			}
		}

		inline VkAttachmentStoreOp StoreOp(const RENDER_STORE_OP a_StoreOp)
		{
			switch (a_StoreOp)
			{
			case BB::RENDER_STORE_OP::STORE:
				return VK_ATTACHMENT_STORE_OP_STORE;
				break;
			case BB::RENDER_STORE_OP::DONT_CARE:
				return VK_ATTACHMENT_STORE_OP_DONT_CARE;
				break;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_STORE_OP failed to convert to a VkAttachmentStoreOp.");
				return VK_ATTACHMENT_STORE_OP_DONT_CARE;
				break;
			}
		}

		inline VkImageLayout ImageLayout(const RENDER_IMAGE_LAYOUT a_ImageLayout)
		{
			switch (a_ImageLayout)
			{
			case BB::RENDER_IMAGE_LAYOUT::UNDEFINED:
				return VK_IMAGE_LAYOUT_UNDEFINED;
				break;
			case BB::RENDER_IMAGE_LAYOUT::GENERAL:
				return VK_IMAGE_LAYOUT_GENERAL;
				break;
			case BB::RENDER_IMAGE_LAYOUT::TRANSFER_SRC:
				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				break;
			case BB::RENDER_IMAGE_LAYOUT::TRANSFER_DST:
				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				break;
			case BB::RENDER_IMAGE_LAYOUT::PRESENT:
				return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				break;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_IMAGE_LAYOUT failed to convert to a VkImageLayout.");
				return VK_IMAGE_LAYOUT_UNDEFINED;
				break;
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

	struct VulkanShaderResult
	{
		VkShaderModule* shaderModules;
		VkPipelineShaderStageCreateInfo* pipelineShaderStageInfo;
	};

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		VkSurfaceFormatKHR* formats;
		VkPresentModeKHR* presentModes;
		uint32_t formatCount;
		uint32_t presentModeCount;
	};

	constexpr uint32_t EMPTY_FAMILY_INDICES = UINT32_MAX;

	struct DepthCreateInfo
	{
		VkFormat depthFormat;
		VkImageLayout initialLayout;
		VkImageLayout finalLayout;
	};

	struct FrameWaitSync
	{
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
		VkImage* images;
		VkImageView* imageViews;
		FrameWaitSync* waitSyncs;
	};

	struct VulkanPipeline
	{
		VkDescriptorSet sets[4];
		uint32_t setCount;

		VkDescriptorSetLayout setLayout;

		VkPipeline pipeline;
		VkPipelineLayout layout;
	};

	struct VulkanFrameBuffer
	{
		uint32_t width;
		uint32_t height;
		VkFramebuffer* frameBuffers;
		VkRenderPass renderPass;
		VkClearValue clearValue;
	};

	struct VulkanDevice
	{
		VkDevice logicalDevice;
		VkPhysicalDevice physicalDevice;
		VkQueue presentQueue;
		struct VulkanQueues
		{
			uint32_t graphics;
			uint32_t present; //Is currently always same as graphics.
			uint32_t compute;
			uint32_t transfer;
		} queueIndices;
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
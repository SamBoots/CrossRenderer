#pragma once
#include "Utils/Logger.h"
#include "Utils/Slice.h"
#include "Allocators/AllocTypes.h"

#include "RenderBackendCommon.h"
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
				BB_ASSERT(false, "Vulkan: RENDER_SHADER_STAGE failed to convert to a VkShaderStageFlagBits.");
				return VK_SHADER_STAGE_ALL;
				break;
			}
		}

		inline VkAttachmentLoadOp LoadOP(RENDER_LOAD_OP a_LoadOp)
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

		inline VkAttachmentStoreOp StoreOp(RENDER_STORE_OP a_StoreOp)
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

		inline VkImageLayout ImageLayout(RENDER_IMAGE_LAYOUT a_ImageLayout)
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
			t_Result.extensions = BBnewArr<const char*>(a_TempAllocator, 32);
			t_Result.count = 0;
			for (size_t i = 0; i < a_Extensions.size(); i++)
			{
				switch (a_Extensions[i])
				{
				case BB::RENDER_EXTENSIONS::STANDARD_VULKAN_INSTANCE:
					t_Result.extensions[t_Result.count++] = "VK_KHR_surface";
					t_Result.extensions[t_Result.count++] = "VK_KHR_win32_surface";
					break;
				case BB::RENDER_EXTENSIONS::STANDARD_VULKAN_DEVICE:
					t_Result.extensions[t_Result.count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
					break;
				case BB::RENDER_EXTENSIONS::DEBUG:
					t_Result.extensions[t_Result.count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
					break;
				case BB::RENDER_EXTENSIONS::PHYSICAL_DEVICE_EXTRA_PROPERTIES:
					t_Result.extensions[t_Result.count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
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

	struct DepthCreateInfo
	{
		VkFormat depthFormat;
		VkImageLayout initialLayout;
		VkImageLayout finalLayout;
	};

	struct VulkanSwapChain
	{
		VkSwapchainKHR swapChain;
		VkFormat imageFormat;
		VkExtent2D extent;
		VkImage* images;
		VkImageView* imageViews;

		VkFence* frameFences;
		VkSemaphore* presentSems;
		VkSemaphore* renderSems;
	};

	struct VulkanPipeline
	{
		VkPipeline pipeline;
	};

	struct VulkanFrameBuffer
	{
		uint32_t width;
		uint32_t height;
		VkFramebuffer* frameBuffers;
		VkRenderPass renderPass;
		uint32_t frameBufferCount;
	};

	struct VulkanCommandList
	{
		struct GraphicsCommands
		{
			VkCommandPool pool;
			VkCommandBuffer* buffers;
			uint32_t bufferCount;
			uint32_t currentFree;
		};

		GraphicsCommands* graphicCommands;
	};

	struct VulkanDevice
	{
		VkDevice logicalDevice;
		VkPhysicalDevice physicalDevice;

		VkQueue graphicsQueue;
		VkQueue presentQueue;
	};

	struct VulkanBackend
	{
		VkInstance instance;
		VkSurfaceKHR surface;

#ifdef _DEBUG
		VkDebugUtilsMessengerEXT debugMessenger;
		const char** extensions;
		uint32_t extensionCount;
#endif //_DEBUG
	};

	//Functions
	APIRenderBackend VulkanCreateBackend(Allocator a_SysAllocator,
		Allocator a_TempAllocator,
		const RenderBackendCreateInfo& a_CreateInfo);

	FrameBufferHandle VulkanCreateFrameBuffer(Allocator a_TempAllocator,
		const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo);

	PipelineHandle VulkanCreatePipeline(Allocator a_TempAllocator,
		const RenderPipelineCreateInfo& a_CreateInfo);

	CommandListHandle VulkanCreateCommandList(Allocator a_TempAllocator,
		const uint32_t a_BufferCount);

	void RenderFrame(Allocator a_TempAllocator,
		CommandListHandle a_CommandHandle,
		FrameBufferHandle a_FrameBufferHandle,
		PipelineHandle a_PipeHandle);

	void VulkanWaitDeviceReady();

	void VulkanDestroyCommandList(CommandListHandle a_Handle);
	void VulkanDestroyFramebuffer(FrameBufferHandle a_Handle);
	void VulkanDestroyPipeline(PipelineHandle a_Handle);
	void VulkanDestroyBackend(APIRenderBackend a_Handle);
}

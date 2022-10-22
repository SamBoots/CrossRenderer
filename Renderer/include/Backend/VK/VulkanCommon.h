#pragma once
#include "Utils/Logger.h"
#include "Utils/Slice.h"
#include "BBMemory.h"

#include "RenderBackendCommon.h"
#include <vulkan/vulkan.h>

namespace BB
{
#ifdef _DEBUG
#define VKASSERT(a_VKResult, a_Msg)\
	if (a_VKResult != VK_SUCCESS)\
		BB_ASSERT(false, a_Msg);\

#else
#define VKASSERT(a_VKResult, a_Msg) a_VKResult

#endif //_DEBUG

	constexpr uint32_t EMPTY_FAMILY_INDICES = UINT32_MAX;
	namespace VKConv
	{
		inline VkBufferUsageFlags RenderBufferUsage(RENDER_BUFFER_USAGE a_Usage)
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
				return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
				break;
			case RENDER_BUFFER_USAGE::STORAGE:
				return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
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

		inline VkMemoryPropertyFlags MemoryPropertyFlags(RENDER_MEMORY_PROPERTIES a_Properties)
		{
			switch (a_Properties)
			{
			case BB::RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL:
				return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
				break;
			case BB::RENDER_MEMORY_PROPERTIES::HOST_VISIBLE:
				return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
				break;
			default:
				BB_ASSERT(false, "this memory property is not supported by the vulkan backend!");
				return 0;
				break;
			}
		}

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
			t_Result.extensions = BBnewArr(a_TempAllocator, 32, const char*);
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
			VkCommandBuffer currentRecording = VK_NULL_HANDLE;
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
	struct VulkanDebug
	{
		VkDebugUtilsMessengerEXT debugMessenger;
		const char** extensions;
		size_t extensionCount;
	};


	//Functions
	APIRenderBackend VulkanCreateBackend(Allocator a_TempAllocator,const RenderBackendCreateInfo& a_CreateInfo);
	FrameBufferHandle VulkanCreateFrameBuffer(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo);
	PipelineHandle VulkanCreatePipeline(Allocator a_TempAllocator, const RenderPipelineCreateInfo& a_CreateInfo);
	CommandListHandle VulkanCreateCommandList(Allocator a_TempAllocator, const RenderCommandListCreateInfo& a_CreateInfo);
	RBufferHandle VulkanCreateBuffer(const RenderBufferCreateInfo& a_Info);

	RecordingCommandListHandle VulkanStartCommandList(const CommandListHandle a_CmdHandle, const FrameBufferHandle a_Framebuffer);
	void VulkanEndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
	void VulkanBindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
	void VulkanDrawBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_BufferHandles, const size_t a_BufferCount);

	void VulkanBufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_View, const uint64_t a_Offset);

	void ResizeWindow(Allocator a_TempAllocator, const uint32_t a_X, const uint32_t a_Y);
	void RenderFrame(Allocator a_TempAllocator, const CommandListHandle a_CommandHandle, const FrameBufferHandle a_FrameBufferHandle, const PipelineHandle a_PipeHandle);

	void VulkanWaitDeviceReady();

	void VulkanDestroyBuffer(const RBufferHandle a_Handle);
	void VulkanDestroyCommandList(const CommandListHandle a_Handle);
	void VulkanDestroyFramebuffer(const FrameBufferHandle a_Handle);
	void VulkanDestroyPipeline(const PipelineHandle a_Handle);
	void VulkanDestroyBackend();


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
		t_AttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		t_AttributeDescriptions[0].offset = offsetof(Vertex, pos);

		t_AttributeDescriptions[1].binding = 0;
		t_AttributeDescriptions[1].location = 1;
		t_AttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		t_AttributeDescriptions[1].offset = offsetof(Vertex, color);

		return t_AttributeDescriptions;
	}
#pragma endregion

}
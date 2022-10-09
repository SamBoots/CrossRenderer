#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1002000 // Vulkan 1.2
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "VulkanInitializers.h"
#include "Storage/Hashmap.h"
#include "Storage/Slotmap.h"

#include "VulkanCommon.h"

#include <iostream>

struct VulkanBuffer
{
	VkBuffer buffer;
	VmaAllocation allocation;
};

namespace BB
{
	static FreelistAllocator_t s_VulkanAllocator{ mbSize * 2 };

	using PipelineLayoutHash = uint64_t;
	struct VulkanBackend_inst
	{
		uint32_t currentFrame = 0;
		uint32_t frameCount = 0;

		VkInstance instance;
		VkSurfaceKHR surface;

		VulkanDevice device{};
		VulkanSwapChain swapChain{};
		VmaAllocator vma{};
		Slotmap<VulkanCommandList> commandLists{ s_VulkanAllocator };
		Slotmap<VulkanPipeline> pipelines{ s_VulkanAllocator };
		Slotmap<VulkanFrameBuffer> frameBuffers{ s_VulkanAllocator };
		Slotmap<VulkanBuffer> renderBuffers{ s_VulkanAllocator };

		OL_HashMap<PipelineLayoutHash, VkPipelineLayout> pipelineLayouts{ s_VulkanAllocator };

		VulkanDebug vulkanDebug;
	};
	static VulkanBackend_inst s_VkBackendInst;
}

using namespace BB;

PipelineLayoutHash HashPipelineLayoutInfo(const VkPipelineLayoutCreateInfo& t_CreateInfo)
{
	PipelineLayoutHash t_Hash = 25;
	t_Hash ^= t_CreateInfo.pushConstantRangeCount;
	for (uint32_t i = 0; i < t_CreateInfo.pushConstantRangeCount; i++)
	{
		t_Hash ^= static_cast<PipelineLayoutHash>(t_CreateInfo.pPushConstantRanges[i].size << 13);
	}

	return t_Hash;
}

struct FramebufferAttachment
{
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkFormat format;
	VkImageSubresourceRange subresourceRange;
	VkAttachmentDescription description;
};

struct SwapchainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR* formats;
	VkPresentModeKHR* presentModes;
	uint32_t formatCount;
	uint32_t presentModeCount;
};

struct QueueFamilyIndices
{
	uint32_t graphicsFamily;
	uint32_t presentFamily;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT a_MessageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT a_MessageType,
	const VkDebugUtilsMessengerCallbackDataEXT* a_pCallbackData,
	void* a_pUserData)
{
	std::cerr << "validation layer: " << a_pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

static uint32_t FindMemoryType(uint32_t a_TypeFilter, VkMemoryPropertyFlags a_Properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(s_VkBackendInst.device.physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((a_TypeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & a_Properties) == a_Properties) {
			return i;
		}
	}

	BB_ASSERT(false, "Vulkan: Failed to find correct memory type!");
	return 0;
}

static VkDebugUtilsMessengerEXT CreateVulkanDebugMsgger(VkInstance a_Instance)
{

	auto t_CreateDebugFunc = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		a_Instance, "vkCreateDebugUtilsMessengerEXT");

	if (t_CreateDebugFunc == nullptr)
	{
		BB_WARNING(false, "Failed to get the vkCreateDebugUtilsMessengerEXT function pointer.", WarningType::HIGH);
		return 0;
	}
	auto t_DebugMessenger = VkInit::DebugUtilsMessengerCreateInfoEXT(debugCallback);
	VkDebugUtilsMessengerEXT t_ReturnDebug;

	VKASSERT(t_CreateDebugFunc(a_Instance, &t_DebugMessenger, nullptr, &t_ReturnDebug), "Vulkan: Failed to create debug messenger.");
	return t_ReturnDebug;
}

static void DestroyVulkanDebug(VkInstance a_Instance, VkDebugUtilsMessengerEXT& a_DebugMsgger)
{
	auto t_DestroyDebugFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(a_Instance, "vkDestroyDebugUtilsMessengerEXT");
	if (t_DestroyDebugFunc == nullptr)
	{
		BB_WARNING(false, "Failed to get the vkDestroyDebugUtilsMessengerEXT function pointer.", WarningType::HIGH);
	}
	t_DestroyDebugFunc(a_Instance, a_DebugMsgger, nullptr);
}

static SwapchainSupportDetails QuerySwapChainSupport(BB::Allocator a_TempAllocator, const VkSurfaceKHR a_Surface, const VkPhysicalDevice a_PhysicalDevice)
{
	SwapchainSupportDetails t_SwapDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.formatCount, nullptr);
	t_SwapDetails.formats = BBnewArr(a_TempAllocator, t_SwapDetails.formatCount, VkSurfaceFormatKHR);
	vkGetPhysicalDeviceSurfaceFormatsKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.formatCount, t_SwapDetails.formats);

	vkGetPhysicalDeviceSurfacePresentModesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.presentModeCount, nullptr);
	t_SwapDetails.presentModes = BBnewArr(a_TempAllocator, t_SwapDetails.presentModeCount, VkPresentModeKHR);
	vkGetPhysicalDeviceSurfacePresentModesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.presentModeCount, t_SwapDetails.presentModes);

	return t_SwapDetails;
}

static bool CheckExtensionSupport(BB::Allocator a_TempAllocator, BB::Slice<const char*> a_Extensions)
{
	// check extensions if they are available.
	uint32_t t_ExtensionCount;
	vkEnumerateInstanceExtensionProperties(nullptr, &t_ExtensionCount, nullptr);
	VkExtensionProperties* t_Extensions = BBnewArr(a_TempAllocator, t_ExtensionCount, VkExtensionProperties);
	vkEnumerateInstanceExtensionProperties(nullptr, &t_ExtensionCount, t_Extensions);

	for (auto t_It = a_Extensions.begin(); t_It < a_Extensions.end(); t_It++)
	{
		for (size_t i = 0; i < t_ExtensionCount; i++)
		{

			if (strcmp(*t_It, t_Extensions[i].extensionName) == 0)
				break;

			if (t_It == a_Extensions.end())
				return false;
		}
	}
	return true;
}

static bool CheckValidationLayerSupport(BB::Allocator a_TempAllocator, const BB::Slice<const char*> a_Layers)
{
	// check layers if they are available.
	uint32_t t_LayerCount;
	vkEnumerateInstanceLayerProperties(&t_LayerCount, nullptr);
	VkLayerProperties* t_Layers = BBnewArr(a_TempAllocator, t_LayerCount, VkLayerProperties);
	vkEnumerateInstanceLayerProperties(&t_LayerCount, t_Layers);

	for (auto t_It = a_Layers.begin(); t_It < a_Layers.end(); t_It++)
	{
		for (size_t i = 0; i < t_LayerCount; i++)
		{

			if (strcmp(*t_It, t_Layers[i].layerName) == 0)
				break;

			if (t_It == a_Layers.end())
				return false;
		}
	}
	return true;
}

//If a_Index is nullptr it will just check if we have a queue that has a graphics bit.
static bool QueueFindGraphicsBit(Allocator a_TempAllocator, VkPhysicalDevice a_PhysicalDevice, uint32_t* a_Index)
{
	uint32_t t_QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, nullptr);

	VkQueueFamilyProperties* t_QueueFamilies = BBnewArr(a_TempAllocator, t_QueueFamilyCount, VkQueueFamilyProperties);
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, t_QueueFamilies);

	for (uint32_t i = 0; i < t_QueueFamilyCount; i++)
	{
		if (t_QueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			if (a_Index != nullptr)
			{
				*a_Index = i;
			}
			return true;
		}
	}
	if (a_Index != nullptr)
	{
		*a_Index = EMPTY_FAMILY_INDICES;
	}
	return false;
}

//If a_Index is nullptr it will just check if we have a queue that is available.
static bool QueueHasPresentSupport(Allocator a_TempAllocator, VkPhysicalDevice a_PhysicalDevice, VkSurfaceKHR a_Surface, uint32_t* a_Index)
{
	uint32_t t_QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, nullptr);
	VkQueueFamilyProperties* t_QueueFamilies = BBnewArr(a_TempAllocator, t_QueueFamilyCount, VkQueueFamilyProperties);
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, t_QueueFamilies);

	for (uint32_t i = 0; i < t_QueueFamilyCount; i++)
	{
		VkBool32 t_PresentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(a_PhysicalDevice, i, a_Surface, &t_PresentSupport);
		if (t_PresentSupport == VK_TRUE)
		{
			if (a_Index != nullptr)
			{
				*a_Index = i;
			}
			return true;
		}
	}
	if (a_Index != nullptr)
	{
		*a_Index = EMPTY_FAMILY_INDICES;
	}
	return false;
}

static VkPhysicalDevice FindPhysicalDevice(Allocator a_TempAllocator, const VkInstance a_Instance, const VkSurfaceKHR a_Surface)
{
	uint32_t t_DeviceCount = 0;
	vkEnumeratePhysicalDevices(a_Instance, &t_DeviceCount, nullptr);
	BB_ASSERT(t_DeviceCount != 0, "Failed to find any GPU's with vulkan support.");
	VkPhysicalDevice* t_PhysicalDevices = BBnewArr(a_TempAllocator, t_DeviceCount, VkPhysicalDevice);
	vkEnumeratePhysicalDevices(a_Instance, &t_DeviceCount, t_PhysicalDevices);

	for (uint32_t i = 0; i < t_DeviceCount; i++)
	{
		VkPhysicalDeviceProperties t_DeviceProperties;
		VkPhysicalDeviceFeatures t_DeviceFeatures;
		vkGetPhysicalDeviceProperties(t_PhysicalDevices[i], &t_DeviceProperties);
		vkGetPhysicalDeviceFeatures(t_PhysicalDevices[i], &t_DeviceFeatures);

		SwapchainSupportDetails t_SwapChainDetails = QuerySwapChainSupport(a_TempAllocator, a_Surface, t_PhysicalDevices[i]);

		if (t_DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			t_DeviceFeatures.geometryShader &&
			QueueFindGraphicsBit(a_TempAllocator, t_PhysicalDevices[i], nullptr) &&
			t_SwapChainDetails.formatCount != 0 &&
			t_SwapChainDetails.presentModeCount != 0)
		{
			return t_PhysicalDevices[i];
		}
	}

	BB_ASSERT(false, "Failed to find a suitable GPU that is discrete and has a geometry shader.");
	return VK_NULL_HANDLE;
}

static VkDevice CreateLogicalDevice(Allocator a_TempAllocator, const BB::Slice<const char*>& a_DeviceExtensions, VkPhysicalDevice a_PhysicalDevice, VkQueue* a_GraphicsQueue)
{
	VkDevice t_ReturnDevice;

	uint32_t t_Indices;
	QueueFindGraphicsBit(a_TempAllocator, a_PhysicalDevice, &t_Indices);

	VkDeviceQueueCreateInfo t_QueueCreateInfo{};
	t_QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	t_QueueCreateInfo.queueFamilyIndex = t_Indices;
	t_QueueCreateInfo.queueCount = 1;
	float t_QueuePriority = 1.0f;
	t_QueueCreateInfo.pQueuePriorities = &t_QueuePriority;


	VkPhysicalDeviceFeatures t_DeviceFeatures{};
	VkDeviceCreateInfo t_CreateInfo{};
	t_CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	t_CreateInfo.pQueueCreateInfos = &t_QueueCreateInfo;
	t_CreateInfo.queueCreateInfoCount = 1;
	t_CreateInfo.pEnabledFeatures = &t_DeviceFeatures;

	t_CreateInfo.ppEnabledExtensionNames = a_DeviceExtensions.data();
	t_CreateInfo.enabledExtensionCount = static_cast<uint32_t>(a_DeviceExtensions.size());

	VKASSERT(vkCreateDevice(a_PhysicalDevice, &t_CreateInfo, nullptr, &t_ReturnDevice),
		"Failed to create logical device Vulkan.");

	vkGetDeviceQueue(t_ReturnDevice, t_Indices, 0, a_GraphicsQueue);

	return t_ReturnDevice;
}

static VkSurfaceFormatKHR ChooseSurfaceFormat(VkSurfaceFormatKHR* a_Formats, size_t a_FormatCount)
{
	for (size_t i = 0; i < a_FormatCount; i++)
	{
		if (a_Formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
			a_Formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return a_Formats[i];
		}
	}

	BB_WARNING(false, "Vulkan: Found no optimized SurfaceFormat, choosing the first one that is available now.", WarningType::MEDIUM);
	return a_Formats[0];
}

static VkPresentModeKHR ChoosePresentMode(VkPresentModeKHR* a_Modes, size_t a_ModeCount)
{
	for (size_t i = 0; i < a_ModeCount; i++)
	{
		if (a_Modes[i] = VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}

	BB_WARNING(false, "Vulkan: Found no optimized Presentmode, choosing VK_PRESENT_MODE_FIFO_KHR.", WarningType::LOW);
	return VK_PRESENT_MODE_FIFO_KHR;
}

static void CreateImageViews(VkImageView* a_pView, const VkImage* a_Images, VkDevice a_Device, VkFormat a_Format, uint32_t a_ImageViewCount)
{
	VkImageViewCreateInfo t_ImageViewCreateInfo{};
	t_ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	t_ImageViewCreateInfo.format = a_Format;
	t_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	t_ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	t_ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	t_ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	t_ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	t_ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	t_ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	t_ImageViewCreateInfo.subresourceRange.levelCount = 1;
	t_ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	t_ImageViewCreateInfo.subresourceRange.layerCount = 1;

	for (uint32_t i = 0; i < s_VkBackendInst.frameCount; i++)
	{
		t_ImageViewCreateInfo.image = a_Images[i];
		VKASSERT(vkCreateImageView(a_Device,
			&t_ImageViewCreateInfo,
			nullptr,
			&a_pView[i]),
			"Vulkan: Failed to create swapchain image views.");
	}
}

static void CreateSwapchain(VulkanSwapChain& a_SwapChain, BB::Allocator a_TempAllocator, VkSurfaceKHR a_Surface, VkPhysicalDevice a_PhysicalDevice, VkDevice a_Device, uint32_t t_SurfaceWidth, uint32_t t_SurfaceHeight, bool a_Recreate = false)
{
	SwapchainSupportDetails t_SwapchainDetails = QuerySwapChainSupport(a_TempAllocator, a_Surface, a_PhysicalDevice);

	VkSurfaceFormatKHR t_ChosenFormat = ChooseSurfaceFormat(t_SwapchainDetails.formats, t_SwapchainDetails.formatCount);
	VkPresentModeKHR t_ChosenPresentMode = ChoosePresentMode(t_SwapchainDetails.presentModes, t_SwapchainDetails.presentModeCount);
	VkExtent2D t_ChosenExtent;
	t_ChosenExtent.width = Math::clamp(t_SurfaceWidth,
		t_SwapchainDetails.capabilities.minImageExtent.width,
		t_SwapchainDetails.capabilities.maxImageExtent.width);
	t_ChosenExtent.height = Math::clamp(t_SurfaceHeight,
		t_SwapchainDetails.capabilities.minImageExtent.height,
		t_SwapchainDetails.capabilities.maxImageExtent.height);
	a_SwapChain.extent = t_ChosenExtent;

	uint32_t t_GraphicFamily, t_PresentFamily;
	QueueFindGraphicsBit(a_TempAllocator, a_PhysicalDevice, &t_GraphicFamily);
	QueueHasPresentSupport(a_TempAllocator, a_PhysicalDevice, a_Surface, &t_PresentFamily);
	uint32_t t_QueueFamilyIndices[] = { t_GraphicFamily, t_PresentFamily };

	VkSwapchainCreateInfoKHR t_SwapCreateInfo{};
	t_SwapCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	t_SwapCreateInfo.surface = a_Surface;
	t_SwapCreateInfo.imageFormat = t_ChosenFormat.format;
	t_SwapCreateInfo.imageColorSpace = t_ChosenFormat.colorSpace;
	t_SwapCreateInfo.imageExtent = t_ChosenExtent;
	t_SwapCreateInfo.imageArrayLayers = 1;
	t_SwapCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	t_SwapCreateInfo.preTransform = t_SwapchainDetails.capabilities.currentTransform;
	t_SwapCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	t_SwapCreateInfo.presentMode = t_ChosenPresentMode;
	t_SwapCreateInfo.clipped = VK_TRUE;

	if (t_GraphicFamily != t_PresentFamily)
	{
		t_SwapCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		t_SwapCreateInfo.queueFamilyIndexCount = 2;
		t_SwapCreateInfo.pQueueFamilyIndices = t_QueueFamilyIndices;
	}
	else
	{
		t_SwapCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		t_SwapCreateInfo.queueFamilyIndexCount = 0;
		t_SwapCreateInfo.pQueueFamilyIndices = nullptr;
	}

	if (!a_Recreate)
	{ 
		a_SwapChain.imageFormat = t_ChosenFormat.format;
		//Don't recreate so we have no old swapchain.
		t_SwapCreateInfo.oldSwapchain = VK_NULL_HANDLE;

		//Now create the swapchain and set the framecount.
		s_VkBackendInst.frameCount = t_SwapchainDetails.capabilities.minImageCount + 1;
		t_SwapCreateInfo.minImageCount = t_SwapchainDetails.capabilities.minImageCount + 1;
		if (t_SwapchainDetails.capabilities.maxImageCount > 0 && s_VkBackendInst.frameCount >
			t_SwapchainDetails.capabilities.maxImageCount)
		{
			s_VkBackendInst.frameCount = t_SwapchainDetails.capabilities.maxImageCount;
			t_SwapCreateInfo.minImageCount = t_SwapchainDetails.capabilities.maxImageCount;
		}

		VKASSERT(vkCreateSwapchainKHR(a_Device, &t_SwapCreateInfo, nullptr, &a_SwapChain.swapChain), "Vulkan: Failed to create swapchain.");

		vkGetSwapchainImagesKHR(a_Device, a_SwapChain.swapChain, &s_VkBackendInst.frameCount, nullptr);
		a_SwapChain.images = BBnewArr(s_VulkanAllocator, s_VkBackendInst.frameCount, VkImage);
		a_SwapChain.imageViews = BBnewArr(s_VulkanAllocator, s_VkBackendInst.frameCount, VkImageView);
		vkGetSwapchainImagesKHR(a_Device, a_SwapChain.swapChain, &s_VkBackendInst.frameCount, a_SwapChain.images);

		//Create sync structures in the same loop, might be moved to commandlist.
		VkFenceCreateInfo t_FenceCreateInfo = VkInit::FenceCreationInfo();
		//first one is already signaled to make sure we can still render.
		t_FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		VkSemaphoreCreateInfo t_SemCreateInfo = VkInit::SemaphoreCreationInfo();
		a_SwapChain.frameFences = BBnewArr(s_VulkanAllocator, s_VkBackendInst.frameCount, VkFence);
		a_SwapChain.presentSems = BBnewArr(s_VulkanAllocator, s_VkBackendInst.frameCount, VkSemaphore);
		a_SwapChain.renderSems = BBnewArr(s_VulkanAllocator, s_VkBackendInst.frameCount, VkSemaphore);

		CreateImageViews(a_SwapChain.imageViews,
			a_SwapChain.images,
			s_VkBackendInst.device.logicalDevice,
			a_SwapChain.imageFormat,
			s_VkBackendInst.frameCount);

		for (uint32_t i = 0; i < s_VkBackendInst.frameCount; i++)
		{
			VKASSERT(vkCreateFence(a_Device,
				&t_FenceCreateInfo,
				nullptr,
				&a_SwapChain.frameFences[i]),
				"Vulkan: Failed to create fence.");
			VKASSERT(vkCreateSemaphore(a_Device,
				&t_SemCreateInfo,
				nullptr,
				&a_SwapChain.presentSems[i]),
				"Vulkan: Failed to create present semaphore.");
			VKASSERT(vkCreateSemaphore(a_Device,
				&t_SemCreateInfo,
				nullptr,
				&a_SwapChain.renderSems[i]),
				"Vulkan: Failed to create render semaphore.");
		}
	}
	else //Or recreate the swapchain.
	{
		t_SwapCreateInfo.oldSwapchain = a_SwapChain.swapChain;
		t_SwapCreateInfo.imageExtent = a_SwapChain.extent;
		t_SwapCreateInfo.minImageCount = s_VkBackendInst.frameCount;

		VKASSERT(vkCreateSwapchainKHR(a_Device, &t_SwapCreateInfo, nullptr, &a_SwapChain.swapChain), "Vulkan: Failed to create swapchain.");
		vkGetSwapchainImagesKHR(a_Device, a_SwapChain.swapChain, &s_VkBackendInst.frameCount, a_SwapChain.images);

		CreateImageViews(a_SwapChain.imageViews,
			a_SwapChain.images,
			s_VkBackendInst.device.logicalDevice,
			a_SwapChain.imageFormat,
			s_VkBackendInst.frameCount);
	}
}

struct VulkanShaderResult
{
	VkShaderModule* shaderModules;
	VkPipelineShaderStageCreateInfo* pipelineShaderStageInfo;
};

//Creates VkPipelineShaderStageCreateInfo equal to the amount of ShaderCreateInfos in a_CreateInfo.
static VulkanShaderResult CreateShaderModules(Allocator a_TempAllocator, VkDevice a_Device, const Slice<BB::ShaderCreateInfo> a_CreateInfo)
{
	VulkanShaderResult t_ReturnResult;

	t_ReturnResult.pipelineShaderStageInfo = BBnewArr(a_TempAllocator, a_CreateInfo.size(), VkPipelineShaderStageCreateInfo);
	t_ReturnResult.shaderModules = BBnewArr(a_TempAllocator, a_CreateInfo.size(), VkShaderModule);

	VkShaderModuleCreateInfo t_ShaderModCreateInfo{};
	t_ShaderModCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	for (size_t i = 0; i < a_CreateInfo.size(); i++)
	{
		t_ShaderModCreateInfo.codeSize = a_CreateInfo[i].buffer.size;
		t_ShaderModCreateInfo.pCode = reinterpret_cast<const uint32_t*>(a_CreateInfo[i].buffer.data);

		VKASSERT(vkCreateShaderModule(a_Device, &t_ShaderModCreateInfo, nullptr, &t_ReturnResult.shaderModules[i]),
			"Vulkan: Failed to create shadermodule.");

		t_ReturnResult.pipelineShaderStageInfo[i] = VkInit::PipelineShaderStageCreateInfo(
			VKConv::ShaderStageBits(a_CreateInfo[i].shaderStage),
			t_ReturnResult.shaderModules[i],
			"main",
			nullptr);
	}

	return t_ReturnResult;
}

static VkPipelineLayout CreatePipelineLayout(const Slice<VkPushConstantRange> a_PushConstants)
{
	const VkPipelineLayoutCreateInfo t_LayoutCreateInfo = VkInit::PipelineLayoutCreateInfo(
		0,
		nullptr,
		static_cast<uint32_t>(a_PushConstants.size()),
		a_PushConstants.data()
	);

	PipelineLayoutHash t_DescriptorHash = HashPipelineLayoutInfo(t_LayoutCreateInfo);
	VkPipelineLayout* t_FoundLayout = s_VkBackendInst.pipelineLayouts.find(t_DescriptorHash);

	if (t_FoundLayout != nullptr)
		return *t_FoundLayout;

	VkPipelineLayout t_NewLayout = VK_NULL_HANDLE;
	VKASSERT(vkCreatePipelineLayout(s_VkBackendInst.device.logicalDevice,
		&t_LayoutCreateInfo,
		nullptr,
		&t_NewLayout),
		"Vulkan: Failed to create pipelinelayout.");

	s_VkBackendInst.pipelineLayouts.insert(t_DescriptorHash, t_NewLayout);

	return t_NewLayout;
}

static void CreateFrameBuffers(VkFramebuffer* a_FrameBuffers, VkRenderPass a_RenderPass, uint32_t a_Width, uint32_t a_Height, uint32_t a_FramebufferCount)
{
	uint32_t t_UsedAttachments = 1;
	//have enough space for the potentional depth buffer.
	VkImageView t_AttachmentViews[2];

	for (uint32_t i = 0; i < a_FramebufferCount; i++)
	{
		t_AttachmentViews[0] = s_VkBackendInst.swapChain.imageViews[i];

		VkFramebufferCreateInfo t_FramebufferInfo = VkInit::FramebufferCreateInfo();
		t_FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		t_FramebufferInfo.renderPass = a_RenderPass;
		t_FramebufferInfo.attachmentCount = t_UsedAttachments;
		t_FramebufferInfo.pAttachments = t_AttachmentViews;
		t_FramebufferInfo.width = a_Width;
		t_FramebufferInfo.height = a_Height;
		t_FramebufferInfo.layers = 1;

		VKASSERT(vkCreateFramebuffer(s_VkBackendInst.device.logicalDevice,
			&t_FramebufferInfo,
			nullptr,
			&a_FrameBuffers[i]),
			"Vulkan: Failed to create Framebuffer");
	}
}

RBufferHandle BB::VulkanCreateBuffer(const RenderBufferCreateInfo& a_Info)
{
	VulkanBuffer t_Buffer;
	
	VkBufferUsageFlags t_Usage = VKConv::RenderBufferUsage(a_Info.usage);
	VkMemoryPropertyFlags t_MemoryProperties = VKConv::MemoryPropertyFlags(a_Info.memProperties);

	VkBufferCreateInfo t_BufferInfo{};
	t_BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	t_BufferInfo.size = a_Info.size;
	t_BufferInfo.usage = t_Usage;
	t_BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo t_VmaAlloc{};
	t_VmaAlloc.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	VKASSERT(vmaCreateBuffer(s_VkBackendInst.vma, 
		&t_BufferInfo, &t_VmaAlloc,
		&t_Buffer.buffer, &t_Buffer.allocation,
		nullptr), "Vulkan::VMA, Failed to allocate memory");

	return RBufferHandle(s_VkBackendInst.renderBuffers.insert(t_Buffer));
}

void BB::VulkanDestroyBuffer(RBufferHandle a_Handle)
{
	VulkanBuffer& t_Buffer = s_VkBackendInst.renderBuffers.find(a_Handle.handle);
	vmaDestroyBuffer(s_VkBackendInst.vma, t_Buffer.buffer, t_Buffer.allocation);
	s_VkBackendInst.renderBuffers.erase(a_Handle.handle);
}

void BB::VulkanBufferCopyData(RBufferHandle a_Handle, const void* a_Data)
{
	VulkanBuffer& t_Buffer = s_VkBackendInst.renderBuffers.find(a_Handle.handle);
	void* t_MapData;
	VKASSERT(vmaMapMemory(s_VkBackendInst.vma,
		t_Buffer.allocation,
		&t_MapData),
		"Vulkan: Failed to map memory");
	memcpy(t_MapData, a_Data, t_Buffer.allocation->GetSize());
	vmaUnmapMemory(s_VkBackendInst.vma, t_Buffer.allocation);
}

void BB::VulkanBufferCopyData(RBufferHandle a_Handle, const void* a_Data, RDeviceBufferView a_View)
{
	VulkanBuffer& t_Buffer = s_VkBackendInst.renderBuffers.find(a_Handle.handle);
	void* t_MapData;
	VKASSERT(vmaMapMemory(s_VkBackendInst.vma,
		t_Buffer.allocation,
		&t_MapData),
		"Vulkan: Failed to map memory");
	memcpy(t_MapData, a_Data, t_Buffer.allocation->GetSize());
	vmaUnmapMemory(s_VkBackendInst.vma, t_Buffer.allocation);
}


void BB::RenderFrame(Allocator a_TempAllocator, CommandListHandle a_CommandHandle, FrameBufferHandle a_FrameBufferHandle, PipelineHandle a_PipeHandle)
{
	uint32_t t_CurrentFrame = s_VkBackendInst.currentFrame;

	VulkanCommandList::GraphicsCommands& t_Cmdlist =
		s_VkBackendInst.commandLists[a_CommandHandle.handle].graphicCommands[t_CurrentFrame];
	VulkanFrameBuffer& t_FrameBuffer = s_VkBackendInst.frameBuffers[a_FrameBufferHandle.handle];

	uint32_t t_ImageIndex;
	VKASSERT(vkAcquireNextImageKHR(s_VkBackendInst.device.logicalDevice,
		s_VkBackendInst.swapChain.swapChain,
		UINT64_MAX,
		s_VkBackendInst.swapChain.renderSems[t_CurrentFrame],
		VK_NULL_HANDLE,
		&t_ImageIndex),
		"Vulkan: failed to get next image.");

	VKASSERT(vkWaitForFences(s_VkBackendInst.device.logicalDevice,
		1,
		&s_VkBackendInst.swapChain.frameFences[t_CurrentFrame],
		VK_TRUE,
		UINT64_MAX),
		"Vulkan: Failed to wait for frences");

	vkResetFences(s_VkBackendInst.device.logicalDevice,
		1,
		&s_VkBackendInst.swapChain.frameFences[t_CurrentFrame]);

	//vkResetCommandBuffer(a_CmdList.buffers[a_CmdList.currentFree], 0);
	VkCommandBufferBeginInfo t_CmdBeginInfo = VkInit::CommandBufferBeginInfo(nullptr);
	VKASSERT(vkBeginCommandBuffer(t_Cmdlist.buffers[t_Cmdlist.currentFree],
		&t_CmdBeginInfo),
		"Vulkan: Failed to begin commandbuffer");

	VkCommandBuffer t_CmdRecording = t_Cmdlist.buffers[t_Cmdlist.currentFree];

	VkClearValue t_ClearValue = { {{0.0f, 1.0f, 0.0f, 1.0f}} };

	VkRenderPassBeginInfo t_RenderPassBegin = VkInit::RenderPassBeginInfo(
		t_FrameBuffer.renderPass,
		t_FrameBuffer.frameBuffers[t_ImageIndex],
		VkInit::Rect2D(0,
			0,
			s_VkBackendInst.swapChain.extent),
		1,
		&t_ClearValue);

	vkCmdBeginRenderPass(t_CmdRecording,
		&t_RenderPassBegin,
		VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(t_CmdRecording,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		s_VkBackendInst.pipelines[a_PipeHandle.handle].pipeline);

	VkViewport t_Viewport{};
	t_Viewport.x = 0.0f;
	t_Viewport.y = 0.0f;
	t_Viewport.width = static_cast<float>(s_VkBackendInst.swapChain.extent.width);
	t_Viewport.height = static_cast<float>(s_VkBackendInst.swapChain.extent.height);
	t_Viewport.minDepth = 0.0f;
	t_Viewport.maxDepth = 1.0f;
	vkCmdSetViewport(t_CmdRecording, 0, 1, &t_Viewport);

	VkRect2D t_Scissor{};
	t_Scissor.offset = { 0, 0 };
	t_Scissor.extent = s_VkBackendInst.swapChain.extent;
	vkCmdSetScissor(t_CmdRecording, 0, 1, &t_Scissor);

	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(t_CmdRecording, 0, 1,
		&s_VkBackendInst.renderBuffers.find(0).buffer,
		offsets);

	vkCmdDraw(t_CmdRecording, 3, 1, 0, 0);

	vkCmdEndRenderPass(t_CmdRecording);

	VKASSERT(vkEndCommandBuffer(t_CmdRecording), "Vulkan: Failed to record commandbuffer!");


	//submit stage.
	VkPipelineStageFlags t_WaitStagesMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkSubmitInfo t_SubmitInfo{};
	t_SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	t_SubmitInfo.waitSemaphoreCount = 1;
	t_SubmitInfo.pWaitSemaphores = &s_VkBackendInst.swapChain.renderSems[t_CurrentFrame];
	t_SubmitInfo.pWaitDstStageMask = &t_WaitStagesMask;

	t_SubmitInfo.signalSemaphoreCount = 1;
	t_SubmitInfo.pSignalSemaphores = &s_VkBackendInst.swapChain.presentSems[t_CurrentFrame];

	t_SubmitInfo.commandBufferCount = 1;
	t_SubmitInfo.pCommandBuffers = &t_CmdRecording;

	VKASSERT(vkQueueSubmit(s_VkBackendInst.device.graphicsQueue,
		1,
		&t_SubmitInfo,
		s_VkBackendInst.swapChain.frameFences[t_CurrentFrame]),
		"Vulkan: failed to submit to queue.");

	VkPresentInfoKHR t_PresentInfo{};
	t_PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	t_PresentInfo.waitSemaphoreCount = 1;
	t_PresentInfo.pWaitSemaphores = &s_VkBackendInst.swapChain.presentSems[t_CurrentFrame];
	t_PresentInfo.swapchainCount = 1;
	t_PresentInfo.pSwapchains = &s_VkBackendInst.swapChain.swapChain;
	t_PresentInfo.pImageIndices = &t_ImageIndex;
	t_PresentInfo.pResults = nullptr;

	VKASSERT(vkQueuePresentKHR(s_VkBackendInst.device.presentQueue, &t_PresentInfo),
		"Vulkan: Failed to queuepresentKHR.");

	s_VkBackendInst.currentFrame = (s_VkBackendInst.currentFrame + 1) % s_VkBackendInst.frameCount;
}

APIRenderBackend BB::VulkanCreateBackend(Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo)
{
	VKConv::ExtensionResult t_InstanceExtensions = VKConv::TranslateExtensions(
		a_TempAllocator,
		a_CreateInfo.extensions);
	VKConv::ExtensionResult t_DeviceExtensions = VKConv::TranslateExtensions(
		a_TempAllocator,
		a_CreateInfo.deviceExtensions);

	//Check if the extensions and layers work.
	BB_ASSERT(CheckExtensionSupport(a_TempAllocator,
		BB::Slice(t_InstanceExtensions.extensions, t_InstanceExtensions.count)),
		"Vulkan: extension(s) not supported.");

#ifdef _DEBUG
	//For debug, we want to remember the extensions we have.
	s_VkBackendInst.vulkanDebug.extensions = BBnewArr(s_VulkanAllocator, t_InstanceExtensions.count, const char*);
	s_VkBackendInst.vulkanDebug.extensionCount = t_InstanceExtensions.count;
	for (size_t i = 0; i < s_VkBackendInst.vulkanDebug.extensionCount; i++)
	{
		s_VkBackendInst.vulkanDebug.extensions[i] = t_InstanceExtensions.extensions[i];
	}
#endif //_DEBUG
	{
		VkApplicationInfo t_AppInfo{};
		t_AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		t_AppInfo.pApplicationName = a_CreateInfo.appName;
		t_AppInfo.pEngineName = a_CreateInfo.engineName;
		t_AppInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, a_CreateInfo.version, 0);
		t_AppInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, a_CreateInfo.version, 0);
		t_AppInfo.apiVersion = VK_MAKE_API_VERSION(0, 1, a_CreateInfo.version, 0);

		VkInstanceCreateInfo t_InstanceCreateInfo{};
		t_InstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		t_InstanceCreateInfo.pApplicationInfo = &t_AppInfo;

		VkDebugUtilsMessengerCreateInfoEXT t_DebugCreateInfo;
		if (a_CreateInfo.validationLayers)
		{
			const char* validationLayer = "VK_LAYER_KHRONOS_validation";
			BB_WARNING(CheckValidationLayerSupport(a_TempAllocator, Slice(&validationLayer, 1)), "Vulkan: Validation layer(s) not available.", WarningType::MEDIUM);
			t_DebugCreateInfo = VkInit::DebugUtilsMessengerCreateInfoEXT(debugCallback);
			t_InstanceCreateInfo.ppEnabledLayerNames = &validationLayer;
			t_InstanceCreateInfo.enabledLayerCount = 1;
			t_InstanceCreateInfo.pNext = &t_DebugCreateInfo;
		}
		else
		{
			t_InstanceCreateInfo.ppEnabledLayerNames = nullptr;
			t_InstanceCreateInfo.enabledLayerCount = 0;
		}
		t_InstanceCreateInfo.ppEnabledExtensionNames = t_InstanceExtensions.extensions;
		t_InstanceCreateInfo.enabledExtensionCount = t_InstanceExtensions.count;

		VKASSERT(vkCreateInstance(&t_InstanceCreateInfo,
			nullptr,
			&s_VkBackendInst.instance), "Failed to create Vulkan Instance!");

		if (a_CreateInfo.validationLayers)
		{
			s_VkBackendInst.vulkanDebug.debugMessenger = CreateVulkanDebugMsgger(s_VkBackendInst.instance);
		}
		else
		{
			s_VkBackendInst.vulkanDebug.debugMessenger = 0;
		}
	}

	{
		//Surface
		VkWin32SurfaceCreateInfoKHR t_SurfaceCreateInfo{};
		t_SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		t_SurfaceCreateInfo.hwnd = a_CreateInfo.hwnd;
		t_SurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
		VKASSERT(vkCreateWin32SurfaceKHR(s_VkBackendInst.instance,
			&t_SurfaceCreateInfo, nullptr,
			&s_VkBackendInst.surface),
			"Failed to create Win32 vulkan surface.");
	}

	//Get the physical Device
	s_VkBackendInst.device.physicalDevice = FindPhysicalDevice(a_TempAllocator,
		s_VkBackendInst.instance,
		s_VkBackendInst.surface);
	//Get the logical device and the graphics queue.
	s_VkBackendInst.device.logicalDevice = CreateLogicalDevice(a_TempAllocator,
		BB::Slice(t_DeviceExtensions.extensions, t_DeviceExtensions.count),
		s_VkBackendInst.device.physicalDevice,
		&s_VkBackendInst.device.graphicsQueue);

	{
		uint32_t t_PresentIndex;
		if (QueueHasPresentSupport(a_TempAllocator,
			s_VkBackendInst.device.physicalDevice,
			s_VkBackendInst.surface,
			&t_PresentIndex))
		{
			vkGetDeviceQueue(s_VkBackendInst.device.logicalDevice,
				t_PresentIndex,
				0,
				&s_VkBackendInst.device.presentQueue);
		}
	}
	CreateSwapchain(s_VkBackendInst.swapChain, 
		a_TempAllocator,
		s_VkBackendInst.surface,
		s_VkBackendInst.device.physicalDevice,
		s_VkBackendInst.device.logicalDevice,
		a_CreateInfo.windowWidth,
		a_CreateInfo.windowHeight);

	//Setup the Vulkan Memory Allocator
	VmaVulkanFunctions t_VkFunctions{};
	t_VkFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	t_VkFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo t_AllocatorCreateInfo = {};
	t_AllocatorCreateInfo.vulkanApiVersion = VK_MAKE_API_VERSION(0, 1, a_CreateInfo.version, 0);
	t_AllocatorCreateInfo.physicalDevice = s_VkBackendInst.device.physicalDevice;
	t_AllocatorCreateInfo.device = s_VkBackendInst.device.logicalDevice;
	t_AllocatorCreateInfo.instance = s_VkBackendInst.instance;
	t_AllocatorCreateInfo.pVulkanFunctions = &t_VkFunctions;

	vmaCreateAllocator(&t_AllocatorCreateInfo, &s_VkBackendInst.vma);

	//The backend handle is not that important which number it is. But we will make it 1.
	return APIRenderBackend(1);
}

FrameBufferHandle BB::VulkanCreateFrameBuffer(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo)
{
	VulkanFrameBuffer t_ReturnFrameBuffer;
	{
		//First do the renderpass
		VkAttachmentDescription t_ColorAttachment = VkInit::AttachmentDescription(
			s_VkBackendInst.swapChain.imageFormat,
			VK_SAMPLE_COUNT_1_BIT,
			VKConv::LoadOP(a_FramebufferCreateInfo.colorLoadOp),
			VKConv::StoreOp(a_FramebufferCreateInfo.colorStoreOp),
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VKConv::ImageLayout(a_FramebufferCreateInfo.colorInitialLayout),
			VKConv::ImageLayout(a_FramebufferCreateInfo.colorFinalLayout));
		VkAttachmentReference t_ColorAttachmentRef = VkInit::AttachmentReference(
			0,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);


		VkSubpassDescription t_Subpass = VkInit::SubpassDescription(
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			1,
			&t_ColorAttachmentRef,
			nullptr);
		VkSubpassDependency t_Dependency = VkInit::SubpassDependancy(
			VK_SUBPASS_EXTERNAL,
			0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

		VkRenderPassCreateInfo t_RenderPassInfo = VkInit::RenderPassCreateInfo(
			1, &t_ColorAttachment, 1, &t_Subpass, 0, nullptr);

		VKASSERT(vkCreateRenderPass(s_VkBackendInst.device.logicalDevice,
			&t_RenderPassInfo,
			nullptr,
			&t_ReturnFrameBuffer.renderPass),
			"Vulkan: Failed to create graphics Pipeline.");
	}

	{
		t_ReturnFrameBuffer.width = a_FramebufferCreateInfo.width;
		t_ReturnFrameBuffer.height = a_FramebufferCreateInfo.height;
		t_ReturnFrameBuffer.frameBufferCount = s_VkBackendInst.frameCount;
		t_ReturnFrameBuffer.frameBuffers = BBnewArr(s_VulkanAllocator, t_ReturnFrameBuffer.frameBufferCount, VkFramebuffer);
		CreateFrameBuffers(
			t_ReturnFrameBuffer.frameBuffers,
			t_ReturnFrameBuffer.renderPass,
			t_ReturnFrameBuffer.width,
			t_ReturnFrameBuffer.height,
			t_ReturnFrameBuffer.frameBufferCount
			);
	}

	return FrameBufferHandle(s_VkBackendInst.frameBuffers.emplace(t_ReturnFrameBuffer));
}

PipelineHandle BB::VulkanCreatePipeline(Allocator a_TempAllocator, const RenderPipelineCreateInfo& a_CreateInfo)
{
	VulkanPipeline t_ReturnPipeline;

	//Get dynamic state for the viewport and scissor.
	VkDynamicState t_DynamicStates[2]{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo t_DynamicPipeCreateInfo{};
	t_DynamicPipeCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	t_DynamicPipeCreateInfo.dynamicStateCount = 2;
	t_DynamicPipeCreateInfo.pDynamicStates = t_DynamicStates;

	VulkanShaderResult t_ShaderCreateResult = CreateShaderModules(a_TempAllocator,
		s_VkBackendInst.device.logicalDevice,
		a_CreateInfo.shaderCreateInfos);

	//Set viewport to nullptr and let the commandbuffer handle it via 
	VkPipelineViewportStateCreateInfo t_ViewportState = VkInit::PipelineViewportStateCreateInfo(
		1,
		nullptr,
		1,
		nullptr);

	auto t_BindingDescription = VertexBindingDescription();
	auto t_AttributeDescription = VertexAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo t_VertexInput = VkInit::PipelineVertexInputStateCreateInfo(
		1,
		&t_BindingDescription,
		static_cast<uint32_t>(t_AttributeDescription.size()),
		t_AttributeDescription.data());
	VkPipelineInputAssemblyStateCreateInfo t_InputAssembly = VkInit::PipelineInputAssemblyStateCreateInfo(
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE);
	VkPipelineRasterizationStateCreateInfo t_Rasterizer = VkInit::PipelineRasterizationStateCreateInfo(
		VK_FALSE,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_CLOCKWISE);
	VkPipelineMultisampleStateCreateInfo t_MultiSampling = VkInit::PipelineMultisampleStateCreateInfo(
		VK_FALSE,
		VK_SAMPLE_COUNT_1_BIT);
	VkPipelineDepthStencilStateCreateInfo t_DepthStencil = VkInit::PipelineDepthStencilStateCreateInfo(
		VK_TRUE,
		VK_TRUE,
		VK_FALSE,
		VK_FALSE,
		VK_COMPARE_OP_LESS);
	VkPipelineColorBlendAttachmentState t_ColorblendAttachment = VkInit::PipelineColorBlendAttachmentState(
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		VK_FALSE);
	VkPipelineColorBlendStateCreateInfo t_ColorBlending = VkInit::PipelineColorBlendStateCreateInfo(
		VK_FALSE, VK_LOGIC_OP_COPY, 1, &t_ColorblendAttachment);

	VkPushConstantRange t_PushConstantMatrix;
	t_PushConstantMatrix.size = 256;
	t_PushConstantMatrix.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	t_PushConstantMatrix.offset = 0;

	VkPipelineLayout t_PipeLayout = CreatePipelineLayout(BB::Slice<VkPushConstantRange>());

	VkGraphicsPipelineCreateInfo t_PipeCreateInfo{};
	t_PipeCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	t_PipeCreateInfo.pDynamicState = &t_DynamicPipeCreateInfo;
	t_PipeCreateInfo.pViewportState = &t_ViewportState;
	t_PipeCreateInfo.pVertexInputState = &t_VertexInput;
	t_PipeCreateInfo.pInputAssemblyState = &t_InputAssembly;
	t_PipeCreateInfo.pRasterizationState = &t_Rasterizer;
	t_PipeCreateInfo.pMultisampleState = &t_MultiSampling;
	t_PipeCreateInfo.pDepthStencilState = &t_DepthStencil;
	t_PipeCreateInfo.pColorBlendState = &t_ColorBlending;

	t_PipeCreateInfo.pStages = t_ShaderCreateResult.pipelineShaderStageInfo;
	t_PipeCreateInfo.stageCount = 2;
	t_PipeCreateInfo.layout = t_PipeLayout;
	t_PipeCreateInfo.renderPass = s_VkBackendInst.frameBuffers[a_CreateInfo.framebufferHandle.handle].renderPass;
	t_PipeCreateInfo.subpass = 0;
	//Optimalization for later.
	t_PipeCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	t_PipeCreateInfo.basePipelineIndex = -1;

	VKASSERT(vkCreateGraphicsPipelines(s_VkBackendInst.device.logicalDevice,
		VK_NULL_HANDLE,
		1,
		&t_PipeCreateInfo,
		nullptr,
		&t_ReturnPipeline.pipeline),
		"Vulkan: Failed to create graphics Pipeline.");

	vkDestroyShaderModule(s_VkBackendInst.device.logicalDevice,
		t_ShaderCreateResult.shaderModules[0],
		nullptr);
	vkDestroyShaderModule(s_VkBackendInst.device.logicalDevice,
		t_ShaderCreateResult.shaderModules[1],
		nullptr);

	return PipelineHandle(s_VkBackendInst.pipelines.emplace(t_ReturnPipeline));
}

CommandListHandle BB::VulkanCreateCommandList(Allocator a_TempAllocator, const uint32_t a_BufferCount)
{
	VulkanCommandList t_ReturnCommandList;
	uint32_t t_GraphicsBit;
	QueueFindGraphicsBit(a_TempAllocator, s_VkBackendInst.device.physicalDevice, &t_GraphicsBit);
	VkCommandPoolCreateInfo t_CommandPoolInfo = VkInit::CommandPoolCreateInfo(
		t_GraphicsBit,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	t_ReturnCommandList.graphicCommands = BBnewArr(s_VulkanAllocator, s_VkBackendInst.frameCount, VulkanCommandList::GraphicsCommands);

	//The commandlist has graphiccommands per frame.
	for (uint32_t i = 0; i < s_VkBackendInst.frameCount; i++)
	{
		VKASSERT(vkCreateCommandPool(s_VkBackendInst.device.logicalDevice,
			&t_CommandPoolInfo,
			nullptr,
			&t_ReturnCommandList.graphicCommands[i].pool),
			"Vulkan: Failed to create commandpool.");

		VkCommandBufferAllocateInfo t_AllocInfo = VkInit::CommandBufferAllocateInfo(
			t_ReturnCommandList.graphicCommands[i].pool,
			1,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		t_ReturnCommandList.graphicCommands[i].buffers = BBnewArr(s_VulkanAllocator, 1, VkCommandBuffer);
		t_ReturnCommandList.graphicCommands[i].bufferCount = 1;
		t_ReturnCommandList.graphicCommands[i].currentFree = 0;

		VKASSERT(vkAllocateCommandBuffers(s_VkBackendInst.device.logicalDevice,
			&t_AllocInfo,
			t_ReturnCommandList.graphicCommands[i].buffers),
			"Vulkan: failed to allocate commandbuffers.");
	}

	return CommandListHandle(s_VkBackendInst.commandLists.emplace(t_ReturnCommandList));
}

void BB::ResizeWindow(Allocator a_TempAllocator, APIRenderBackend, uint32_t a_X, uint32_t a_Y)
{
	VulkanWaitDeviceReady();

	//Recreate framebuffers.
	for (auto t_It = s_VkBackendInst.frameBuffers.begin();
		t_It < s_VkBackendInst.frameBuffers.end(); t_It++)
	{
		VulkanFrameBuffer& t_FrameBuffer = t_It->value;
		t_FrameBuffer.width = a_X;
		t_FrameBuffer.height = a_Y;

		for (size_t i = 0; i < t_FrameBuffer.frameBufferCount; i++)
		{
			vkDestroyFramebuffer(s_VkBackendInst.device.logicalDevice,
				t_FrameBuffer.frameBuffers[i],
				nullptr);
		}
	}

	for (size_t i = 0; i < s_VkBackendInst.frameCount; i++)
	{
		vkDestroyImageView(s_VkBackendInst.device.logicalDevice,
			s_VkBackendInst.swapChain.imageViews[i],
			nullptr);
	}

	//Creates the swapchain with the image views.
	CreateSwapchain(s_VkBackendInst.swapChain,
		a_TempAllocator,
		s_VkBackendInst.surface,
		s_VkBackendInst.device.physicalDevice,
		s_VkBackendInst.device.logicalDevice,
		a_X,
		a_Y,
		true);

	//Recreate framebuffers.
	for (auto t_It = s_VkBackendInst.frameBuffers.begin();
		t_It < s_VkBackendInst.frameBuffers.end(); t_It++)
	{
		VulkanFrameBuffer& t_FrameBuffer = t_It->value;
		CreateFrameBuffers(t_FrameBuffer.frameBuffers,
			t_FrameBuffer.renderPass,
			a_X,
			a_Y,
			t_FrameBuffer.frameBufferCount);
	}
}

void BB::VulkanWaitDeviceReady()
{
	vkDeviceWaitIdle(s_VkBackendInst.device.logicalDevice);
}

void BB::VulkanDestroyCommandList(CommandListHandle a_Handle)
{
	VulkanCommandList& a_List = s_VkBackendInst.commandLists[a_Handle.handle];

	for (uint32_t i = 0; i < s_VkBackendInst.frameCount; i++)
	{
		vkDestroyCommandPool(s_VkBackendInst.device.logicalDevice,
			a_List.graphicCommands[i].pool, nullptr);
		BBfreeArr(s_VulkanAllocator,
			a_List.graphicCommands[i].buffers);
	}
	BBfreeArr(s_VulkanAllocator,
		a_List.graphicCommands);
}

void BB::VulkanDestroyFramebuffer(FrameBufferHandle a_Handle)
{
	for (uint32_t i = 0; i < s_VkBackendInst.frameBuffers[a_Handle.handle].frameBufferCount; i++)
	{
		vkDestroyFramebuffer(s_VkBackendInst.device.logicalDevice,
			s_VkBackendInst.frameBuffers[a_Handle.handle].frameBuffers[i],
			nullptr);
	}
	BBfree(s_VulkanAllocator,
		s_VkBackendInst.frameBuffers[a_Handle.handle].frameBuffers);

	vkDestroyRenderPass(s_VkBackendInst.device.logicalDevice,
		s_VkBackendInst.frameBuffers[a_Handle.handle].renderPass,
		nullptr);
}

void BB::VulkanDestroyPipeline(PipelineHandle a_Handle)
{
	vkDestroyPipeline(s_VkBackendInst.device.logicalDevice,
		s_VkBackendInst.pipelines[a_Handle.handle].pipeline,
		nullptr);
}

void BB::VulkanDestroyBackend(APIRenderBackend)
{
	for (auto t_It = s_VkBackendInst.pipelineLayouts.begin();
		t_It < s_VkBackendInst.pipelineLayouts.end(); t_It++)
	{
		vkDestroyPipelineLayout(s_VkBackendInst.device.logicalDevice,
			*t_It->value,
			nullptr);
	}

	for (size_t i = 0; i < s_VkBackendInst.frameCount; i++)
	{
		vkDestroyImageView(s_VkBackendInst.device.logicalDevice,
			s_VkBackendInst.swapChain.imageViews[i], nullptr);
		vkDestroyFence(s_VkBackendInst.device.logicalDevice,
			s_VkBackendInst.swapChain.frameFences[i], nullptr);
		vkDestroySemaphore(s_VkBackendInst.device.logicalDevice,
			s_VkBackendInst.swapChain.presentSems[i], nullptr);
		vkDestroySemaphore(s_VkBackendInst.device.logicalDevice,
			s_VkBackendInst.swapChain.renderSems[i], nullptr);
	}

	vkDestroySwapchainKHR(s_VkBackendInst.device.logicalDevice,
		s_VkBackendInst.swapChain.swapChain,
		nullptr);
	vmaDestroyAllocator(s_VkBackendInst.vma);
	vkDestroyDevice(s_VkBackendInst.device.logicalDevice, nullptr);

	if (s_VkBackendInst.vulkanDebug.debugMessenger != 0)
		DestroyVulkanDebug(s_VkBackendInst.instance, s_VkBackendInst.vulkanDebug.debugMessenger);

	vkDestroySurfaceKHR(s_VkBackendInst.instance, s_VkBackendInst.surface, nullptr);
	vkDestroyInstance(s_VkBackendInst.instance, nullptr);

	//clear all the vulkan memory.
	s_VulkanAllocator.Clear();
}
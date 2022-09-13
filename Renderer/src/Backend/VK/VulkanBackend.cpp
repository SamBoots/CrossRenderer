#pragma once
#include "VulkanBackend.h"
#include "VulkanInitializers.h"
#include "Hashmap.h"

#include <iostream>
namespace BB
{
	using PipelineLayoutHash = uint64_t;
	struct VulkanBackend_o
	{
		OL_HashMap<PipelineLayoutHash, VkPipelineLayout>* pipelineLayouts;
	};
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
	const VkDebugUtilsMessengerCallbackDataEXT * a_pCallbackData,
	void* a_pUserData) 
{
	std::cerr << "validation layer: " << a_pCallbackData->pMessage << std::endl;
	return VK_FALSE;
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
	VkDebugUtilsMessengerEXT t_ReturnDebug;
	VKASSERT(t_CreateDebugFunc(a_Instance, &VkInit::DebugUtilsMessengerCreateInfoEXT(debugCallback), nullptr, &t_ReturnDebug), "Vulkan: Failed to create debug messenger.");
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
	t_SwapDetails.formats = BBnewArr<VkSurfaceFormatKHR>(a_TempAllocator, t_SwapDetails.formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.formatCount, t_SwapDetails.formats);

	vkGetPhysicalDeviceSurfacePresentModesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.presentModeCount, nullptr);
	t_SwapDetails.presentModes = BBnewArr<VkPresentModeKHR>(a_TempAllocator, t_SwapDetails.presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.presentModeCount, t_SwapDetails.presentModes);

	return t_SwapDetails;
}

static bool CheckExtensionSupport(BB::Allocator a_TempAllocator, BB::Slice<const char*> a_Extensions)
{
	// check extensions if they are available.
	uint32_t t_ExtensionCount;
	vkEnumerateInstanceExtensionProperties(nullptr, &t_ExtensionCount, nullptr);
	VkExtensionProperties* t_Extensions = BBnewArr<VkExtensionProperties>(a_TempAllocator, t_ExtensionCount);
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
	VkLayerProperties* t_Layers = BBnewArr<VkLayerProperties>(a_TempAllocator, t_LayerCount);
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

	VkQueueFamilyProperties* t_QueueFamilies = BBnewArr<VkQueueFamilyProperties>(a_TempAllocator, t_QueueFamilyCount);
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
	VkQueueFamilyProperties* t_QueueFamilies = BBnewArr<VkQueueFamilyProperties>(a_TempAllocator, t_QueueFamilyCount);
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
	VkPhysicalDevice* t_PhysicalDevices = BBnewArr<VkPhysicalDevice>(a_TempAllocator, t_DeviceCount);
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

static SwapChain CreateSwapchain(BB::Allocator a_SysAllocator, BB::Allocator a_TempAllocator, VkSurfaceKHR a_Surface, VkPhysicalDevice a_PhysicalDevice, VkDevice a_Device, uint32_t t_SurfaceWidth, uint32_t t_SurfaceHeight)
{
	SwapChain t_ReturnSwapchain;

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

	//Now create the swapchain.
	uint32_t t_ImageCount = t_SwapchainDetails.capabilities.minImageCount + 1;
	if (t_SwapchainDetails.capabilities.maxImageCount > 0 && t_ImageCount > 
		t_SwapchainDetails.capabilities.maxImageCount) 
	{
		t_ImageCount = t_SwapchainDetails.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR t_SwapCreateInfo{};
	t_SwapCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	t_SwapCreateInfo.surface = a_Surface;
	t_SwapCreateInfo.minImageCount = t_ImageCount;
	t_SwapCreateInfo.imageFormat = t_ChosenFormat.format;
	t_SwapCreateInfo.imageColorSpace = t_ChosenFormat.colorSpace;
	t_SwapCreateInfo.imageExtent = t_ChosenExtent;
	t_SwapCreateInfo.imageArrayLayers = 1;
	t_SwapCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	t_SwapCreateInfo.preTransform = t_SwapchainDetails.capabilities.currentTransform;
	t_SwapCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	t_SwapCreateInfo.presentMode = t_ChosenPresentMode;
	t_SwapCreateInfo.clipped = VK_TRUE;
	t_SwapCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	uint32_t t_GraphicFamily, t_PresentFamily; 
	QueueFindGraphicsBit(a_TempAllocator, a_PhysicalDevice, &t_GraphicFamily);
	QueueHasPresentSupport(a_TempAllocator, a_PhysicalDevice, a_Surface, &t_PresentFamily);
	uint32_t t_QueueFamilyIndices[] = { t_GraphicFamily, t_PresentFamily };

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

	VKASSERT(vkCreateSwapchainKHR(a_Device, &t_SwapCreateInfo, nullptr, &t_ReturnSwapchain.swapChain), "Vulkan: Failed to create swapchain.");
	
	vkGetSwapchainImagesKHR(a_Device, t_ReturnSwapchain.swapChain, &t_ImageCount, nullptr);
	t_ReturnSwapchain.images = BBnewArr<VkImage>(a_SysAllocator, t_ImageCount);
	t_ReturnSwapchain.imageViews = BBnewArr<VkImageView>(a_SysAllocator, t_ImageCount);
	t_ReturnSwapchain.imageCount = t_ImageCount;
	vkGetSwapchainImagesKHR(a_Device, t_ReturnSwapchain.swapChain, &t_ImageCount, t_ReturnSwapchain.images);
	
	VkImageViewCreateInfo t_ImageViewCreateInfo{};
	t_ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	t_ImageViewCreateInfo.format = t_ChosenFormat.format;
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


	//Create sync structures in the same loop, might be moved to commandlist.
	VkFenceCreateInfo t_FenceCreateInfo = VkInit::FenceCreationInfo();
	//first one is already signaled to make sure we can still render.
	t_FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VkSemaphoreCreateInfo t_SemCreateInfo = VkInit::SemaphoreCreationInfo();
	t_ReturnSwapchain.frameFences = BBnewArr<VkFence>(a_SysAllocator, t_ImageCount);
	t_ReturnSwapchain.presentSems = BBnewArr<VkSemaphore>(a_SysAllocator, t_ImageCount);
	t_ReturnSwapchain.renderSems = BBnewArr<VkSemaphore>(a_SysAllocator, t_ImageCount);

	for (uint32_t i = 0; i < t_ImageCount; i++)
	{
		t_ImageViewCreateInfo.image = t_ReturnSwapchain.images[i];
		VKASSERT(vkCreateImageView(a_Device,
			&t_ImageViewCreateInfo,
			nullptr,
			&t_ReturnSwapchain.imageViews[i]), 
			"Vulkan: Failed to create swapchain image views.");

		VKASSERT(vkCreateFence(a_Device,
			&t_FenceCreateInfo,
			nullptr, 
			&t_ReturnSwapchain.frameFences[i]),
			"Vulkan: Failed to create fence.");
		VKASSERT(vkCreateSemaphore(a_Device,
			&t_SemCreateInfo,
			nullptr, 
			&t_ReturnSwapchain.presentSems[i]),
			"Vulkan: Failed to create present semaphore.");
		VKASSERT(vkCreateSemaphore(a_Device,
			&t_SemCreateInfo,
			nullptr,
			&t_ReturnSwapchain.renderSems[i]),
			"Vulkan: Failed to create render semaphore.");

		//reset the flags to 0 since we won't be making signaled fences again.
		t_FenceCreateInfo.flags = 0;
	}

	t_ReturnSwapchain.imageFormat = t_ChosenFormat.format;
	t_ReturnSwapchain.extent = t_ChosenExtent;

	return t_ReturnSwapchain;
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

	t_ReturnResult.pipelineShaderStageInfo = BBnewArr<VkPipelineShaderStageCreateInfo>(a_TempAllocator, a_CreateInfo.size());
	t_ReturnResult.shaderModules = BBnewArr<VkShaderModule>(a_TempAllocator, a_CreateInfo.size());
	
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

static VkPipelineLayout CreatePipelineLayout(const VulkanBackend& a_VulkanBackend,
	const Slice<VkPushConstantRange> a_PushConstants)
{
	const VkPipelineLayoutCreateInfo t_LayoutCreateInfo = VkInit::PipelineLayoutCreateInfo(
		0,
		nullptr,
		static_cast<uint32_t>(a_PushConstants.size()),
		a_PushConstants.data()
	);

	PipelineLayoutHash t_DescriptorHash = HashPipelineLayoutInfo(t_LayoutCreateInfo);
	VkPipelineLayout* t_FoundLayout = a_VulkanBackend.object->pipelineLayouts->find(t_DescriptorHash);

	if (t_FoundLayout != nullptr)
		return *t_FoundLayout;

	VkPipelineLayout t_NewLayout = VK_NULL_HANDLE;
	VKASSERT(vkCreatePipelineLayout(a_VulkanBackend.device.logicalDevice,
		&t_LayoutCreateInfo,
		nullptr,
		&t_NewLayout),
		"Vulkan: Failed to create pipelinelayout.");

	a_VulkanBackend.object->pipelineLayouts->insert(t_DescriptorHash, t_NewLayout);

	return t_NewLayout;
}

void BB::RenderFrame(Allocator a_TempAllocator, const VulkanCommandList& a_CmdList, const VulkanFrameBuffer& a_FrameBuffer, const VulkanPipeline& a_Pipeline, const VulkanBackend& a_Backend)
{
	uint32_t t_ImageIndex;
	VKASSERT(vkAcquireNextImageKHR(a_Backend.device.logicalDevice,
		a_Backend.mainSwapChain.swapChain,
		UINT64_MAX,
		a_Backend.mainSwapChain.presentSems[0],
		VK_NULL_HANDLE,
		&t_ImageIndex),
		"Vulkan: failed to get next image.");

	VKASSERT(vkWaitForFences(a_Backend.device.logicalDevice,
		1,
		&a_Backend.mainSwapChain.frameFences[0],
		VK_TRUE,
		10000000),
		"Vulkan: Failed to wait for frences");

	vkResetFences(a_Backend.device.logicalDevice, 
		1, 
		&a_Backend.mainSwapChain.frameFences[0]);

	vkResetCommandBuffer(a_CmdList.buffers[a_CmdList.currentFree], 0);
	VkCommandBufferBeginInfo t_CmdBeginInfo = VkInit::CommandBufferBeginInfo(nullptr);
	VKASSERT(vkBeginCommandBuffer(a_CmdList.buffers[a_CmdList.currentFree],
		&t_CmdBeginInfo),
		"Vulkan: Failed to begin commandbuffer");

	VkCommandBuffer t_CmdRecording = a_CmdList.buffers[a_CmdList.currentFree];

	VkClearValue t_ClearValue = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

	VkRenderPassBeginInfo t_RenderPassBegin = VkInit::RenderPassBeginInfo(
		a_FrameBuffer.renderPass,
		a_FrameBuffer.framebuffers[t_ImageIndex],
		VkInit::Rect2D(0,
			0,
			a_Backend.mainSwapChain.extent),
		1,
		&t_ClearValue);

	vkCmdBeginRenderPass(t_CmdRecording,
		&t_RenderPassBegin, 
		VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(t_CmdRecording, 
		VK_PIPELINE_BIND_POINT_GRAPHICS, 
		a_Pipeline.pipeline);

	VkViewport t_Viewport{};
	t_Viewport.x = 0.0f;
	t_Viewport.y = 0.0f;
	t_Viewport.width = static_cast<float>(a_Backend.mainSwapChain.extent.width);
	t_Viewport.height = static_cast<float>(a_Backend.mainSwapChain.extent.height);
	t_Viewport.minDepth = 0.0f;
	t_Viewport.maxDepth = 1.0f;
	vkCmdSetViewport(t_CmdRecording, 0, 1, &t_Viewport);

	VkRect2D t_Scissor{};
	t_Scissor.offset = { 0, 0 };
	t_Scissor.extent = a_Backend.mainSwapChain.extent;
	vkCmdSetScissor(t_CmdRecording, 0, 1, &t_Scissor);

	vkCmdDraw(t_CmdRecording, 3, 1, 0, 0);

	vkCmdEndRenderPass(t_CmdRecording);

	VKASSERT(vkEndCommandBuffer(t_CmdRecording), "Vulkan: Failed to record commandbuffer!");


	//submit stage.
	VkPipelineStageFlags t_WaitStagesMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkSubmitInfo t_SubmitInfo{};
	t_SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	t_SubmitInfo.waitSemaphoreCount = 1;
	t_SubmitInfo.pWaitSemaphores = &a_Backend.mainSwapChain.presentSems[0];
	t_SubmitInfo.pWaitDstStageMask = &t_WaitStagesMask;

	t_SubmitInfo.signalSemaphoreCount = 1;
	t_SubmitInfo.pSignalSemaphores = &a_Backend.mainSwapChain.renderSems[0];

	t_SubmitInfo.commandBufferCount = 1;
	t_SubmitInfo.pCommandBuffers = &t_CmdRecording;

	VKASSERT(vkQueueSubmit(a_Backend.device.graphicsQueue,
		1,
		&t_SubmitInfo,
		a_Backend.mainSwapChain.frameFences[0]),
		"Vulkan: failed to submit to queue.");

	VkPresentInfoKHR t_PresentInfo{};
	t_PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	t_PresentInfo.waitSemaphoreCount = 1;
	t_PresentInfo.pWaitSemaphores = &a_Backend.mainSwapChain.renderSems[0];
	t_PresentInfo.swapchainCount = 1;
	t_PresentInfo.pSwapchains = &a_Backend.mainSwapChain.swapChain;
	t_PresentInfo.pImageIndices = &t_ImageIndex;
	t_PresentInfo.pResults = nullptr;

	VKASSERT(vkQueuePresentKHR(a_Backend.device.presentQueue, &t_PresentInfo),
		"Vulkan: Failed to queuepresentKHR.");
}

VulkanBackend BB::VulkanCreateBackend(BB::Allocator a_TempAllocator, BB::Allocator a_SysAllocator, const VulkanBackendCreateInfo& a_CreateInfo)
{
	VulkanBackend t_ReturnBackend;
	t_ReturnBackend.currentFrame = 0;
	t_ReturnBackend.object = BBnew<VulkanBackend_o>(a_SysAllocator);
	t_ReturnBackend.object->pipelineLayouts = 
		BBnew<OL_HashMap<PipelineLayoutHash, VkPipelineLayout>>(a_SysAllocator, a_SysAllocator);

	//Check if the extensions and layers work.
	BB_ASSERT(CheckExtensionSupport(a_TempAllocator, a_CreateInfo.extensions),
		"Vulkan: extension(s) not supported.");

#pragma region //Debug
	//For debug, we want to remember the extensions we have.
	t_ReturnBackend.extensions = BB::BBnewArr<const char*>(a_SysAllocator, a_CreateInfo.extensions.size());
	t_ReturnBackend.extensionCount = static_cast<uint32_t>(a_CreateInfo.extensions.size());
	for (size_t i = 0; i < t_ReturnBackend.extensionCount; i++)
	{
		t_ReturnBackend.extensions[i] = a_CreateInfo.extensions[i];
	}
#pragma endregion //Debug
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
		if (a_CreateInfo.validationLayers)
		{
			const char* validationLayer = "VK_LAYER_KHRONOS_validation";
			BB_WARNING(CheckValidationLayerSupport(a_TempAllocator, Slice(&validationLayer, 1)), "Vulkan: Validation layer(s) not available.", WarningType::MEDIUM);
			t_InstanceCreateInfo.ppEnabledLayerNames = &validationLayer;
			t_InstanceCreateInfo.enabledLayerCount = 1;
			t_InstanceCreateInfo.pNext = &VkInit::DebugUtilsMessengerCreateInfoEXT(debugCallback);
		}
		else
		{
			t_InstanceCreateInfo.ppEnabledLayerNames = nullptr;
			t_InstanceCreateInfo.enabledLayerCount = 0;
		}
		t_InstanceCreateInfo.ppEnabledExtensionNames = a_CreateInfo.extensions.data();
		t_InstanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(a_CreateInfo.extensions.size());

		VKASSERT(vkCreateInstance(&t_InstanceCreateInfo, nullptr, &t_ReturnBackend.instance), "Failed to create Vulkan Instance!");

		if (a_CreateInfo.validationLayers)
		{
			t_ReturnBackend.debugMessenger = CreateVulkanDebugMsgger(t_ReturnBackend.instance);
		}
		else
		{
			t_ReturnBackend.debugMessenger = 0;
		}
	}

	{
		//Surface
		VkWin32SurfaceCreateInfoKHR t_SurfaceCreateInfo{};
		t_SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		t_SurfaceCreateInfo.hwnd = a_CreateInfo.hwnd;
		t_SurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
		VKASSERT(vkCreateWin32SurfaceKHR(t_ReturnBackend.instance, &t_SurfaceCreateInfo, nullptr, &t_ReturnBackend.surface),
			"Failed to create Win32 vulkan surface.");
	}

	//Get the physical Device
	t_ReturnBackend.device.physicalDevice = FindPhysicalDevice(a_TempAllocator, 
		t_ReturnBackend.instance, 
		t_ReturnBackend.surface);
	//Get the logical device and the graphics queue.
	t_ReturnBackend.device.logicalDevice = CreateLogicalDevice(a_TempAllocator,
		a_CreateInfo.deviceExtensions,
		t_ReturnBackend.device.physicalDevice,
		&t_ReturnBackend.device.graphicsQueue);

	{
		uint32_t t_PresentIndex;
		if (QueueHasPresentSupport(a_TempAllocator,
			t_ReturnBackend.device.physicalDevice,
			t_ReturnBackend.surface,
			&t_PresentIndex))
		{
			vkGetDeviceQueue(t_ReturnBackend.device.logicalDevice, t_PresentIndex, 0, &t_ReturnBackend.device.presentQueue);
		}
	}

	t_ReturnBackend.mainSwapChain = CreateSwapchain(a_SysAllocator,
		a_TempAllocator,
		t_ReturnBackend.surface,
		t_ReturnBackend.device.physicalDevice,
		t_ReturnBackend.device.logicalDevice,
		a_CreateInfo.windowWidth,
		a_CreateInfo.windowHeight);

	return t_ReturnBackend;
}

VulkanFrameBuffer BB::VulkanCreateFrameBuffer(Allocator a_SysAllocator, Allocator a_TempAllocator, const VulkanBackend& a_VulkanBackend, const VulkanFrameBufferCreateInfo& a_FramebufferCreateInfo)
{
	VulkanFrameBuffer t_ReturnFrameBuffer;
	{
		//First do the renderpass
		VkAttachmentDescription t_ColorAttachment = VkInit::AttachmentDescription(
			a_FramebufferCreateInfo.swapchainFormat,
			VK_SAMPLE_COUNT_1_BIT,
			a_FramebufferCreateInfo.colorLoadOp,
			a_FramebufferCreateInfo.colorStoreOp,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			a_FramebufferCreateInfo.colorInitialLayout,
			a_FramebufferCreateInfo.colorFinalLayout);
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
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

		VkRenderPassCreateInfo t_RenderPassInfo = VkInit::RenderPassCreateInfo(
			1, &t_ColorAttachment, 1, &t_Subpass, 1, &t_Dependency);

		VKASSERT(vkCreateRenderPass(a_VulkanBackend.device.logicalDevice,
			&t_RenderPassInfo,
			nullptr,
			&t_ReturnFrameBuffer.renderPass),
			"Vulkan: Failed to create graphics Pipeline.");
	}

	{
		//Now do framebuffer

		t_ReturnFrameBuffer.width = a_FramebufferCreateInfo.width;
		t_ReturnFrameBuffer.height = a_FramebufferCreateInfo.height;
		t_ReturnFrameBuffer.frameBufferCount = a_FramebufferCreateInfo.frameBufferCount;
		t_ReturnFrameBuffer.framebuffers = BBnewArr<VkFramebuffer>(a_SysAllocator,
			a_FramebufferCreateInfo.frameBufferCount);

		{
			//I do not remember why my old code did this, the renderpass was already created?
			//renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			//renderPassBeginInfo.renderArea.offset = { 0, 0 };
			//renderPassBeginInfo.renderArea.extent = { width, height };
		}

		uint32_t t_UsedAttachments = 1;
		//have enough space for the potentional depth buffer.
		VkImageView t_AttachmentViews[2];
		if (a_FramebufferCreateInfo.depthTestView != VK_NULL_HANDLE)
		{
			t_UsedAttachments = 2;
			//last attachment is depth info.
			t_AttachmentViews[1] = a_FramebufferCreateInfo.depthTestView;
		}

		for (uint32_t i = 0; i < t_ReturnFrameBuffer.frameBufferCount; i++)
		{
			t_AttachmentViews[0] = a_FramebufferCreateInfo.swapChainViews[i];

			VkFramebufferCreateInfo t_FramebufferInfo = VkInit::FramebufferCreateInfo();
			t_FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			t_FramebufferInfo.renderPass = t_ReturnFrameBuffer.renderPass;
			t_FramebufferInfo.attachmentCount = t_UsedAttachments;
			t_FramebufferInfo.pAttachments = t_AttachmentViews;
			t_FramebufferInfo.width = t_ReturnFrameBuffer.width;
			t_FramebufferInfo.height = t_ReturnFrameBuffer.height;
			t_FramebufferInfo.layers = 1;

			VKASSERT(vkCreateFramebuffer(a_VulkanBackend.device.logicalDevice, 
				&t_FramebufferInfo, 
				nullptr, 
				&t_ReturnFrameBuffer.framebuffers[i]),
				"Vulkan: Failed to create Framebuffer");
		}
	}

	return t_ReturnFrameBuffer;
}

VulkanPipeline BB::VulkanCreatePipeline(Allocator a_TempAllocator, const VulkanBackend& a_Backend, const VulkanPipelineCreateInfo& a_CreateInfo)
{
	VulkanPipeline t_ReturnPipeline;

	//Get dynamic state for the viewport and scissor.
	VkDynamicState t_DynamicStates[2]{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo t_DynamicPipeCreateInfo{};
	t_DynamicPipeCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	t_DynamicPipeCreateInfo.dynamicStateCount = 2;
	t_DynamicPipeCreateInfo.pDynamicStates = t_DynamicStates;

	VulkanShaderResult t_ShaderCreateResult = CreateShaderModules(a_TempAllocator,
		a_Backend.device.logicalDevice,
		a_CreateInfo.shaderCreateInfos);

	//Set viewport to nullptr and let the commandbuffer handle it via 
	VkPipelineViewportStateCreateInfo t_ViewportState = VkInit::PipelineViewportStateCreateInfo(
		1,
		nullptr,
		1,
		nullptr
	);

	VkPipelineVertexInputStateCreateInfo t_VertexInput = VkInit::PipelineVertexInputStateCreateInfo(
		0,
		nullptr,
		0,
		nullptr);
	VkPipelineInputAssemblyStateCreateInfo t_InputAssembly = VkInit::PipelineInputAssemblyStateCreateInfo(
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE);
	VkPipelineRasterizationStateCreateInfo t_Rasterizer = VkInit::PipelineRasterizationStateCreateInfo(
		VK_FALSE,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE);
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

	VkPipelineLayout t_PipeLayout = CreatePipelineLayout(a_Backend, BB::Slice(&t_PushConstantMatrix, 1));

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
	t_PipeCreateInfo.renderPass = a_CreateInfo.pVulkanFrameBuffer->renderPass;
	t_PipeCreateInfo.subpass = 0;
	//Optimalization for later.
	t_PipeCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	t_PipeCreateInfo.basePipelineIndex = -1;

	VKASSERT(vkCreateGraphicsPipelines(a_Backend.device.logicalDevice,
		VK_NULL_HANDLE,
		1,
		&t_PipeCreateInfo,
		nullptr,
		&t_ReturnPipeline.pipeline),
		"Vulkan: Failed to create graphics Pipeline.");


	vkDestroyShaderModule(a_Backend.device.logicalDevice,
		t_ShaderCreateResult.shaderModules[0],
		nullptr);
	vkDestroyShaderModule(a_Backend.device.logicalDevice,
		t_ShaderCreateResult.shaderModules[1],
		nullptr);
	
	return t_ReturnPipeline;
}

VulkanCommandList BB::VulkanCreateCommandList(Allocator a_SysAllocator, Allocator a_TempAllocator, const VulkanBackend& a_Backend, uint32_t a_BufferCount)
{
	VulkanCommandList t_ReturnCommandList;
	uint32_t t_GraphicsBit;
	QueueFindGraphicsBit(a_TempAllocator, a_Backend.device.physicalDevice, &t_GraphicsBit);
	VkCommandPoolCreateInfo t_CommandPoolInfo = VkInit::CommandPoolCreateInfo(
		t_GraphicsBit,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	VKASSERT(vkCreateCommandPool(a_Backend.device.logicalDevice,
		&t_CommandPoolInfo,
		nullptr,
		&t_ReturnCommandList.pool),
		"Vulkan: Failed to create commandpool.");

	VkCommandBufferAllocateInfo t_AllocInfo = VkInit::CommandBufferAllocateInfo(
		t_ReturnCommandList.pool,
		a_BufferCount,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	t_ReturnCommandList.buffers = BBnewArr<VkCommandBuffer>(a_SysAllocator, a_BufferCount);
	t_ReturnCommandList.bufferCount = a_BufferCount;
	t_ReturnCommandList.currentFree = 0;

	VKASSERT(vkAllocateCommandBuffers(a_Backend.device.logicalDevice,
		&t_AllocInfo,
		t_ReturnCommandList.buffers),
		"Vulkan: failed to allocate commandbuffers.");

	return t_ReturnCommandList;
}


void BB::VulkanDestroyFramebuffer(Allocator a_SysAllocator, VulkanFrameBuffer& a_FrameBuffer,const VulkanBackend& a_Backend)
{
	for (uint32_t i = 0; i < a_FrameBuffer.frameBufferCount; i++)
	{
		vkDestroyFramebuffer(a_Backend.device.logicalDevice,
			a_FrameBuffer.framebuffers[i],
			nullptr);
	}
	BBfree(a_SysAllocator, a_FrameBuffer.framebuffers);

	vkDestroyRenderPass(a_Backend.device.logicalDevice,
		a_FrameBuffer.renderPass, 
		nullptr);
}

void BB::VulkanDestroyCommandList(Allocator a_SysAllocator, VulkanCommandList& a_CommandList, const VulkanBackend& a_Backend)
{
	vkDestroyCommandPool(a_Backend.device.logicalDevice, a_CommandList.pool, nullptr);
	BBfreeArr(a_SysAllocator, a_CommandList.buffers);
}

void BB::VulkanDestroyPipeline(VulkanPipeline& a_Pipeline, const VulkanBackend& a_Backend)
{
	vkDestroyPipeline(a_Backend.device.logicalDevice, a_Pipeline.pipeline, nullptr);
}

void BB::VulkanDestroyBackend(BB::Allocator a_SysAllocator, VulkanBackend& a_Backend)
{
	for (auto t_It = a_Backend.object->pipelineLayouts->begin();
		t_It < a_Backend.object->pipelineLayouts->end(); t_It++)
	{
		vkDestroyPipelineLayout(a_Backend.device.logicalDevice,
			*t_It->value,
			nullptr);
	}

	BBfree(a_SysAllocator, a_Backend.object->pipelineLayouts);
	BBfree(a_SysAllocator, a_Backend.object);

	BBfreeArr(a_SysAllocator, a_Backend.extensions);
	for (size_t i = 0; i < a_Backend.mainSwapChain.imageCount; i++)
	{
		vkDestroyImageView(a_Backend.device.logicalDevice, a_Backend.mainSwapChain.imageViews[i], nullptr);
		vkDestroyFence(a_Backend.device.logicalDevice, a_Backend.mainSwapChain.frameFences[i], nullptr);
		vkDestroySemaphore(a_Backend.device.logicalDevice, a_Backend.mainSwapChain.presentSems[i], nullptr);
		vkDestroySemaphore(a_Backend.device.logicalDevice, a_Backend.mainSwapChain.renderSems[i], nullptr);
	}
	BBfreeArr(a_SysAllocator, a_Backend.mainSwapChain.images);
	BBfreeArr(a_SysAllocator, a_Backend.mainSwapChain.imageViews);
	BBfreeArr(a_SysAllocator, a_Backend.mainSwapChain.frameFences);
	BBfreeArr(a_SysAllocator, a_Backend.mainSwapChain.presentSems);
	BBfreeArr(a_SysAllocator, a_Backend.mainSwapChain.renderSems);

	vkDestroySwapchainKHR(a_Backend.device.logicalDevice, a_Backend.mainSwapChain.swapChain, nullptr);
	vkDestroyDevice(a_Backend.device.logicalDevice, nullptr);

	if (a_Backend.debugMessenger != 0)
		DestroyVulkanDebug(a_Backend.instance, a_Backend.debugMessenger);

	vkDestroySurfaceKHR(a_Backend.instance, a_Backend.surface, nullptr);
	vkDestroyInstance(a_Backend.instance, nullptr);
}
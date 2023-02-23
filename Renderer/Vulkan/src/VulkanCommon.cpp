#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1003000 // Vulkan 1.2
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

constexpr int VULKAN_VERSION = 3;

#include "VulkanInitializers.h"
#include "Storage/Hashmap.h"
#include "Storage/Slotmap.h"
#include "Storage/Pool.h"

#include "VulkanCommon.h"

#include <iostream>

using namespace BB;


inline VmaMemoryUsage MemoryPropertyFlags(const RENDER_MEMORY_PROPERTIES a_Properties)
{
	switch (a_Properties)
	{
	case BB::RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL:
		return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		break;
	case BB::RENDER_MEMORY_PROPERTIES::HOST_VISIBLE:
		return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		break;
	default:
		BB_ASSERT(false, "this memory property is not supported by the vulkan backend!");
		return VMA_MEMORY_USAGE_AUTO;
		break;
	}
}

struct VulkanBuffer
{
	VkBuffer buffer;
	VmaAllocation allocation;
};

static FreelistAllocator_t s_VulkanAllocator{ mbSize * 2 };

struct VkCommandAllocator;

struct VulkanCommandList
{
	VkCommandBuffer* bufferPtr;

	//Cached variables
	VkPipelineLayout currentPipelineLayout = VK_NULL_HANDLE;

	VkCommandAllocator* cmdAllocator;

	VkCommandBuffer Buffer() const { return *bufferPtr; }
};

constexpr uint64_t COMMAND_BUFFER_STANDARD_COUNT = 32;
struct VkCommandAllocator
{
	VkCommandPool pool;
	BB::Pool<VkCommandBuffer> buffers;

	VulkanCommandList GetCommandList()
	{
		VulkanCommandList t_CommandList{};
		t_CommandList.bufferPtr = buffers.Get();
		t_CommandList.cmdAllocator = this;
		return t_CommandList;
	}
	void FreeCommandList(const VulkanCommandList t_CmdList)
	{
		buffers.Free(t_CmdList.bufferPtr);
	}
};

static VkDescriptorPoolSize s_DescriptorPoolSizes[]
{
	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 2 }
};

struct DescriptorAllocator
{
	const VkDescriptorPool GetPool() const
	{
		return descriptorPool;
	}

	void CreateDescriptorPool();
	void Destroy();

private:
	VkDescriptorPool descriptorPool;
};

struct VKPipelineBuildInfo
{
	//temporary allocator, this gets removed when we are finished building.
	TemporaryAllocator buildAllocator{ s_VulkanAllocator };

	VkGraphicsPipelineCreateInfo pipeInfo{};
	VkPipelineRenderingCreateInfo dynamicRenderingInfo{}; //attachment for dynamic rendering.
	VulkanShaderResult shaderInfo;

	uint32_t layoutCount;
	VkDescriptorSetLayout layout[BINDING_MAX];
	uint32_t constantRangeCount;
	VkPushConstantRange constantRanges[BINDING_MAX]; //Can be more, but for now no good solution for this.
};

using PipelineLayoutHash = uint64_t;
struct VulkanBackend_inst
{
	uint32_t currentFrame = 0;
	FrameIndex frameCount = 0;
	uint32_t imageIndex = 0;

	VkInstance instance{};
	VkSurfaceKHR surface{};
	DescriptorAllocator descriptorAllocator;

	VulkanDevice device{};
	VulkanSwapChain swapChain{};
	VmaAllocator vma{};
	Slotmap<VulkanCommandList> commandLists{ s_VulkanAllocator };

	Pool<VulkanCommandQueue> cmdQueues;
	Pool<VkCommandAllocator> cmdAllocators;
	Pool<VulkanPipeline> pipelinePool;
	Pool<VulkanBuffer> renderBuffers;
	Pool<VulkanBindingSet> bindingSetPool;

	//OL_HashMap<DescriptorLayout, VkDescriptorSetLayout> descriptorLayouts{ s_VulkanAllocator };
	OL_HashMap<PipelineLayoutHash, VkPipelineLayout> pipelineLayouts{ s_VulkanAllocator };

	VulkanDebug vulkanDebug;

	void CreatePools()
	{
		cmdQueues.CreatePool(s_VulkanAllocator, 8);
		cmdAllocators.CreatePool(s_VulkanAllocator, 8);
		pipelinePool.CreatePool(s_VulkanAllocator, 8);
		renderBuffers.CreatePool(s_VulkanAllocator, 16);
		bindingSetPool.CreatePool(s_VulkanAllocator, 16);
	}

	void DestroyPools()
	{
		cmdQueues.DestroyPool(s_VulkanAllocator);
		cmdAllocators.DestroyPool(s_VulkanAllocator);
		pipelinePool.DestroyPool(s_VulkanAllocator);
		renderBuffers.DestroyPool(s_VulkanAllocator);
		bindingSetPool.DestroyPool(s_VulkanAllocator);
	}
};
static VulkanBackend_inst s_VKB;

static VkDeviceSize PadUBOBufferSize(const VkDeviceSize a_BuffSize)
{
	VkPhysicalDeviceProperties t_Properties;
	vkGetPhysicalDeviceProperties(s_VKB.device.physicalDevice, &t_Properties);
	return Pointer::AlignPad(a_BuffSize, t_Properties.limits.minUniformBufferOffsetAlignment);
}

void DescriptorAllocator::CreateDescriptorPool()
{
	VkDescriptorPoolCreateInfo t_CreateInfo{};
	t_CreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	t_CreateInfo.pPoolSizes = s_DescriptorPoolSizes;
	t_CreateInfo.poolSizeCount = _countof(s_DescriptorPoolSizes);

	t_CreateInfo.maxSets = 1000;
	t_CreateInfo.flags = 0;

	VKASSERT(vkCreateDescriptorPool(s_VKB.device.logicalDevice,
		&t_CreateInfo, nullptr, &descriptorPool),
		"Vulkan: Failed to create descriptorPool.");
}

void DescriptorAllocator::Destroy()
{
	vkDestroyDescriptorPool(s_VKB.device.logicalDevice,
		descriptorPool,
		nullptr);
}

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
	vkGetPhysicalDeviceMemoryProperties(s_VKB.device.physicalDevice, &memProperties);

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
	SwapchainSupportDetails t_SwapDetails{};

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
static bool QueueFindGraphicsBit(Allocator a_TempAllocator, VkPhysicalDevice a_PhysicalDevice)
{
	uint32_t t_QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, nullptr);

	VkQueueFamilyProperties* t_QueueFamilies = BBnewArr(a_TempAllocator, t_QueueFamilyCount, VkQueueFamilyProperties);
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, t_QueueFamilies);

	for (uint32_t i = 0; i < t_QueueFamilyCount; i++)
	{
		if (t_QueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			return true;
		}
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
		VkPhysicalDeviceProperties2 t_DeviceProperties{};
		t_DeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		vkGetPhysicalDeviceProperties2(t_PhysicalDevices[i], &t_DeviceProperties);

		VkPhysicalDeviceTimelineSemaphoreFeatures t_SyncFeatures{};
		t_SyncFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
		VkPhysicalDeviceFeatures2 t_DeviceFeatures{};
		t_DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		t_DeviceFeatures.pNext = &t_SyncFeatures;
		vkGetPhysicalDeviceFeatures2(t_PhysicalDevices[i], &t_DeviceFeatures);

		SwapchainSupportDetails t_SwapChainDetails = QuerySwapChainSupport(a_TempAllocator, a_Surface, t_PhysicalDevices[i]);

		if (t_DeviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			t_SyncFeatures.timelineSemaphore == VK_TRUE,
			t_DeviceFeatures.features.geometryShader &&

			QueueFindGraphicsBit(a_TempAllocator, t_PhysicalDevices[i]) &&
			t_SwapChainDetails.formatCount != 0 &&
			t_SwapChainDetails.presentModeCount != 0)
		{
			return t_PhysicalDevices[i];
		}
	}

	BB_ASSERT(false, "Failed to find a suitable GPU that is discrete and has a geometry shader.");
	return VK_NULL_HANDLE;
}

static VkDevice CreateLogicalDevice(Allocator a_TempAllocator, const BB::Slice<const char*>& a_DeviceExtensions)
{
	VkDevice t_ReturnDevice;

	uint32_t t_QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(s_VKB.device.physicalDevice, &t_QueueFamilyCount, nullptr);
	VkQueueFamilyProperties* t_QueueFamilies = BBnewArr(a_TempAllocator, t_QueueFamilyCount, VkQueueFamilyProperties);
	vkGetPhysicalDeviceQueueFamilyProperties(s_VKB.device.physicalDevice, &t_QueueFamilyCount, t_QueueFamilies);

	VkDeviceQueueCreateInfo* t_QueueCreateInfos = BBnewArr(a_TempAllocator, 3, VkDeviceQueueCreateInfo);
	uint32_t t_DifferentQueues = 0;
	float t_StandardQueuePrios[16] = { 1.0f }; // just put it all to 1 for multiple queues;

	{
		VulkanQueueDeviceInfo t_GraphicQueue = FindQueueIndex(t_QueueFamilies,
			t_QueueFamilyCount,
			VK_QUEUE_GRAPHICS_BIT);

		s_VKB.device.queueIndices.graphics = t_GraphicQueue.index;
		s_VKB.device.queueIndices.present = t_GraphicQueue.index;
		//set the graphics queue first.
		t_QueueCreateInfos[t_DifferentQueues] = {};
		t_QueueCreateInfos[t_DifferentQueues].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		t_QueueCreateInfos[t_DifferentQueues].queueFamilyIndex = t_GraphicQueue.index;
		t_QueueCreateInfos[t_DifferentQueues].queueCount = t_GraphicQueue.queueCount;
		t_QueueCreateInfos[t_DifferentQueues].pQueuePriorities = t_StandardQueuePrios;
		++t_DifferentQueues;
	}

	{
		VulkanQueueDeviceInfo t_TransferQueue = FindQueueIndex(t_QueueFamilies,
			t_QueueFamilyCount,
			VK_QUEUE_TRANSFER_BIT);
		//Check if the queueindex is the same as graphics.
		if (t_TransferQueue.index != s_VKB.device.queueIndices.graphics)
		{
			s_VKB.device.queueIndices.transfer = t_TransferQueue.index;
			//set the graphics queue first.
			t_QueueCreateInfos[t_DifferentQueues] = {};
			t_QueueCreateInfos[t_DifferentQueues].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			t_QueueCreateInfos[t_DifferentQueues].queueFamilyIndex = t_TransferQueue.index;
			t_QueueCreateInfos[t_DifferentQueues].queueCount = t_TransferQueue.queueCount;
			t_QueueCreateInfos[t_DifferentQueues].pQueuePriorities = t_StandardQueuePrios;
			++t_DifferentQueues;
		}
		else
		{
			s_VKB.device.queueIndices.transfer = s_VKB.device.queueIndices.graphics;
		}
	}

	{
		VulkanQueueDeviceInfo t_ComputeQueue = FindQueueIndex(t_QueueFamilies,
			t_QueueFamilyCount,
			VK_QUEUE_COMPUTE_BIT);
		//Check if the queueindex is the same as graphics.
		if ((t_ComputeQueue.index != s_VKB.device.queueIndices.graphics) &&
			(t_ComputeQueue.index != s_VKB.device.queueIndices.compute))
		{
			s_VKB.device.queueIndices.compute = t_ComputeQueue.index;
			//set the graphics queue first.
			t_QueueCreateInfos[t_DifferentQueues] = {};
			t_QueueCreateInfos[t_DifferentQueues].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			t_QueueCreateInfos[t_DifferentQueues].queueFamilyIndex = t_ComputeQueue.index;
			t_QueueCreateInfos[t_DifferentQueues].queueCount = t_ComputeQueue.queueCount;
			t_QueueCreateInfos[t_DifferentQueues].pQueuePriorities = t_StandardQueuePrios;
			++t_DifferentQueues;
		}
		else
		{
			s_VKB.device.queueIndices.compute = s_VKB.device.queueIndices.graphics;
		}
	}

	VkPhysicalDeviceFeatures t_DeviceFeatures{};
	VkPhysicalDeviceTimelineSemaphoreFeatures t_TimelineSemFeatures{};
	t_TimelineSemFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
	t_TimelineSemFeatures.timelineSemaphore = VK_TRUE;
	t_TimelineSemFeatures.pNext = nullptr;

	VkPhysicalDeviceShaderDrawParametersFeatures t_ShaderDrawFeatures{};
	t_ShaderDrawFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
	t_ShaderDrawFeatures.pNext = nullptr;
	t_ShaderDrawFeatures.shaderDrawParameters = VK_TRUE;
	t_ShaderDrawFeatures.pNext = &t_TimelineSemFeatures;

	VkPhysicalDeviceDynamicRenderingFeatures t_DynamicRendering{};
	t_DynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
	t_DynamicRendering.dynamicRendering = VK_TRUE;
	t_DynamicRendering.pNext = &t_ShaderDrawFeatures;

	VkDeviceCreateInfo t_CreateInfo{};
	t_CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	t_CreateInfo.pQueueCreateInfos = t_QueueCreateInfos;
	t_CreateInfo.queueCreateInfoCount = t_DifferentQueues;
	t_CreateInfo.pEnabledFeatures = &t_DeviceFeatures;

	t_CreateInfo.ppEnabledExtensionNames = a_DeviceExtensions.data();
	t_CreateInfo.enabledExtensionCount = static_cast<uint32_t>(a_DeviceExtensions.size());
	t_CreateInfo.pNext = &t_DynamicRendering;

	VKASSERT(vkCreateDevice(s_VKB.device.physicalDevice, 
		&t_CreateInfo, 
		nullptr, 
		&t_ReturnDevice),
		"Failed to create logical device Vulkan.");

	//Get the present queue.
	vkGetDeviceQueue(t_ReturnDevice,
		s_VKB.device.queueIndices.present,
		0,
		&s_VKB.device.presentQueue);

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

	for (uint32_t i = 0; i < s_VKB.frameCount; i++)
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
	VkExtent2D t_ChosenExtent{};
	t_ChosenExtent.width = Math::clamp(t_SurfaceWidth,
		t_SwapchainDetails.capabilities.minImageExtent.width,
		t_SwapchainDetails.capabilities.maxImageExtent.width);
	t_ChosenExtent.height = Math::clamp(t_SurfaceHeight,
		t_SwapchainDetails.capabilities.minImageExtent.height,
		t_SwapchainDetails.capabilities.maxImageExtent.height);
	a_SwapChain.extent = t_ChosenExtent;

	uint32_t t_GraphicFamily, t_PresentFamily;
	t_GraphicFamily = s_VKB.device.queueIndices.graphics;
	t_PresentFamily = s_VKB.device.queueIndices.present;
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
		s_VKB.frameCount = t_SwapchainDetails.capabilities.minImageCount + 1;
		t_SwapCreateInfo.minImageCount = t_SwapchainDetails.capabilities.minImageCount + 1;
		if (t_SwapchainDetails.capabilities.maxImageCount > 0 && s_VKB.frameCount >
			t_SwapchainDetails.capabilities.maxImageCount)
		{
			s_VKB.frameCount = t_SwapchainDetails.capabilities.maxImageCount;
			t_SwapCreateInfo.minImageCount = t_SwapchainDetails.capabilities.maxImageCount;
		}

		VKASSERT(vkCreateSwapchainKHR(a_Device, &t_SwapCreateInfo, nullptr, &a_SwapChain.swapChain), "Vulkan: Failed to create swapchain.");

		vkGetSwapchainImagesKHR(a_Device, a_SwapChain.swapChain, &s_VKB.frameCount, nullptr);
		a_SwapChain.images = BBnewArr(s_VulkanAllocator, s_VKB.frameCount, VkImage);
		a_SwapChain.imageViews = BBnewArr(s_VulkanAllocator, s_VKB.frameCount, VkImageView);
		vkGetSwapchainImagesKHR(a_Device, a_SwapChain.swapChain, &s_VKB.frameCount, a_SwapChain.images);

		//Create sync structures in the same loop, might be moved to commandlist.
		VkFenceCreateInfo t_FenceCreateInfo = VkInit::FenceCreationInfo();
		//first one is already signaled to make sure we can still render.
		t_FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		CreateImageViews(a_SwapChain.imageViews,
			a_SwapChain.images,
			s_VKB.device.logicalDevice,
			a_SwapChain.imageFormat,
			s_VKB.frameCount);

		//Also create the present semaphores, these are unique semaphores that handle the window integration API as they cannot use timeline semaphores.
		a_SwapChain.waitSyncs = BBnewArr(
			s_VulkanAllocator,
			s_VKB.frameCount,
			FrameWaitSync);

		VkSemaphoreCreateInfo t_SemInfo{};
		t_SemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		//Used for the last semaphore created of a single frame struct.
		VkSemaphoreTypeCreateInfo t_TimelineSemInfo{};
		t_TimelineSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		t_TimelineSemInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		t_TimelineSemInfo.initialValue = 0;
		for (size_t i = 0; i < s_VKB.frameCount; i++)
		{
			vkCreateSemaphore(s_VKB.device.logicalDevice,
				&t_SemInfo,
				nullptr,
				&a_SwapChain.waitSyncs[i].imageAvailableSem);
			vkCreateSemaphore(s_VKB.device.logicalDevice,
				&t_SemInfo,
				nullptr,
				&a_SwapChain.waitSyncs[i].imageRenderFinishedSem);

			t_SemInfo.pNext = &t_TimelineSemInfo;
			vkCreateSemaphore(s_VKB.device.logicalDevice,
				&t_SemInfo,
				nullptr,
				&a_SwapChain.waitSyncs[i].frameTimelineSemaphore);
			t_SemInfo.pNext = nullptr;
			a_SwapChain.waitSyncs[i].frameWaitValue = 0;
		}
	}
	else //Or recreate the swapchain.
	{
		t_SwapCreateInfo.oldSwapchain = a_SwapChain.swapChain;
		t_SwapCreateInfo.imageExtent = a_SwapChain.extent;
		t_SwapCreateInfo.minImageCount = s_VKB.frameCount;

		VKASSERT(vkCreateSwapchainKHR(a_Device, &t_SwapCreateInfo, nullptr, &a_SwapChain.swapChain), "Vulkan: Failed to create swapchain.");
		vkGetSwapchainImagesKHR(a_Device, a_SwapChain.swapChain, &s_VKB.frameCount, a_SwapChain.images);

		CreateImageViews(a_SwapChain.imageViews,
			a_SwapChain.images,
			s_VKB.device.logicalDevice,
			a_SwapChain.imageFormat,
			s_VKB.frameCount);
	}
}

//Creates VkPipelineShaderStageCreateInfo equal to the amount of ShaderCreateInfos in a_CreateInfo.
static VulkanShaderResult CreateShaderModules(Allocator a_TempAllocator, VkDevice a_Device, const Slice<BB::ShaderCreateInfo> a_CreateInfo)
{
	VulkanShaderResult t_ReturnResult{};

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

static VkPipelineLayout CreatePipelineLayout(const Slice<VkDescriptorSetLayout> a_DescLayouts, const Slice<VkPushConstantRange> a_PushConstants)
{
	VkPipelineLayoutCreateInfo t_LayoutCreateInfo{};
	t_LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	t_LayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(a_DescLayouts.size());
	t_LayoutCreateInfo.pSetLayouts = a_DescLayouts.data();
	t_LayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(a_PushConstants.size());
	t_LayoutCreateInfo.pPushConstantRanges = a_PushConstants.data();

	PipelineLayoutHash t_DescriptorHash = HashPipelineLayoutInfo(t_LayoutCreateInfo);
	VkPipelineLayout* t_FoundLayout = s_VKB.pipelineLayouts.find(t_DescriptorHash);

	if (t_FoundLayout != nullptr)
		return *t_FoundLayout;

	VkPipelineLayout t_NewLayout = VK_NULL_HANDLE;
	VKASSERT(vkCreatePipelineLayout(s_VKB.device.logicalDevice,
		&t_LayoutCreateInfo,
		nullptr,
		&t_NewLayout),
		"Vulkan: Failed to create pipelinelayout.");

	s_VKB.pipelineLayouts.insert(t_DescriptorHash, t_NewLayout);

	return t_NewLayout;
}

RBufferHandle BB::VulkanCreateBuffer(const RenderBufferCreateInfo& a_Info)
{
	VulkanBuffer* t_Buffer = s_VKB.renderBuffers.Get();

	VkBufferCreateInfo t_BufferInfo{};
	t_BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	t_BufferInfo.size = a_Info.size;
	t_BufferInfo.usage = VKConv::RenderBufferUsage(a_Info.usage);
	t_BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo t_VmaAlloc{};
	t_VmaAlloc.usage = MemoryPropertyFlags(a_Info.memProperties);
	if (t_VmaAlloc.usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST)
		t_VmaAlloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;


	VKASSERT(vmaCreateBuffer(s_VKB.vma,
		&t_BufferInfo, &t_VmaAlloc,
		&t_Buffer->buffer, &t_Buffer->allocation,
		nullptr), "Vulkan::VMA, Failed to allocate memory");

	if (a_Info.data != nullptr && 
		a_Info.memProperties != RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL)
	{
		void* t_MapData;
		VKASSERT(vmaMapMemory(s_VKB.vma,
			t_Buffer->allocation,
			&t_MapData),
			"Vulkan: Failed to map memory");
		memcpy(Pointer::Add(t_MapData, 0), a_Info.data, a_Info.size);
		vmaUnmapMemory(s_VKB.vma, t_Buffer->allocation);
	}

	return RBufferHandle(t_Buffer);
}

void BB::VulkanDestroyBuffer(RBufferHandle a_Handle)
{
	VulkanBuffer* t_Buffer = reinterpret_cast<VulkanBuffer*>(a_Handle.ptrHandle);
	vmaDestroyBuffer(s_VKB.vma, t_Buffer->buffer, t_Buffer->allocation);
	s_VKB.renderBuffers.Free(t_Buffer);
}

BackendInfo BB::VulkanCreateBackend(Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo)
{
	//Initialize data structure
	s_VKB.CreatePools();


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
	s_VKB.vulkanDebug.extensions = BBnewArr(s_VulkanAllocator, t_InstanceExtensions.count, const char*);
	s_VKB.vulkanDebug.extensionCount = t_InstanceExtensions.count;
	for (size_t i = 0; i < s_VKB.vulkanDebug.extensionCount; i++)
	{
		s_VKB.vulkanDebug.extensions[i] = t_InstanceExtensions.extensions[i];
	}
#endif //_DEBUG
	{
		VkApplicationInfo t_AppInfo{};
		t_AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		t_AppInfo.pApplicationName = a_CreateInfo.appName;
		t_AppInfo.pEngineName = a_CreateInfo.engineName;
		t_AppInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);
		t_AppInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);
		t_AppInfo.apiVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);

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
			&s_VKB.instance), "Failed to create Vulkan Instance!");

		if (a_CreateInfo.validationLayers)
		{
			s_VKB.vulkanDebug.debugMessenger = CreateVulkanDebugMsgger(s_VKB.instance);
		}
		else
		{
			s_VKB.vulkanDebug.debugMessenger = 0;
		}
	}

	{
		//Surface
		VkWin32SurfaceCreateInfoKHR t_SurfaceCreateInfo{};
		t_SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		t_SurfaceCreateInfo.hwnd = a_CreateInfo.hwnd;
		t_SurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
		VKASSERT(vkCreateWin32SurfaceKHR(s_VKB.instance,
			&t_SurfaceCreateInfo, nullptr,
			&s_VKB.surface),
			"Failed to create Win32 vulkan surface.");
	}

	//Get the physical Device
	s_VKB.device.physicalDevice = FindPhysicalDevice(a_TempAllocator,
		s_VKB.instance,
		s_VKB.surface);
	//Get the logical device and the graphics queue.
	s_VKB.device.logicalDevice = CreateLogicalDevice(a_TempAllocator,
		BB::Slice(t_DeviceExtensions.extensions, t_DeviceExtensions.count));


	CreateSwapchain(s_VKB.swapChain, 
		a_TempAllocator,
		s_VKB.surface,
		s_VKB.device.physicalDevice,
		s_VKB.device.logicalDevice,
		a_CreateInfo.windowWidth,
		a_CreateInfo.windowHeight);



	//Setup the Vulkan Memory Allocator
	VmaVulkanFunctions t_VkFunctions{};
	t_VkFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	t_VkFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo t_AllocatorCreateInfo = {};
	t_AllocatorCreateInfo.vulkanApiVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);
	t_AllocatorCreateInfo.physicalDevice = s_VKB.device.physicalDevice;
	t_AllocatorCreateInfo.device = s_VKB.device.logicalDevice;
	t_AllocatorCreateInfo.instance = s_VKB.instance;
	t_AllocatorCreateInfo.pVulkanFunctions = &t_VkFunctions;

	vmaCreateAllocator(&t_AllocatorCreateInfo, &s_VKB.vma);

	//Create descriptor allocator.
	s_VKB.descriptorAllocator.CreateDescriptorPool();

	//Returns some info to the global backend that is important.
	BackendInfo t_BackendInfo;
	t_BackendInfo.currentFrame = s_VKB.currentFrame;;
	t_BackendInfo.framebufferCount = s_VKB.frameCount;

	return t_BackendInfo;
}

RBindingSetHandle BB::VulkanCreateBindingSet(const RenderBindingSetCreateInfo& a_Info)
{
	constexpr uint32_t STANDARD_DESCRIPTORSET_COUNT = 1; //Setting a standard here, may change this later if I want to make more sets in 1 call. 

	VulkanBindingSet* t_BindingSet = s_VKB.bindingSetPool.Get();
	*t_BindingSet = {}; //set to 0

	uint32_t t_PushConstantOffset = 0;
	for (size_t i = 0; i < a_Info.constantBinds.size(); i++)
	{
		t_BindingSet->pushConstants[i].shaderStage = VKConv::ShaderStageBits(a_Info.constantBinds[i].stage);
		t_BindingSet->pushConstants[i].offset = t_PushConstantOffset;
		t_PushConstantOffset += (a_Info.constantBinds[i].dwordCount * sizeof(uint32_t));
	}

	t_BindingSet->pushConstantCount = static_cast<uint32_t>(a_Info.constantBinds.size());

	VkDescriptorSetLayoutBinding* t_BufferBindings = BBnewArr(
		s_VulkanAllocator,
		a_Info.bufferBinds.size(),
		VkDescriptorSetLayoutBinding);

	VkWriteDescriptorSet* t_Writes = BBnewArr(
		s_VulkanAllocator,
		a_Info.bufferBinds.size(),
		VkWriteDescriptorSet);

	//Setup buffer specific info.
	VkDescriptorBufferInfo* t_BufferInfos = BBnewArr(
		s_VulkanAllocator,
		a_Info.bufferBinds.size(),
		VkDescriptorBufferInfo);

	for (size_t i = 0; i < a_Info.bufferBinds.size(); i++)
	{
		t_BufferBindings[i].binding = a_Info.bufferBinds[i].binding;
		t_BufferBindings[i].descriptorCount = STANDARD_DESCRIPTORSET_COUNT;
		t_BufferBindings[i].descriptorType = VKConv::DescriptorBufferType(a_Info.bufferBinds[i].type);
		t_BufferBindings[i].pImmutableSamplers = nullptr;
		t_BufferBindings[i].stageFlags = VKConv::ShaderStageBits(a_Info.bufferBinds[i].stage);

		//Setup the buffer Info.
		t_BufferInfos[i].buffer = reinterpret_cast<VulkanBuffer*>(a_Info.bufferBinds[i].buffer.handle)->buffer;
		t_BufferInfos[i].offset = a_Info.bufferBinds[i].bufferOffset;
		t_BufferInfos[i].range = a_Info.bufferBinds[i].bufferSize;

		t_Writes[i] = {};
		t_Writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		t_Writes[i].dstBinding = a_Info.bufferBinds[i].binding;
		t_Writes[i].descriptorCount = STANDARD_DESCRIPTORSET_COUNT;
		t_Writes[i].descriptorType = VKConv::DescriptorBufferType(a_Info.bufferBinds[i].type);
		t_Writes[i].pBufferInfo = &t_BufferInfos[i];
	}

	{ //Create the descriptorSet layout.
		VkDescriptorSetLayoutCreateInfo t_LayoutInfo{};
		t_LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		t_LayoutInfo.pBindings = t_BufferBindings;
		t_LayoutInfo.bindingCount = static_cast<uint32_t>(a_Info.bufferBinds.size());

		//Do some algorithm to see if I already made a descriptorlayout like this one.
		VKASSERT(vkCreateDescriptorSetLayout(s_VKB.device.logicalDevice,
			&t_LayoutInfo, nullptr, &t_BindingSet->setLayout),
			"Vulkan: Failed to create a descriptorsetlayout.");
	}

	VkDescriptorSetAllocateInfo t_AllocInfo = {};
	t_AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	t_AllocInfo.pSetLayouts = &t_BindingSet->setLayout;
	t_AllocInfo.descriptorSetCount = STANDARD_DESCRIPTORSET_COUNT;
	//Lmao creat pool
	t_AllocInfo.descriptorPool = s_VKB.descriptorAllocator.GetPool();

	VkResult t_AllocResult = vkAllocateDescriptorSets(s_VKB.device.logicalDevice,
		&t_AllocInfo,
		&t_BindingSet->set);
	bool t_NeedReallocate = false;

	for (size_t i = 0; i < a_Info.bufferBinds.size(); i++)
	{
		t_Writes[i].dstSet = t_BindingSet->set;
	}

	switch (t_AllocResult)
	{
	case VK_SUCCESS: //Just continue
	case VK_ERROR_FRAGMENTED_POOL: //Implement checking later.
	case VK_ERROR_OUT_OF_POOL_MEMORY:
		//need a new pool.
		t_NeedReallocate = true;
		break;

	default:
		BB_ASSERT(false, "Vulkan: Something went very badly with vkAllocateDescriptorSets");
		break;
	}

	vkUpdateDescriptorSets(s_VKB.device.logicalDevice,
		static_cast<uint32_t>(a_Info.bufferBinds.size()),
		t_Writes,
		0,
		nullptr);

	BBfreeArr(s_VulkanAllocator, t_BufferBindings);
	BBfreeArr(s_VulkanAllocator, t_Writes);
	BBfreeArr(s_VulkanAllocator, t_BufferInfos);

	return RBindingSetHandle(t_BindingSet);

}

CommandQueueHandle BB::VulkanCreateCommandQueue(const RenderCommandQueueCreateInfo& a_Info)
{
	VulkanCommandQueue* t_Queue = s_VKB.cmdQueues.Get();
	uint32_t t_QueueIndex;

	switch (a_Info.queue)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		t_QueueIndex = s_VKB.device.queueIndices.graphics;
		break;
	case RENDER_QUEUE_TYPE::TRANSFER_COPY:
		t_QueueIndex = s_VKB.device.queueIndices.transfer;
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
		t_QueueIndex = s_VKB.device.queueIndices.compute;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Trying to get a device queue that you didn't setup yet.");
		break;
	}

	vkGetDeviceQueue(s_VKB.device.logicalDevice,
		t_QueueIndex,
		0,
		&t_Queue->queue);



	VkSemaphoreTypeCreateInfo t_TimelineSemInfo{};
	t_TimelineSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	t_TimelineSemInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	t_TimelineSemInfo.initialValue = 0;

	VkSemaphoreCreateInfo t_SemCreateInfo{};
	t_SemCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	t_SemCreateInfo.pNext = &t_TimelineSemInfo;

	vkCreateSemaphore(s_VKB.device.logicalDevice,
		&t_SemCreateInfo,
		nullptr,
		&t_Queue->timelineSemaphore);


	t_Queue->lastCompleteValue = 1;
	t_Queue->nextSemValue = 2;

	VkSemaphoreSignalInfo t_SigInfo{};
	t_SigInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
	t_SigInfo.semaphore = t_Queue->timelineSemaphore;
	t_SigInfo.value = t_Queue->lastCompleteValue;
	vkSignalSemaphore(s_VKB.device.logicalDevice,
		&t_SigInfo);

	return CommandQueueHandle(t_Queue);
}

CommandAllocatorHandle BB::VulkanCreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo)
{
	VkCommandAllocator* t_CmdAllocator = s_VKB.cmdAllocators.Get();

	VkCommandPoolCreateInfo t_CreateInfo{};
	t_CreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	switch (a_CreateInfo.queueType)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		t_CreateInfo.queueFamilyIndex = s_VKB.device.queueIndices.graphics;
		break;
	case RENDER_QUEUE_TYPE::TRANSFER_COPY:
		t_CreateInfo.queueFamilyIndex = s_VKB.device.queueIndices.transfer;
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
		t_CreateInfo.queueFamilyIndex = s_VKB.device.queueIndices.compute;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Tried to make a command allocator with a queue type that does not exist.");
		break;
	}

	VKASSERT(vkCreateCommandPool(s_VKB.device.logicalDevice,
		&t_CreateInfo,
		nullptr,
		&t_CmdAllocator->pool),
		"Vulkan: Failed to create command pool.");

	t_CmdAllocator->buffers.CreatePool(s_VulkanAllocator, a_CreateInfo.commandListCount);

	VkCommandBufferAllocateInfo t_AllocCreateInfo{};
	t_AllocCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	t_AllocCreateInfo.commandPool = t_CmdAllocator->pool;
	t_AllocCreateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	t_AllocCreateInfo.commandBufferCount = a_CreateInfo.commandListCount;

	VKASSERT(vkAllocateCommandBuffers(s_VKB.device.logicalDevice,
		&t_AllocCreateInfo,
		t_CmdAllocator->buffers.data()),
		"Vulkan: Failed to allocate command buffers!");

	return t_CmdAllocator; //Creates a handle from this.
}

CommandListHandle BB::VulkanCreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo)
{
	BB_ASSERT(a_CreateInfo.commandAllocator.handle != NULL, "Sending a commandallocator handle that is null!");
	return CommandListHandle(s_VKB.commandLists.insert(reinterpret_cast<VkCommandAllocator*>(a_CreateInfo.commandAllocator.ptrHandle)->GetCommandList()).handle);
}

RFenceHandle BB::VulkanCreateFence(const FenceCreateInfo& a_Info)
{
	VkSemaphoreTypeCreateInfo t_TimelineSemInfo{};
	t_TimelineSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	t_TimelineSemInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	t_TimelineSemInfo.initialValue = 0;

	VkSemaphoreCreateInfo t_SemCreateInfo{};
	t_SemCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	t_SemCreateInfo.pNext = &t_TimelineSemInfo;

	VkSemaphore t_TimelineSem;
	vkCreateSemaphore(s_VKB.device.logicalDevice,
		&t_SemCreateInfo,
		nullptr,
		&t_TimelineSem);

	return RFenceHandle(t_TimelineSem);
}

PipelineBuilderHandle BB::VulkanPipelineBuilderInit(const PipelineInitInfo& t_InitInfo)
{
	VKPipelineBuildInfo* t_BuildInfo = BBnew(s_VulkanAllocator, VKPipelineBuildInfo);

	//We do dynamic rendering to avoid having to handle renderpasses and such.
	t_BuildInfo->dynamicRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	t_BuildInfo->dynamicRenderingInfo.colorAttachmentCount = 1;
	t_BuildInfo->dynamicRenderingInfo.pColorAttachmentFormats = &s_VKB.swapChain.imageFormat;
	t_BuildInfo->dynamicRenderingInfo.pNext = nullptr;

	
	t_BuildInfo->pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	t_BuildInfo->pipeInfo.subpass = 0;
	t_BuildInfo->pipeInfo.renderPass = nullptr; //We not using em anymore! Dynamic rendering enabled.
	t_BuildInfo->pipeInfo.pNext = &t_BuildInfo->dynamicRenderingInfo;

	return PipelineBuilderHandle(t_BuildInfo);
}

void BB::VulkanPipelineBuilderBindBindingSet(const PipelineBuilderHandle a_Handle, const RBindingSetHandle a_BindingSetHandle)
{
	constexpr uint32_t MAX_PUSHCONSTANTSIZE = 128;
	VKPipelineBuildInfo* t_BuildInfo = reinterpret_cast<VKPipelineBuildInfo*>(a_Handle.ptrHandle);
	VulkanBindingSet* t_Set = reinterpret_cast<VulkanBindingSet*>(a_BindingSetHandle.ptrHandle);

	t_BuildInfo->layout[t_BuildInfo->layoutCount++] = t_Set->setLayout;

	for (uint32_t i = 0; i < t_Set->pushConstantCount; i++)
	{
		t_BuildInfo->constantRanges[i].offset = t_Set->pushConstants[i].offset;
		t_BuildInfo->constantRanges[i].stageFlags = t_Set->pushConstants[i].shaderStage;
		t_BuildInfo->constantRanges[i].size = MAX_PUSHCONSTANTSIZE;
	}

	t_BuildInfo->constantRangeCount = t_Set->pushConstantCount;
}

void BB::VulkanPipelineBuilderBindShaders(const PipelineBuilderHandle a_Handle, const Slice<BB::ShaderCreateInfo> a_ShaderInfo)
{
	VKPipelineBuildInfo* t_BuildInfo = reinterpret_cast<VKPipelineBuildInfo*>(a_Handle.ptrHandle);

	t_BuildInfo->shaderInfo = CreateShaderModules(
		t_BuildInfo->buildAllocator,
		s_VKB.device.logicalDevice,
		a_ShaderInfo);

	t_BuildInfo->pipeInfo.pStages = t_BuildInfo->shaderInfo.pipelineShaderStageInfo;
	t_BuildInfo->pipeInfo.stageCount = a_ShaderInfo.size();
}

PipelineHandle BB::VulkanPipelineBuildPipeline(const PipelineBuilderHandle a_Handle)
{
	VulkanPipeline t_Pipeline{};
	VKPipelineBuildInfo* t_BuildInfo = reinterpret_cast<VKPipelineBuildInfo*>(a_Handle.ptrHandle);

	{
		//layout
		t_Pipeline.layout = CreatePipelineLayout(
			BB::Slice<VkDescriptorSetLayout>(t_BuildInfo->layout, t_BuildInfo->layoutCount),
			BB::Slice<VkPushConstantRange>(t_BuildInfo->constantRanges, t_BuildInfo->constantRangeCount));

		t_BuildInfo->pipeInfo.layout = t_Pipeline.layout;
	}

	{ //Create the pipeline.
		//Get dynamic state for the viewport and scissor.
		VkDynamicState t_DynamicStates[2]{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo t_DynamicPipeCreateInfo{};
		t_DynamicPipeCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		t_DynamicPipeCreateInfo.dynamicStateCount = 2;
		t_DynamicPipeCreateInfo.pDynamicStates = t_DynamicStates;

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

		//viewport is always controlled by the dynamic state so we just initialize them here.
		t_BuildInfo->pipeInfo.pViewportState = &t_ViewportState;
		t_BuildInfo->pipeInfo.pDynamicState = &t_DynamicPipeCreateInfo;
		t_BuildInfo->pipeInfo.pVertexInputState = &t_VertexInput;
		t_BuildInfo->pipeInfo.pInputAssemblyState = &t_InputAssembly;
		t_BuildInfo->pipeInfo.pRasterizationState = &t_Rasterizer;
		t_BuildInfo->pipeInfo.pMultisampleState = &t_MultiSampling;

		//THIS IS TEMP! We want to build these ourselves with the builder.
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

		t_BuildInfo->pipeInfo.pDepthStencilState = &t_DepthStencil;
		t_BuildInfo->pipeInfo.pColorBlendState = &t_ColorBlending;

		//Optimalization for later.
		t_BuildInfo->pipeInfo.basePipelineHandle = VK_NULL_HANDLE;
		t_BuildInfo->pipeInfo.basePipelineIndex = -1;

		VKASSERT(vkCreateGraphicsPipelines(s_VKB.device.logicalDevice,
			VK_NULL_HANDLE,
			1,
			&t_BuildInfo->pipeInfo,
			nullptr,
			&t_Pipeline.pipeline),
			"Vulkan: Failed to create graphics Pipeline.");

		for (uint32_t i = 0; i < t_BuildInfo->pipeInfo.stageCount; i++)
		{
			vkDestroyShaderModule(s_VKB.device.logicalDevice,
				t_BuildInfo->shaderInfo.shaderModules[i],
				nullptr);
		}
	}

	VulkanPipeline* t_ReturnPipeline = s_VKB.pipelinePool.Get();
	*t_ReturnPipeline = t_Pipeline;

	BBfree(s_VulkanAllocator, t_BuildInfo);

	return PipelineHandle(t_ReturnPipeline);
}



void BB::VulkanResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle)
{
	//Wait for fence.
	VkCommandAllocator* t_CmdAllocator = reinterpret_cast<VkCommandAllocator*>(a_CmdAllocatorHandle.ptrHandle);

	vkResetCommandPool(s_VKB.device.logicalDevice,
		t_CmdAllocator->pool,
		0);
}

RecordingCommandListHandle BB::VulkanStartCommandList(const CommandListHandle a_CmdHandle)
{
	VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_CmdHandle.handle];

	//vkResetCommandBuffer(a_CmdList.buffers[a_CmdList.currentFree], 0);
	VkCommandBufferBeginInfo t_CmdBeginInfo{};
	t_CmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VKASSERT(vkBeginCommandBuffer(t_Cmdlist.Buffer(),
		&t_CmdBeginInfo),
		"Vulkan: Failed to begin commandbuffer");

	return RecordingCommandListHandle(&t_Cmdlist);
}

void BB::VulkanEndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);

	t_Cmdlist->currentPipelineLayout = VK_NULL_HANDLE;

	VKASSERT(vkEndCommandBuffer(t_Cmdlist->Buffer()),
		"Vulkan: Error when trying to end commandbuffer!");
}

void BB::VulkanStartRenderPass(const RecordingCommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_RenderInfo)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);

	VkImageMemoryBarrier t_PresentBarrier{};
	t_PresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	t_PresentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	t_PresentBarrier.oldLayout = VKConv::ImageLayout(a_RenderInfo.colorInitialLayout);
	t_PresentBarrier.newLayout = VKConv::ImageLayout(a_RenderInfo.colorFinalLayout);
	t_PresentBarrier.image = s_VKB.swapChain.images[s_VKB.currentFrame];
	t_PresentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	t_PresentBarrier.subresourceRange.baseArrayLayer = 0;
	t_PresentBarrier.subresourceRange.layerCount = 1;
	t_PresentBarrier.subresourceRange.baseMipLevel = 0;
	t_PresentBarrier.subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(t_Cmdlist->Buffer(),
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&t_PresentBarrier);

	VkRenderingAttachmentInfo t_RenderColorAttach{};
	t_RenderColorAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	t_RenderColorAttach.loadOp = VKConv::LoadOP(a_RenderInfo.colorLoadOp);
	t_RenderColorAttach.storeOp = VKConv::StoreOp(a_RenderInfo.colorStoreOp);
	t_RenderColorAttach.imageLayout = VKConv::ImageLayout(a_RenderInfo.colorFinalLayout); //Get the layout after the memory barrier.
	t_RenderColorAttach.imageView = s_VKB.swapChain.imageViews[s_VKB.currentFrame];
	t_RenderColorAttach.clearValue.color.float32[0] = a_RenderInfo.clearColor[0];
	t_RenderColorAttach.clearValue.color.float32[1] = a_RenderInfo.clearColor[1];
	t_RenderColorAttach.clearValue.color.float32[2] = a_RenderInfo.clearColor[2];
	t_RenderColorAttach.clearValue.color.float32[3] = a_RenderInfo.clearColor[3];

	VkRenderingInfo t_RenderInfo{};
	t_RenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	t_RenderInfo.renderArea = VkInit::Rect2D(0, 0, s_VKB.swapChain.extent);
	t_RenderInfo.layerCount = 1;
	t_RenderInfo.pColorAttachments = &t_RenderColorAttach;
	t_RenderInfo.colorAttachmentCount = 1;
	t_RenderInfo.pNext = nullptr;

	vkCmdBeginRendering(t_Cmdlist->Buffer(), &t_RenderInfo);

	VkViewport t_Viewport{};
	t_Viewport.x = 0.0f;
	t_Viewport.y = 0.0f;
	t_Viewport.width = static_cast<float>(s_VKB.swapChain.extent.width);
	t_Viewport.height = static_cast<float>(s_VKB.swapChain.extent.height);
	t_Viewport.minDepth = 0.0f;
	t_Viewport.maxDepth = 1.0f;
	vkCmdSetViewport(t_Cmdlist->Buffer(), 0, 1, &t_Viewport);

	VkRect2D t_Scissor{};
	t_Scissor.offset = { 0, 0 };
	t_Scissor.extent = s_VKB.swapChain.extent;
	vkCmdSetScissor(t_Cmdlist->Buffer(), 0, 1, &t_Scissor);
}

void BB::VulkanEndRenderPass(const RecordingCommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);
	vkCmdEndRendering(t_Cmdlist->Buffer());

	VkImageMemoryBarrier t_PresentBarrier{};
	t_PresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	t_PresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	t_PresentBarrier.oldLayout = VKConv::ImageLayout(a_EndInfo.colorInitialLayout);
	t_PresentBarrier.newLayout = VKConv::ImageLayout(a_EndInfo.colorFinalLayout);
	t_PresentBarrier.image = s_VKB.swapChain.images[s_VKB.currentFrame];
	t_PresentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	t_PresentBarrier.subresourceRange.baseArrayLayer = 0;
	t_PresentBarrier.subresourceRange.layerCount = 1;
	t_PresentBarrier.subresourceRange.baseMipLevel = 0;
	t_PresentBarrier.subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(t_Cmdlist->Buffer(),
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&t_PresentBarrier);
}

void BB::VulkanBindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);

	const VulkanPipeline* t_Pipeline = reinterpret_cast<VulkanPipeline*>(a_Pipeline.handle);

	vkCmdBindPipeline(t_Cmdlist->Buffer(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		t_Pipeline->pipeline);

	t_Cmdlist->currentPipelineLayout = t_Pipeline->layout;
}

void BB::VulkanBindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);

	//quick cheat.
	VkBuffer t_Buffers[12]{};
	for (size_t i = 0; i < a_BufferCount; i++)
	{
		t_Buffers[i] = reinterpret_cast<VulkanBuffer*>(a_Buffers[i].ptrHandle)->buffer;
	}

	vkCmdBindVertexBuffers(t_Cmdlist->Buffer(),
		0,
		static_cast<uint32_t>(a_BufferCount),
		t_Buffers,
		a_BufferOffsets);
}

void BB::VulkanBindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);

	vkCmdBindIndexBuffer(t_Cmdlist->Buffer(),
		reinterpret_cast<VulkanBuffer*>(a_Buffer.ptrHandle)->buffer,
		a_Offset,
		VK_INDEX_TYPE_UINT32);
}

void BB::VulkanBindBindingSets(const RecordingCommandListHandle a_RecordingCmdHandle, const RBindingSetHandle* a_Sets, const uint32_t a_SetCount, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);
	VkDescriptorSet t_BindSets[4]{};

	for (uint32_t i = 0; i < a_SetCount; i++)
	{
		t_BindSets[i] = reinterpret_cast<VulkanBindingSet*>(a_Sets[i].ptrHandle)->set;
	}

	vkCmdBindDescriptorSets(t_Cmdlist->Buffer(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		t_Cmdlist->currentPipelineLayout, //Set pipeline layout.
		static_cast<uint32_t>(reinterpret_cast<VulkanBindingSet*>(a_Sets[0].ptrHandle)->bindingSet),
		a_SetCount,
		t_BindSets,
		a_DynamicOffsetCount,
		a_DynamicOffsets);
}

void BB::VulkanBindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const RBindingSetHandle a_Set, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_Offset, const void* a_Data)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);
	const VulkanBindingSet* t_Set = reinterpret_cast<VulkanBindingSet*>(a_Set.ptrHandle);
	BB_ASSERT(a_ConstantIndex < t_Set->pushConstantCount, "Vulkan: push constant index points to an index that does not exist!");
	
	vkCmdPushConstants(t_Cmdlist->Buffer(),
		t_Cmdlist->currentPipelineLayout,
		t_Set->pushConstants[a_ConstantIndex].shaderStage,
		//VKConv::ShaderStageBits(a_Stage),
		a_Offset,
		a_DwordCount * sizeof(uint32_t), //we do Dword count to help dx12 more.
		a_Data);
}

void BB::VulkanDrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);

	vkCmdDraw(t_Cmdlist->Buffer(), a_VertexCount, a_InstanceCount, a_FirstVertex, a_FirstInstance);
}

void BB::VulkanDrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);

	vkCmdDrawIndexed(t_Cmdlist->Buffer(), a_IndexCount, a_InstanceCount, a_FirstIndex, a_VertexOffset, a_FirstInstance);
}

void BB::VulkanBufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset)
{
	VulkanBuffer* t_Buffer = reinterpret_cast<VulkanBuffer*>(a_Handle.handle);
	void* t_MapData;
	VKASSERT(vmaMapMemory(s_VKB.vma,
		t_Buffer->allocation,
		&t_MapData),
		"Vulkan: Failed to map memory");
	memcpy(Pointer::Add(t_MapData, a_Offset), a_Data, a_Size);
	vmaUnmapMemory(s_VKB.vma, t_Buffer->allocation);
}

void BB::VulkanCopyBuffer(Allocator a_TempAllocator, const RenderCopyBufferInfo& a_CopyInfo)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_CopyInfo.transferCommandHandle.ptrHandle);
	VulkanBuffer* t_SrcBuffer = reinterpret_cast<VulkanBuffer*>(a_CopyInfo.src.handle);
	VulkanBuffer* t_DstBuffer = reinterpret_cast<VulkanBuffer*>(a_CopyInfo.dst.handle);

	VkBufferCopy* t_CopyRegion = BBnewArr(a_TempAllocator, a_CopyInfo.CopyRegionCount, VkBufferCopy);
	for (size_t i = 0; i < a_CopyInfo.CopyRegionCount; i++)
	{
		t_CopyRegion[i].srcOffset = a_CopyInfo.copyRegions[i].srcOffset;
		t_CopyRegion[i].dstOffset = a_CopyInfo.copyRegions[i].dstOffset;
		t_CopyRegion[i].size = a_CopyInfo.copyRegions[i].size;
	}

	vkCmdCopyBuffer(t_Cmdlist->Buffer(),
		t_SrcBuffer->buffer,
		t_DstBuffer->buffer,
		static_cast<uint32_t>(a_CopyInfo.CopyRegionCount),
		t_CopyRegion);
}

void* BB::VulkanMapMemory(const RBufferHandle a_Handle)
{
	void* t_MapData;
	VKASSERT(vmaMapMemory(s_VKB.vma,
		reinterpret_cast<VulkanBuffer*>(a_Handle.ptrHandle)->allocation,
		&t_MapData),
		"Vulkan: Failed to map memory");

	return t_MapData;
}

void BB::VulkanUnMemory(const RBufferHandle a_Handle)
{
	vmaUnmapMemory(s_VKB.vma, reinterpret_cast<VulkanBuffer*>(a_Handle.ptrHandle)->allocation);
}

void BB::VulkanResizeWindow(Allocator a_TempAllocator, const uint32_t a_X, const uint32_t a_Y)
{
	VulkanWaitDeviceReady();

	for (size_t i = 0; i < s_VKB.frameCount; i++)
	{
		vkDestroyImageView(s_VKB.device.logicalDevice,
			s_VKB.swapChain.imageViews[i],
			nullptr);
	}

	//Creates the swapchain with the image views.
	CreateSwapchain(s_VKB.swapChain,
		a_TempAllocator,
		s_VKB.surface,
		s_VKB.device.physicalDevice,
		s_VKB.device.logicalDevice,
		a_X,
		a_Y,
		true);
}

void BB::VulkanStartFrame(Allocator a_TempAllocator, const StartFrameInfo& a_StartInfo)
{
	FrameIndex t_CurrentFrame = s_VKB.currentFrame;

	VKASSERT(vkAcquireNextImageKHR(s_VKB.device.logicalDevice,
		s_VKB.swapChain.swapChain,
		UINT64_MAX,
		s_VKB.swapChain.waitSyncs[s_VKB.currentFrame].imageAvailableSem,
		VK_NULL_HANDLE,
		&s_VKB.imageIndex),
		"Vulkan: failed to get next image.");

	//For now not wait for semaphores, may be required later.
	VkSemaphoreWaitInfo t_WaitInfo{};
	t_WaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	t_WaitInfo.semaphoreCount = 1;
	t_WaitInfo.pSemaphores = &s_VKB.swapChain.waitSyncs[s_VKB.currentFrame].frameTimelineSemaphore;
	t_WaitInfo.pValues = &s_VKB.swapChain.waitSyncs[s_VKB.currentFrame].frameWaitValue;
	vkWaitSemaphores(s_VKB.device.logicalDevice, &t_WaitInfo, 1000000000);
}

void BB::VulkanExecuteCommands(Allocator a_TempAllocator, CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount)
{
	VkPipelineStageFlags t_WaitStagesMask[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };

	VkTimelineSemaphoreSubmitInfo* t_TimelineInfos = BBnewArr(
		a_TempAllocator,
		a_ExecuteInfoCount,
		VkTimelineSemaphoreSubmitInfo);
	VkSubmitInfo* t_SubmitInfos = BBnewArr(
		a_TempAllocator,
		a_ExecuteInfoCount,
		VkSubmitInfo);

	for (uint32_t i = 0; i < a_ExecuteInfoCount; i++)
	{
		VkCommandBuffer* t_CmdBuffers = BBnewArr(a_TempAllocator,
			a_ExecuteInfos[i].commandCount,
			VkCommandBuffer);
		for (uint32_t j = 0; j < a_ExecuteInfos[i].commandCount; j++)
		{
			t_CmdBuffers[j] = s_VKB.commandLists[a_ExecuteInfos[i].commands[j].handle].Buffer();
		}

		const uint32_t t_WaitSemCount = a_ExecuteInfos[i].waitQueueCount;
		const uint32_t t_SignalSemCount = a_ExecuteInfos[i].signalQueueCount;

		VkSemaphore* t_Semaphores = BBnewArr(a_TempAllocator,
			t_WaitSemCount + t_SignalSemCount + 1,
			VkSemaphore);
		uint64_t* t_SemValues = BBnewArr(a_TempAllocator,
			t_WaitSemCount + t_SignalSemCount + 1,
			uint64_t);

		//SETTING THE WAIT
		for (uint32_t j = 0; j < t_WaitSemCount; j++)
		{
			t_Semaphores[j] = reinterpret_cast<VulkanCommandQueue*>(
				a_ExecuteInfos[i].waitQueues[j].ptrHandle)->timelineSemaphore;
			t_SemValues[j] = a_ExecuteInfos[i].waitValues[j];;
		}

		//SETTING THE SIGNAL
		for (uint32_t j = 0; j < t_SignalSemCount; j++)
		{
			t_Semaphores[j + t_WaitSemCount] = reinterpret_cast<VulkanCommandQueue*>(
				a_ExecuteInfos[i].signalQueues[j].ptrHandle)->timelineSemaphore;
			//Increment the next sem value for signal
			t_SemValues[j + t_WaitSemCount] = reinterpret_cast<VulkanCommandQueue*>(
				a_ExecuteInfos[i].signalQueues[j].ptrHandle)->nextSemValue++;
		}

		t_TimelineInfos[i].sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		t_TimelineInfos[i].waitSemaphoreValueCount = t_WaitSemCount;
		t_TimelineInfos[i].pWaitSemaphoreValues = t_SemValues;
		t_TimelineInfos[i].signalSemaphoreValueCount = t_SignalSemCount;
		t_TimelineInfos[i].pSignalSemaphoreValues = &t_SemValues[t_WaitSemCount];
		t_TimelineInfos[i].pNext = nullptr;

		t_SubmitInfos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		t_SubmitInfos[i].waitSemaphoreCount = t_WaitSemCount;
		t_SubmitInfos[i].pWaitSemaphores = t_Semaphores;
		t_SubmitInfos[i].pWaitDstStageMask = t_WaitStagesMask;
		t_SubmitInfos[i].signalSemaphoreCount = t_SignalSemCount;
		t_SubmitInfos[i].pSignalSemaphores = &t_Semaphores[t_WaitSemCount]; //Get the semaphores after all the wait sems
		t_SubmitInfos[i].commandBufferCount = a_ExecuteInfos[i].commandCount;
		t_SubmitInfos[i].pCommandBuffers = t_CmdBuffers;

		t_SubmitInfos[i].pNext = &t_TimelineInfos[i];
	}

	VulkanCommandQueue t_Queue = *reinterpret_cast<VulkanCommandQueue*>(a_ExecuteQueue.ptrHandle);
	VKASSERT(vkQueueSubmit(t_Queue.queue,
		a_ExecuteInfoCount,
		t_SubmitInfos,
		VK_NULL_HANDLE),
		"Vulkan: failed to submit to queue.");
}

void BB::VulkanExecutePresentCommand(Allocator a_TempAllocator, CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo)
{
	VkCommandBuffer* t_CmdBuffers = BBnewArr(a_TempAllocator,
		a_ExecuteInfo.commandCount,
		VkCommandBuffer);
	for (uint32_t j = 0; j < a_ExecuteInfo.commandCount; j++)
	{
		t_CmdBuffers[j] = s_VKB.commandLists[a_ExecuteInfo.commands[j].handle].Buffer();
	}

	//add 1 more to wait the binary semaphore for image presenting
	const uint32_t t_WaitSemCount = a_ExecuteInfo.waitQueueCount + 1;
	//add 1 more to signal the binary semaphore for image presenting
	//Add 1 additional more to signal if the rendering of this frame is complete. Hacky and not totally accurate however. Might use the queue values for it later.
	const uint32_t t_SignalSemCount = a_ExecuteInfo.signalQueueCount + 2;

	VkSemaphore* t_Semaphores = BBnewArr(a_TempAllocator,
		t_WaitSemCount + t_SignalSemCount,
		VkSemaphore);
	uint64_t* t_SemValues = BBnewArr(a_TempAllocator,
		t_WaitSemCount + t_SignalSemCount,
		uint64_t);

	//SETTING THE WAIT
	//Set the wait semaphore so that it must wait until it can present.
	t_Semaphores[0] = s_VKB.swapChain.waitSyncs[s_VKB.currentFrame].imageAvailableSem;
	t_SemValues[0] = 0;
	//Get the semaphore from the queues.
	for (uint32_t i = 0; i < t_WaitSemCount - 1; i++)
	{
		t_Semaphores[i + 1] = reinterpret_cast<VulkanCommandQueue*>(
			a_ExecuteInfo.waitQueues[i].ptrHandle)->timelineSemaphore;
		t_SemValues[i + 1] = a_ExecuteInfo.waitValues[i];
	}

	//SETTING THE SIGNAL
	//signal the binary semaphore to signal that the image is being worked on.
	t_Semaphores[t_WaitSemCount] = s_VKB.swapChain.waitSyncs[s_VKB.currentFrame].imageRenderFinishedSem;
	t_SemValues[t_WaitSemCount] = 0;
	//signal the binary semaphore to signal that the image is being worked on.
	t_Semaphores[t_WaitSemCount + 1] = s_VKB.swapChain.waitSyncs[s_VKB.currentFrame].frameTimelineSemaphore;
	//Increment the semaphore by 1 for the next frame to get.
	t_SemValues[t_WaitSemCount + 1] = ++s_VKB.swapChain.waitSyncs[s_VKB.currentFrame].frameWaitValue;
	for (uint32_t i = 0; i < t_SignalSemCount - 2; i++)
	{
		t_Semaphores[t_WaitSemCount + i + 1] = reinterpret_cast<VulkanCommandQueue*>(
			a_ExecuteInfo.signalQueues[i].ptrHandle)->timelineSemaphore;
		//Increment the next sem value for signal
		t_SemValues[t_WaitSemCount + i + 1] = reinterpret_cast<VulkanCommandQueue*>(
			a_ExecuteInfo.signalQueues[i].ptrHandle)->nextSemValue++;
	}

	VkPipelineStageFlags t_WaitStagesMask[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };

	VkTimelineSemaphoreSubmitInfo t_TimelineInfo{};
	t_TimelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	t_TimelineInfo.waitSemaphoreValueCount = t_WaitSemCount;
	t_TimelineInfo.pWaitSemaphoreValues = t_SemValues;
	t_TimelineInfo.signalSemaphoreValueCount = t_SignalSemCount;
	t_TimelineInfo.pSignalSemaphoreValues = &t_SemValues[t_WaitSemCount];
	t_TimelineInfo.pNext = nullptr;

	VkSubmitInfo t_SubmitInfo{};
	t_SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	t_SubmitInfo.waitSemaphoreCount = t_WaitSemCount;
	t_SubmitInfo.pWaitSemaphores = t_Semaphores;
	t_SubmitInfo.pWaitDstStageMask = t_WaitStagesMask;
	t_SubmitInfo.signalSemaphoreCount = t_SignalSemCount;
	t_SubmitInfo.pSignalSemaphores = &t_Semaphores[t_WaitSemCount]; //Get the semaphores after all the wait sems
	t_SubmitInfo.commandBufferCount = a_ExecuteInfo.commandCount;
	t_SubmitInfo.pCommandBuffers = t_CmdBuffers;
	t_SubmitInfo.pNext = &t_TimelineInfo;

	VulkanCommandQueue t_Queue = *reinterpret_cast<VulkanCommandQueue*>(a_ExecuteQueue.ptrHandle);
	VKASSERT(vkQueueSubmit(t_Queue.queue,
		1,
		&t_SubmitInfo,
		VK_NULL_HANDLE),
		"Vulkan: failed to submit to queue.");
}

FrameIndex BB::VulkanPresentFrame(Allocator a_TempAllocator, const PresentFrameInfo& a_PresentInfo)
{
	const uint32_t t_CurrentFrame = s_VKB.currentFrame;

	VkPresentInfoKHR t_PresentInfo{};
	t_PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	t_PresentInfo.waitSemaphoreCount = 1;
	t_PresentInfo.pWaitSemaphores = &s_VKB.swapChain.waitSyncs[s_VKB.currentFrame].imageRenderFinishedSem;
	t_PresentInfo.swapchainCount = 1; //Swapchain will always be 1
	t_PresentInfo.pSwapchains = &s_VKB.swapChain.swapChain;
	t_PresentInfo.pImageIndices = &s_VKB.imageIndex;
	t_PresentInfo.pResults = nullptr;

	VKASSERT(vkQueuePresentKHR(s_VKB.device.presentQueue, &t_PresentInfo),
		"Vulkan: Failed to queuepresentKHR.");

	return s_VKB.currentFrame = (s_VKB.currentFrame + 1) % s_VKB.frameCount;
}

uint64_t BB::VulkanNextQueueFenceValue(const CommandQueueHandle a_Handle)
{
	return reinterpret_cast<VulkanCommandQueue*>(a_Handle.ptrHandle)->nextSemValue;
}

//NOT IMPLEMENTED YET.
uint64_t BB::VulkanNextFenceValue(const RFenceHandle a_Handle)
{
	return 0; //return reinterpret_cast<VkSemaphore*>(a_Handle.ptrHandle)->nextSemValue;
}

void BB::VulkanWaitDeviceReady()
{
	vkDeviceWaitIdle(s_VKB.device.logicalDevice);
}

void BB::VulkanDestroyFence(const RFenceHandle a_Handle)
{
	vkDestroyFence(s_VKB.device.logicalDevice,
		reinterpret_cast<VkFence>(a_Handle.ptrHandle),
		nullptr);
}

void BB::VulkanDestroyCommandQueue(const CommandQueueHandle a_Handle)
{
	VulkanCommandQueue* t_CmdQueue = reinterpret_cast<VulkanCommandQueue*>(a_Handle.ptrHandle);
	vkDestroySemaphore(s_VKB.device.logicalDevice,
		t_CmdQueue->timelineSemaphore,
		nullptr);

	t_CmdQueue->queue = nullptr;
	t_CmdQueue->timelineSemaphore = nullptr;
	t_CmdQueue->nextSemValue = 0;
	t_CmdQueue->lastCompleteValue = 0;
}

void BB::VulkanDestroyCommandAllocator(const CommandAllocatorHandle a_Handle)
{
	VkCommandAllocator* t_CmdAllocator = reinterpret_cast<VkCommandAllocator*>(a_Handle.ptrHandle);
	t_CmdAllocator->buffers.DestroyPool(s_VulkanAllocator);
	vkDestroyCommandPool(s_VKB.device.logicalDevice, t_CmdAllocator->pool, nullptr);
	s_VKB.cmdAllocators.Free(t_CmdAllocator);
}

void BB::VulkanDestroyCommandList(const CommandListHandle a_Handle)
{
	VulkanCommandList& a_List = s_VKB.commandLists[a_Handle.handle];
	a_List.cmdAllocator->FreeCommandList(a_List); //Place back in the freelist.
	s_VKB.commandLists.erase(a_Handle.handle);
}

void BB::VulkanDestroyBindingSet(const RBindingSetHandle a_Handle)
{
	VulkanBindingSet* t_Set = reinterpret_cast<VulkanBindingSet*>(a_Handle.ptrHandle);
	*t_Set = {}; //zero it for safety
	//maybe store the sets? For now we just get new ones.
	s_VKB.bindingSetPool.Free(t_Set);
}

void BB::VulkanDestroyPipeline(const PipelineHandle a_Handle)
{
	VulkanPipeline* t_Pipeline = reinterpret_cast<VulkanPipeline*>(a_Handle.handle);

	vkDestroyPipeline(s_VKB.device.logicalDevice,
		t_Pipeline->pipeline,
		nullptr);
	vkDestroyDescriptorSetLayout(s_VKB.device.logicalDevice,
		t_Pipeline->setLayout,
		nullptr);
}

void BB::VulkanDestroyBackend()
{
	for (auto t_It = s_VKB.pipelineLayouts.begin();
		t_It < s_VKB.pipelineLayouts.end(); t_It++)
	{
		vkDestroyPipelineLayout(s_VKB.device.logicalDevice,
			*t_It->value,
			nullptr);
	}
	s_VKB.pipelineLayouts.clear();

	s_VKB.descriptorAllocator.Destroy();

	for (size_t i = 0; i < s_VKB.frameCount; i++)
	{
		vkDestroyImageView(s_VKB.device.logicalDevice,
			s_VKB.swapChain.imageViews[i], nullptr);
		vkDestroySemaphore(s_VKB.device.logicalDevice,
			s_VKB.swapChain.waitSyncs[i].frameTimelineSemaphore, nullptr);
		vkDestroySemaphore(s_VKB.device.logicalDevice,
			s_VKB.swapChain.waitSyncs[i].imageAvailableSem, nullptr);
		vkDestroySemaphore(s_VKB.device.logicalDevice,
			s_VKB.swapChain.waitSyncs[i].imageRenderFinishedSem, nullptr);
	}

	vkDestroySwapchainKHR(s_VKB.device.logicalDevice,
		s_VKB.swapChain.swapChain,
		nullptr);
	vmaDestroyAllocator(s_VKB.vma);
	vkDestroyDevice(s_VKB.device.logicalDevice, nullptr);

	if (s_VKB.vulkanDebug.debugMessenger != 0)
		DestroyVulkanDebug(s_VKB.instance, s_VKB.vulkanDebug.debugMessenger);

	vkDestroySurfaceKHR(s_VKB.instance, s_VKB.surface, nullptr);
	vkDestroyInstance(s_VKB.instance, nullptr);

	s_VKB.DestroyPools();

	//clear all the vulkan memory.
	//s_VulkanAllocator.Clear();
}
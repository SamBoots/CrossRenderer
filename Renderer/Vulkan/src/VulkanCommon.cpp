#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1003000 // Vulkan 1.2
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

constexpr int VULKAN_VERSION = 3;

#include "Storage/Hashmap.h"
#include "Storage/Slotmap.h"
#include "Storage/Pool.h"
#include "Allocators/RingAllocator.h"
#include "Allocators/TemporaryAllocator.h"
#include "BBMemory.h"
#include "Math.inl"

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

struct VulkanImage
{
	VkImage image;
	VmaAllocation allocation;
	VkImageView view;

	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint16_t mips;
	uint16_t arrays;
};

static FreelistAllocator_t s_VulkanAllocator{ mbSize * 2 };
//This allocator is what we use to temporary allocate elements.
static RingAllocator s_VulkanTempAllocator{ s_VulkanAllocator, kbSize * 64 };

struct VkCommandAllocator;

struct VulkanCommandList
{
	VkCommandBuffer* bufferPtr;

	//Cached variables
	VkPipelineLayout currentPipelineLayout = VK_NULL_HANDLE;

	VkCommandAllocator* cmdAllocator;
	VkImage depthImage = VK_NULL_HANDLE;

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

	VkPipelineLayoutCreateInfo pipeLayoutInfo{};

	uint32_t layoutCount;
	VkDescriptorSetLayout layout[BINDING_MAX];
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

	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkQueue presentQueue;

	VulkanQueuesIndices queueIndices;

	VulkanSwapChain swapChain{};
	VmaAllocator vma{};
	Slotmap<VulkanCommandList> commandLists{ s_VulkanAllocator };

	Pool<VulkanCommandQueue> cmdQueues;
	Pool<VkCommandAllocator> cmdAllocators;
	Pool<VulkanPipeline> pipelinePool;
	Pool<VulkanBuffer> bufferPool;
	Pool<VulkanImage> imagePool;
	Pool<VulkanBindingSet> bindingSetPool;

	OL_HashMap<PipelineLayoutHash, VkPipelineLayout> pipelineLayouts{ s_VulkanAllocator };

	VulkanDebug vulkanDebug;
	VulkanPhysicalDeviceInfo deviceInfo;

	void CreatePools()
	{
		cmdQueues.CreatePool(s_VulkanAllocator, 8);
		cmdAllocators.CreatePool(s_VulkanAllocator, 8);
		pipelinePool.CreatePool(s_VulkanAllocator, 8);
		bufferPool.CreatePool(s_VulkanAllocator, 16);
		imagePool.CreatePool(s_VulkanAllocator, 16);
		bindingSetPool.CreatePool(s_VulkanAllocator, 16);
	}

	void DestroyPools()
	{
		cmdQueues.DestroyPool(s_VulkanAllocator);
		cmdAllocators.DestroyPool(s_VulkanAllocator);
		pipelinePool.DestroyPool(s_VulkanAllocator);
		bufferPool.DestroyPool(s_VulkanAllocator);
		imagePool.DestroyPool(s_VulkanAllocator);
		bindingSetPool.DestroyPool(s_VulkanAllocator);
	}
};
static VulkanBackend_inst s_VKB;

static VkDeviceSize PadUBOBufferSize(const VkDeviceSize a_BuffSize)
{
	VkPhysicalDeviceProperties t_Properties;
	vkGetPhysicalDeviceProperties(s_VKB.physicalDevice, &t_Properties);
	return Pointer::AlignPad(a_BuffSize, t_Properties.limits.minUniformBufferOffsetAlignment);
}

void DescriptorAllocator::CreateDescriptorPool()
{
	VkDescriptorPoolCreateInfo t_CreateInfo{};
	t_CreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	t_CreateInfo.pPoolSizes = s_DescriptorPoolSizes;
	t_CreateInfo.poolSizeCount = _countof(s_DescriptorPoolSizes);
	t_CreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	
	t_CreateInfo.maxSets = 1024;

	VKASSERT(vkCreateDescriptorPool(s_VKB.device,
		&t_CreateInfo, nullptr, &descriptorPool),
		"Vulkan: Failed to create descriptorPool.");
}

void DescriptorAllocator::Destroy()
{
	vkDestroyDescriptorPool(s_VKB.device,
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

static VkDebugUtilsMessengerCreateInfoEXT CreateDebugCallbackCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT t_CreateInfo{};
	t_CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	t_CreateInfo.messageSeverity =
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	t_CreateInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	t_CreateInfo.pfnUserCallback = debugCallback;
	t_CreateInfo.pUserData = nullptr;

	return t_CreateInfo;
}

static uint32_t FindMemoryType(uint32_t a_TypeFilter, VkMemoryPropertyFlags a_Properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(s_VKB.physicalDevice, &memProperties);

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
	VkDebugUtilsMessengerCreateInfoEXT t_DebugCreateInfo = CreateDebugCallbackCreateInfo();

	VkDebugUtilsMessengerEXT t_ReturnDebug;

	VKASSERT(t_CreateDebugFunc(a_Instance, &t_DebugCreateInfo, nullptr, &t_ReturnDebug), "Vulkan: Failed to create debug messenger.");
	return t_ReturnDebug;
}

static SwapchainSupportDetails QuerySwapChainSupport(const VkSurfaceKHR a_Surface, const VkPhysicalDevice a_PhysicalDevice)
{
	SwapchainSupportDetails t_SwapDetails{};

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.formatCount, nullptr);
	t_SwapDetails.formats = BBnewArr(s_VulkanTempAllocator, t_SwapDetails.formatCount, VkSurfaceFormatKHR);
	vkGetPhysicalDeviceSurfaceFormatsKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.formatCount, t_SwapDetails.formats);

	vkGetPhysicalDeviceSurfacePresentModesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.presentModeCount, nullptr);
	t_SwapDetails.presentModes = BBnewArr(s_VulkanTempAllocator, t_SwapDetails.presentModeCount, VkPresentModeKHR);
	vkGetPhysicalDeviceSurfacePresentModesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.presentModeCount, t_SwapDetails.presentModes);

	return t_SwapDetails;
}

static bool CheckExtensionSupport(BB::Slice<const char*> a_Extensions)
{
	// check extensions if they are available.
	uint32_t t_ExtensionCount;
	vkEnumerateInstanceExtensionProperties(nullptr, &t_ExtensionCount, nullptr);
	VkExtensionProperties* t_Extensions = BBnewArr(s_VulkanTempAllocator, t_ExtensionCount, VkExtensionProperties);
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

static bool CheckValidationLayerSupport(const BB::Slice<const char*> a_Layers)
{
	// check layers if they are available.
	uint32_t t_LayerCount;
	vkEnumerateInstanceLayerProperties(&t_LayerCount, nullptr);
	VkLayerProperties* t_Layers = BBnewArr(s_VulkanTempAllocator, t_LayerCount, VkLayerProperties);
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
static bool QueueFindGraphicsBit(VkPhysicalDevice a_PhysicalDevice)
{
	uint32_t t_QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, nullptr);

	VkQueueFamilyProperties* t_QueueFamilies = BBnewArr(s_VulkanTempAllocator, t_QueueFamilyCount, VkQueueFamilyProperties);
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

static VkPhysicalDevice FindPhysicalDevice(const VkInstance a_Instance, const VkSurfaceKHR a_Surface)
{
	uint32_t t_DeviceCount = 0;
	vkEnumeratePhysicalDevices(a_Instance, &t_DeviceCount, nullptr);
	BB_ASSERT(t_DeviceCount != 0, "Failed to find any GPU's with vulkan support.");
	VkPhysicalDevice* t_PhysicalDevices = BBnewArr(s_VulkanTempAllocator, t_DeviceCount, VkPhysicalDevice);
	vkEnumeratePhysicalDevices(a_Instance, &t_DeviceCount, t_PhysicalDevices);

	for (uint32_t i = 0; i < t_DeviceCount; i++)
	{

		VkPhysicalDeviceProperties2 t_DeviceProperties{};
		t_DeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		t_DeviceProperties.pNext = nullptr;
		vkGetPhysicalDeviceProperties2(t_PhysicalDevices[i], &t_DeviceProperties);
		
		VkPhysicalDeviceDescriptorIndexingFeatures t_IndexingFeatures{};
		t_IndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
		VkPhysicalDeviceTimelineSemaphoreFeatures t_SyncFeatures{};
		t_SyncFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
		t_SyncFeatures.pNext = &t_IndexingFeatures;
		VkPhysicalDeviceFeatures2 t_DeviceFeatures{};
		t_DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		t_DeviceFeatures.pNext = &t_SyncFeatures;
		vkGetPhysicalDeviceFeatures2(t_PhysicalDevices[i], &t_DeviceFeatures);

		SwapchainSupportDetails t_SwapChainDetails = QuerySwapChainSupport(a_Surface, t_PhysicalDevices[i]);

		if (t_DeviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			t_SyncFeatures.timelineSemaphore == VK_TRUE &&
			t_DeviceFeatures.features.geometryShader &&
			t_DeviceFeatures.features.samplerAnisotropy &&
			QueueFindGraphicsBit(t_PhysicalDevices[i]) &&
			t_SwapChainDetails.formatCount != 0 &&
			t_SwapChainDetails.presentModeCount != 0 &&
			t_IndexingFeatures.descriptorBindingPartiallyBound == VK_TRUE &&
			t_IndexingFeatures.runtimeDescriptorArray == VK_TRUE &&
			t_IndexingFeatures.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE &&
			t_IndexingFeatures.descriptorBindingVariableDescriptorCount == VK_TRUE)
		{
			return t_PhysicalDevices[i];
		}
	}

	BB_ASSERT(false, "Failed to find a suitable GPU that is discrete and has a geometry shader.");
	return VK_NULL_HANDLE;
}

static VkDevice CreateLogicalDevice(const BB::Slice<const char*>& a_DeviceExtensions)
{
	VkDevice t_ReturnDevice;

	uint32_t t_QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(s_VKB.physicalDevice, &t_QueueFamilyCount, nullptr);
	VkQueueFamilyProperties* t_QueueFamilies = BBnewArr(s_VulkanTempAllocator, t_QueueFamilyCount, VkQueueFamilyProperties);
	vkGetPhysicalDeviceQueueFamilyProperties(s_VKB.physicalDevice, &t_QueueFamilyCount, t_QueueFamilies);

	VkDeviceQueueCreateInfo* t_QueueCreateInfos = BBnewArr(s_VulkanTempAllocator, 3, VkDeviceQueueCreateInfo);
	uint32_t t_DifferentQueues = 0;
	float t_StandardQueuePrios[16] = { 1.0f }; // just put it all to 1 for multiple queues;

	{
		VulkanQueueDeviceInfo t_GraphicQueue = FindQueueIndex(t_QueueFamilies,
			t_QueueFamilyCount,
			VK_QUEUE_GRAPHICS_BIT);

		s_VKB.queueIndices.graphics = t_GraphicQueue.index;
		s_VKB.queueIndices.present = t_GraphicQueue.index;
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
		if (t_TransferQueue.index != s_VKB.queueIndices.graphics)
		{
			s_VKB.queueIndices.transfer = t_TransferQueue.index;
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
			s_VKB.queueIndices.transfer = s_VKB.queueIndices.graphics;
		}
	}

	{
		VulkanQueueDeviceInfo t_ComputeQueue = FindQueueIndex(t_QueueFamilies,
			t_QueueFamilyCount,
			VK_QUEUE_COMPUTE_BIT);
		//Check if the queueindex is the same as graphics.
		if ((t_ComputeQueue.index != s_VKB.queueIndices.graphics) &&
			(t_ComputeQueue.index != s_VKB.queueIndices.compute))
		{
			s_VKB.queueIndices.compute = t_ComputeQueue.index;
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
			s_VKB.queueIndices.compute = s_VKB.queueIndices.graphics;
		}
	}

	VkPhysicalDeviceFeatures t_DeviceFeatures{};
	t_DeviceFeatures.samplerAnisotropy = VK_TRUE;
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

	VkPhysicalDeviceDescriptorIndexingFeatures t_IndexingFeatures{};
	t_IndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	t_IndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
	t_IndexingFeatures.runtimeDescriptorArray = VK_TRUE;
	t_IndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	t_IndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
	t_IndexingFeatures.pNext = &t_DynamicRendering;

	VkDeviceCreateInfo t_CreateInfo{};
	t_CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	t_CreateInfo.pQueueCreateInfos = t_QueueCreateInfos;
	t_CreateInfo.queueCreateInfoCount = t_DifferentQueues;
	t_CreateInfo.pEnabledFeatures = &t_DeviceFeatures;

	t_CreateInfo.ppEnabledExtensionNames = a_DeviceExtensions.data();
	t_CreateInfo.enabledExtensionCount = static_cast<uint32_t>(a_DeviceExtensions.size());
	t_CreateInfo.pNext = &t_IndexingFeatures;

	VKASSERT(vkCreateDevice(s_VKB.physicalDevice, 
		&t_CreateInfo, 
		nullptr, 
		&t_ReturnDevice),
		"Failed to create logical device Vulkan.");

	VkPhysicalDeviceProperties2 t_DeviceProperties{};
	t_DeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	vkGetPhysicalDeviceProperties2(s_VKB.physicalDevice, &t_DeviceProperties);

	s_VKB.deviceInfo.maxAnisotropy = t_DeviceProperties.properties.limits.maxSamplerAnisotropy;

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
		if (a_Modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}

	BB_WARNING(false, "Vulkan: Found no optimized Presentmode, choosing VK_PRESENT_MODE_FIFO_KHR.", WarningType::LOW);
	return VK_PRESENT_MODE_FIFO_KHR;
}

static void CreateSwapchain(VkSurfaceKHR a_Surface, VkPhysicalDevice a_PhysicalDevice, VkDevice a_Device, uint32_t t_SurfaceWidth, uint32_t t_SurfaceHeight, bool a_Recreate = false)
{
#ifdef _DEBUG
	if (!a_Recreate)
	{
		BB_ASSERT(s_VKB.swapChain.swapChain == VK_NULL_HANDLE || s_VKB.swapChain.frames == nullptr,
			"Vulkan: Trying to create a swapchain while one already exists! Memory could leak!");
	}
	else
	{
		BB_ASSERT(s_VKB.swapChain.swapChain != VK_NULL_HANDLE || s_VKB.swapChain.frames != nullptr,
			"Vulkan: Trying to recreate swapchain while none exist!")
	}
#endif //_DEBUG

	if (a_Recreate)
	{
		for (FrameIndex i = 0; i < s_VKB.frameCount; i++)
		{
			vkDestroyImageView(s_VKB.device,
				s_VKB.swapChain.frames[i].imageView, nullptr);
			vkDestroySemaphore(s_VKB.device,
				s_VKB.swapChain.frames[i].frameTimelineSemaphore, nullptr);
			vkDestroySemaphore(s_VKB.device,
				s_VKB.swapChain.frames[i].imageAvailableSem, nullptr);
			vkDestroySemaphore(s_VKB.device,
				s_VKB.swapChain.frames[i].imageRenderFinishedSem, nullptr);
		}

		//When we recreate we will destroy these elements. Since we could build with a different amount of framenbuffers.
		BBfreeArr(s_VulkanAllocator, s_VKB.swapChain.frames);
	}

	SwapchainSupportDetails t_SwapchainDetails = QuerySwapChainSupport(a_Surface, a_PhysicalDevice);

	VkSurfaceFormatKHR t_ChosenFormat = ChooseSurfaceFormat(t_SwapchainDetails.formats, t_SwapchainDetails.formatCount);
	VkPresentModeKHR t_ChosenPresentMode = ChoosePresentMode(t_SwapchainDetails.presentModes, t_SwapchainDetails.presentModeCount);
	VkExtent2D t_ChosenExtent{};
	t_ChosenExtent.width = Clamp(t_SurfaceWidth,
		t_SwapchainDetails.capabilities.minImageExtent.width,
		t_SwapchainDetails.capabilities.maxImageExtent.width);
	t_ChosenExtent.height = Clamp(t_SurfaceHeight,
		t_SwapchainDetails.capabilities.minImageExtent.height,
		t_SwapchainDetails.capabilities.maxImageExtent.height);
	s_VKB.swapChain.extent = t_ChosenExtent;

	uint32_t t_GraphicFamily, t_PresentFamily;
	t_GraphicFamily = s_VKB.queueIndices.graphics;
	t_PresentFamily = s_VKB.queueIndices.present;
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

	s_VKB.swapChain.imageFormat = t_ChosenFormat.format;
	//Don't recreate so we have no old swapchain.
	t_SwapCreateInfo.oldSwapchain = s_VKB.swapChain.swapChain;

	//Now create the swapchain and set the framecount.
	s_VKB.frameCount = t_SwapchainDetails.capabilities.minImageCount + 1;
	t_SwapCreateInfo.minImageCount = t_SwapchainDetails.capabilities.minImageCount + 1;
	if (t_SwapchainDetails.capabilities.maxImageCount > 0 && s_VKB.frameCount >
		t_SwapchainDetails.capabilities.maxImageCount)
	{
		s_VKB.frameCount = t_SwapchainDetails.capabilities.maxImageCount;
		t_SwapCreateInfo.minImageCount = t_SwapchainDetails.capabilities.maxImageCount;
	}

	VKASSERT(vkCreateSwapchainKHR(a_Device, &t_SwapCreateInfo, nullptr, &s_VKB.swapChain.swapChain), "Vulkan: Failed to create swapchain.");

	vkGetSwapchainImagesKHR(a_Device, s_VKB.swapChain.swapChain, &s_VKB.frameCount, nullptr);
	VkImage* t_SwapchainImages = BBnewArr(s_VulkanAllocator, s_VKB.frameCount, VkImage);
	vkGetSwapchainImagesKHR(a_Device, s_VKB.swapChain.swapChain, &s_VKB.frameCount, t_SwapchainImages);

	//Also create the present semaphores, these are unique semaphores that handle the window integration API as they cannot use timeline semaphores.
	s_VKB.swapChain.frames = BBnewArr(
		s_VulkanAllocator,
		s_VKB.frameCount,
		SwapchainFrame);

	VkImageViewCreateInfo t_ImageViewCreateInfo{};
	t_ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	t_ImageViewCreateInfo.format = s_VKB.swapChain.imageFormat;
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

	VkSemaphoreCreateInfo t_SemInfo{};
	t_SemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	//Used for the last semaphore created of a single frame struct.
	VkSemaphoreTypeCreateInfo t_TimelineSemInfo{};
	t_TimelineSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	t_TimelineSemInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	t_TimelineSemInfo.initialValue = 0;
	for (size_t i = 0; i < s_VKB.frameCount; i++)
	{
		s_VKB.swapChain.frames[i].image = t_SwapchainImages[i];

		t_ImageViewCreateInfo.image = t_SwapchainImages[i];
		VKASSERT(vkCreateImageView(a_Device,
			&t_ImageViewCreateInfo,
			nullptr,
			&s_VKB.swapChain.frames[i].imageView),
			"Vulkan: Failed to create swapchain image views.");

		vkCreateSemaphore(s_VKB.device,
			&t_SemInfo,
			nullptr,
			&s_VKB.swapChain.frames[i].imageAvailableSem);
		vkCreateSemaphore(s_VKB.device,
			&t_SemInfo,
			nullptr,
			&s_VKB.swapChain.frames[i].imageRenderFinishedSem);

		t_SemInfo.pNext = &t_TimelineSemInfo;
		vkCreateSemaphore(s_VKB.device,
			&t_SemInfo,
			nullptr,
			&s_VKB.swapChain.frames[i].frameTimelineSemaphore);
		t_SemInfo.pNext = nullptr;
		s_VKB.swapChain.frames[i].frameWaitValue = 0;
	}
}

BackendInfo BB::VulkanCreateBackend(const RenderBackendCreateInfo& a_CreateInfo)
{
	//Initialize data structure
	s_VKB.CreatePools();


	VKConv::ExtensionResult t_InstanceExtensions = VKConv::TranslateExtensions(
		s_VulkanTempAllocator,
		a_CreateInfo.extensions);
	VKConv::ExtensionResult t_DeviceExtensions = VKConv::TranslateExtensions(
		s_VulkanTempAllocator,
		a_CreateInfo.deviceExtensions);

	//Check if the extensions and layers work.
	BB_ASSERT(CheckExtensionSupport(BB::Slice(t_InstanceExtensions.extensions, t_InstanceExtensions.count)),
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
			BB_WARNING(CheckValidationLayerSupport(Slice(&validationLayer, 1)), "Vulkan: Validation layer(s) not available.", WarningType::MEDIUM);
			t_DebugCreateInfo = CreateDebugCallbackCreateInfo();
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
		t_SurfaceCreateInfo.hwnd = reinterpret_cast<HWND>(a_CreateInfo.windowHandle.ptrHandle);
		t_SurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
		VKASSERT(vkCreateWin32SurfaceKHR(s_VKB.instance,
			&t_SurfaceCreateInfo, nullptr,
			&s_VKB.surface),
			"Failed to create Win32 vulkan surface.");
	}

	//Get the physical Device
	s_VKB.physicalDevice = FindPhysicalDevice(s_VKB.instance, s_VKB.surface);
	//Get the logical device and the graphics queue.
	s_VKB.device = CreateLogicalDevice(BB::Slice(t_DeviceExtensions.extensions, t_DeviceExtensions.count));


	CreateSwapchain(s_VKB.surface,
		s_VKB.physicalDevice,
		s_VKB.device,
		a_CreateInfo.windowWidth,
		a_CreateInfo.windowHeight);

	//Get the present queue.
	vkGetDeviceQueue(s_VKB.device,
		s_VKB.queueIndices.present,
		0,
		&s_VKB.presentQueue);

	//Setup the Vulkan Memory Allocator
	VmaVulkanFunctions t_VkFunctions{};
	t_VkFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	t_VkFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo t_AllocatorCreateInfo = {};
	t_AllocatorCreateInfo.vulkanApiVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);
	t_AllocatorCreateInfo.physicalDevice = s_VKB.physicalDevice;
	t_AllocatorCreateInfo.device = s_VKB.device;
	t_AllocatorCreateInfo.instance = s_VKB.instance;
	t_AllocatorCreateInfo.pVulkanFunctions = &t_VkFunctions;

	vmaCreateAllocator(&t_AllocatorCreateInfo, &s_VKB.vma);

	//Create descriptor allocator.
	s_VKB.descriptorAllocator.CreateDescriptorPool();

	//Returns some info to the global backend that is important.
	BackendInfo t_BackendInfo;
	t_BackendInfo.currentFrame = s_VKB.currentFrame;
	t_BackendInfo.framebufferCount = s_VKB.frameCount;

	return t_BackendInfo;
}

RDescriptorHandle BB::VulkanCreateDescriptor(const RenderDescriptorCreateInfo& a_Info)
{
	bool bindlessSet = false;
	VulkanBindingSet* t_BindingSet = s_VKB.bindingSetPool.Get();
	*t_BindingSet = {}; //set to 0

	t_BindingSet->bindingSet = a_Info.bindingSet;

	uint32_t t_DescriptorCount = 0;
	
	{
		VkDescriptorSetLayoutBinding* t_LayoutBinds = BBnewArr(
			s_VulkanTempAllocator,
			a_Info.bindings.size(),
			VkDescriptorSetLayoutBinding);

		VkDescriptorBindingFlags* t_BindlessFlags = BBnewArr(
			s_VulkanTempAllocator,
			a_Info.bindings.size(),
			VkDescriptorBindingFlags);

		for (size_t i = 0; i < a_Info.bindings.size(); i++)
		{
			const DescriptorBinding& t_Binding = a_Info.bindings[i];
			VkSampler* t_Samplers = nullptr;
			if (t_Binding.staticSamplers.size())
			{
				t_Samplers = reinterpret_cast<VkSampler*>(alloca(sizeof(VkSampler) * t_Binding.staticSamplers.size()));
				for (size_t i = 0; i < t_Binding.staticSamplers.size(); i++)
				{
					//Hacky for now... we just create the sampler like this. (WE WILL DO THIS LATER LMAO)
					//t_Samplers[i] = *reinterpret_cast<VkSampler*>(VulkanCreateSampler(t_Binding.staticSamplers[i]).ptrHandle);
					BB_ASSERT(false, "Vulkan: static samplers not yet implemented!");
				}
				t_LayoutBinds[i].pImmutableSamplers = t_Samplers;
			}

			t_LayoutBinds[i].binding = t_Binding.binding;
			t_LayoutBinds[i].descriptorCount = t_Binding.descriptorCount;
			t_LayoutBinds[i].descriptorType = VKConv::DescriptorBufferType(t_Binding.type);
			t_LayoutBinds[i].pImmutableSamplers = t_Samplers; //Is null or we provide these elements.
			t_LayoutBinds[i].stageFlags = VKConv::ShaderStageBits(t_Binding.stage);

			switch (t_Binding.flags)
			{
			case BB::RENDER_DESCRIPTOR_FLAG::NONE:
				t_BindlessFlags[i] = 0;
				break;
			case BB::RENDER_DESCRIPTOR_FLAG::BINDLESS:
				t_DescriptorCount += t_Binding.descriptorCount;
				t_BindlessFlags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
					VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
					VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;

				bindlessSet = true;
				break;
			default:
				BB_ASSERT(false, "Vulkan: RENDER_DESCRIPTOR_FLAG not supported!");
				break;
			}
		}

		VkDescriptorSetLayoutCreateInfo t_LayoutInfo{};
		t_LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		t_LayoutInfo.pBindings = t_LayoutBinds;
		t_LayoutInfo.bindingCount = static_cast<uint32_t>(a_Info.bindings.size());
		
		if (bindlessSet) //if bindless add another struct and return here.
		{
			t_LayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

			VkDescriptorSetLayoutBindingFlagsCreateInfo t_LayoutExtInfo{};
			t_LayoutExtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
			t_LayoutExtInfo.bindingCount = static_cast<uint32_t>(a_Info.bindings.size());
			t_LayoutExtInfo.pBindingFlags = t_BindlessFlags;

			t_LayoutInfo.pNext = &t_LayoutExtInfo;

			//Do some algorithm to see if I already made a descriptorlayout like this one.
			VKASSERT(vkCreateDescriptorSetLayout(s_VKB.device,
				&t_LayoutInfo, nullptr, &t_BindingSet->setLayout),
				"Vulkan: Failed to create a descriptorsetlayout.");
		}
		else
		{
			//Do some algorithm to see if I already made a descriptorlayout like this one.
			VKASSERT(vkCreateDescriptorSetLayout(s_VKB.device,
				&t_LayoutInfo, nullptr, &t_BindingSet->setLayout),
				"Vulkan: Failed to create a descriptorsetlayout.");
		}
	}

	{
		//Now we create the descriptor set.
		VkDescriptorSetAllocateInfo t_AllocInfo = {};
		t_AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		t_AllocInfo.pSetLayouts = &t_BindingSet->setLayout;
		t_AllocInfo.descriptorSetCount = 1;
		//Lmao creat pool
		t_AllocInfo.descriptorPool = s_VKB.descriptorAllocator.GetPool();

		if (bindlessSet) //if bindless add another struct and return here.
		{
			VkDescriptorSetVariableDescriptorCountAllocateInfo t_CountInfo{};
			t_CountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
			t_CountInfo.descriptorSetCount = 1;
			t_CountInfo.pDescriptorCounts = &t_DescriptorCount;

			t_AllocInfo.pNext = &t_CountInfo;

			VKASSERT(vkAllocateDescriptorSets(s_VKB.device, &t_AllocInfo, &t_BindingSet->set),
				"Vulkan: Allocating bindless descriptor sets failed.");
			
			return RDescriptorHandle(t_BindingSet);
		}

		VKASSERT(vkAllocateDescriptorSets(s_VKB.device, &t_AllocInfo, &t_BindingSet->set),
			"Vulkan: Allocating descriptor sets failed.");
	}

	return RDescriptorHandle(t_BindingSet);
}

CommandQueueHandle BB::VulkanCreateCommandQueue(const RenderCommandQueueCreateInfo& a_Info)
{
	VulkanCommandQueue* t_Queue = s_VKB.cmdQueues.Get();
	uint32_t t_QueueIndex;

	switch (a_Info.queue)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		t_QueueIndex = s_VKB.queueIndices.graphics;
		break;
	case RENDER_QUEUE_TYPE::TRANSFER_COPY:
		t_QueueIndex = s_VKB.queueIndices.transfer;
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
		t_QueueIndex = s_VKB.queueIndices.compute;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Trying to get a device queue that you didn't setup yet.");
		break;
	}

	vkGetDeviceQueue(s_VKB.device,
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

	vkCreateSemaphore(s_VKB.device,
		&t_SemCreateInfo,
		nullptr,
		&t_Queue->timelineSemaphore);


	t_Queue->lastCompleteValue = 1;
	t_Queue->nextSemValue = 2;

	VkSemaphoreSignalInfo t_SigInfo{};
	t_SigInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
	t_SigInfo.semaphore = t_Queue->timelineSemaphore;
	t_SigInfo.value = t_Queue->lastCompleteValue;
	vkSignalSemaphore(s_VKB.device,
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
		t_CreateInfo.queueFamilyIndex = s_VKB.queueIndices.graphics;
		break;
	case RENDER_QUEUE_TYPE::TRANSFER_COPY:
		t_CreateInfo.queueFamilyIndex = s_VKB.queueIndices.transfer;
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
		t_CreateInfo.queueFamilyIndex = s_VKB.queueIndices.compute;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Tried to make a command allocator with a queue type that does not exist.");
		break;
	}

	VKASSERT(vkCreateCommandPool(s_VKB.device,
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

	VKASSERT(vkAllocateCommandBuffers(s_VKB.device,
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

RBufferHandle BB::VulkanCreateBuffer(const RenderBufferCreateInfo& a_Info)
{
	VulkanBuffer* t_Buffer = s_VKB.bufferPool.Get();

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

constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D24_UNORM_S8_UINT;

RImageHandle BB::VulkanCreateImage(const RenderImageCreateInfo& a_CreateInfo)
{
	VulkanImage* t_Image = s_VKB.imagePool.Get();

	VkImageCreateInfo t_ImageCreateInfo{};
	t_ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

	VkImageViewCreateInfo t_ViewInfo{};
	t_ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	t_ImageCreateInfo.extent.width = a_CreateInfo.width;
	t_ImageCreateInfo.extent.height = a_CreateInfo.height;
	t_ImageCreateInfo.extent.depth = a_CreateInfo.depth;
	switch (a_CreateInfo.format)
	{
	case RENDER_IMAGE_FORMAT::RGBA8_SRGB:
		t_ImageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		t_ImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		t_ViewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		t_ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	case RENDER_IMAGE_FORMAT::RGBA8_UNORM:
		t_ImageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		t_ImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		t_ViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		t_ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	case RENDER_IMAGE_FORMAT::DEPTH_STENCIL:
		t_ImageCreateInfo.format = DEPTH_FORMAT;
		t_ImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		t_ViewInfo.format = DEPTH_FORMAT;
		t_ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Image format type not supported!");
		break;
	}

	switch (a_CreateInfo.tiling)
	{
	case RENDER_IMAGE_TILING::OPTIMAL:
		t_ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		break;
	case RENDER_IMAGE_TILING::LINEAR:
		t_ImageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Image tiling type not supported!");
		break;
	}

	switch (a_CreateInfo.type)
	{
	case RENDER_IMAGE_TYPE::TYPE_2D:
		t_ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;

		if (a_CreateInfo.arrayLayers > 1)
			t_ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		else
			t_ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Image type is not supported!");
		break;
	}

	t_ImageCreateInfo.mipLevels = a_CreateInfo.mipLevels;
	t_ImageCreateInfo.arrayLayers = a_CreateInfo.arrayLayers;
	//Will be defined in the first layout transition.
	t_ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	t_ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	t_ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	t_ImageCreateInfo.flags = 0;

	VmaAllocationCreateInfo t_AllocInfo{};
	t_AllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VKASSERT(vmaCreateImage(s_VKB.vma, &t_ImageCreateInfo, &t_AllocInfo, &t_Image->image, &t_Image->allocation, nullptr),
		"Vulkan: Failed to create image");

	t_ViewInfo.image = t_Image->image;
	//ASPECT MASK DEFINED IN IMAGE_FORMAT!
	t_ViewInfo.subresourceRange.baseMipLevel = 0;
	t_ViewInfo.subresourceRange.levelCount = a_CreateInfo.mipLevels;
	t_ViewInfo.subresourceRange.baseArrayLayer = 0;
	t_ViewInfo.subresourceRange.layerCount = a_CreateInfo.arrayLayers;

	VKASSERT(vkCreateImageView(s_VKB.device, &t_ViewInfo, nullptr, &t_Image->view),
		"Vulkan: Failed to create image view.");

	t_Image->width = a_CreateInfo.width;
	t_Image->height = a_CreateInfo.height;
	t_Image->depth = a_CreateInfo.depth;
	t_Image->arrays = a_CreateInfo.arrayLayers;
	t_Image->mips = a_CreateInfo.mipLevels;

	return RImageHandle(t_Image);
}

RSamplerHandle BB::VulkanCreateSampler(const SamplerCreateInfo& a_Info)
{
	VkSampler t_Sampler{};
	VkSamplerCreateInfo t_SamplerInfo{};
	t_SamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	t_SamplerInfo.addressModeU = VKConv::AddressMode(a_Info.addressModeU);
	t_SamplerInfo.addressModeV = VKConv::AddressMode(a_Info.addressModeV);
	t_SamplerInfo.addressModeW = VKConv::AddressMode(a_Info.addressModeW);
	switch (a_Info.filter)
	{
	case SAMPLER_FILTER::NEAREST:
		t_SamplerInfo.magFilter = VK_FILTER_NEAREST;
		t_SamplerInfo.minFilter = VK_FILTER_NEAREST;
		break;
	case SAMPLER_FILTER::LINEAR:
		t_SamplerInfo.magFilter = VK_FILTER_LINEAR;
		t_SamplerInfo.minFilter = VK_FILTER_LINEAR;
		break;
	default:
		BB_ASSERT(false, "Vulkan, does not support this type of sampler filter!");
		break;
	}
	t_SamplerInfo.minLod = a_Info.minLod;
	t_SamplerInfo.maxLod = a_Info.maxLod;
	t_SamplerInfo.mipLodBias = 0;
	if (a_Info.maxAnistoropy > 0)
	{
		t_SamplerInfo.anisotropyEnable = VK_TRUE;
		t_SamplerInfo.maxAnisotropy = a_Info.maxAnistoropy;
	}

	VKASSERT(vkCreateSampler(s_VKB.device, &t_SamplerInfo, nullptr, &t_Sampler),
		"Vulkan: Failed to create image sampler!");

	return RSamplerHandle(t_Sampler);
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
	vkCreateSemaphore(s_VKB.device,
		&t_SemCreateInfo,
		nullptr,
		&t_TimelineSem);

	return RFenceHandle(t_TimelineSem);
}

void BB::VulkanUpdateDescriptorBuffer(const UpdateDescriptorBufferInfo& a_Info)
{
	VkDescriptorBufferInfo t_BufferInfo{};
	t_BufferInfo.buffer = reinterpret_cast<VulkanBuffer*>(a_Info.buffer.ptrHandle)->buffer;
	t_BufferInfo.offset = a_Info.bufferOffset;
	t_BufferInfo.range = a_Info.bufferSize;

	VkWriteDescriptorSet t_Write{};
	t_Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	t_Write.dstBinding = a_Info.binding;
	t_Write.dstArrayElement = a_Info.descriptorIndex;
	t_Write.dstSet = reinterpret_cast<VulkanBindingSet*>(a_Info.set.ptrHandle)->set;
	t_Write.descriptorCount = 1;
	t_Write.descriptorType = VKConv::DescriptorBufferType(a_Info.type);
	t_Write.pBufferInfo = &t_BufferInfo;

	vkUpdateDescriptorSets(s_VKB.device,
		1,
		&t_Write,
		0,
		nullptr);
}

void BB::VulkanUpdateDescriptorImage(const UpdateDescriptorImageInfo& a_Info)
{
	VkDescriptorImageInfo t_ImageInfo{};

	VkWriteDescriptorSet t_Write{};
	t_Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	t_Write.dstBinding = a_Info.binding;
	t_Write.dstArrayElement = a_Info.descriptorIndex;
	t_Write.dstSet = reinterpret_cast<VulkanBindingSet*>(a_Info.set.ptrHandle)->set;
	t_Write.descriptorCount = 1;
	switch (a_Info.type)
	{
	case RENDER_DESCRIPTOR_TYPE::IMAGE:
		t_ImageInfo.imageLayout = VKConv::ImageLayout(a_Info.imageLayout);
		t_ImageInfo.imageView = reinterpret_cast<VulkanImage*>(a_Info.image.ptrHandle)->view;
		t_ImageInfo.sampler = VK_NULL_HANDLE;
		t_Write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		break;
	case RENDER_DESCRIPTOR_TYPE::SAMPLER:
		t_ImageInfo.imageView = VK_NULL_HANDLE;
		t_ImageInfo.sampler = reinterpret_cast<VkSampler>(a_Info.sampler.ptrHandle);
		t_Write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Trying to update an invalid descriptor type for update image!");
		break;
	}
	t_Write.pImageInfo = &t_ImageInfo;

	//maybe make this a scheduler
	vkUpdateDescriptorSets(s_VKB.device,
		1,
		&t_Write,
		0,
		nullptr);
}

void BB::VulkanUpdateDescriptorBuffers(const Slice<UpdateDescriptorBufferInfo> a_Info)
{
	VkDescriptorBufferInfo* t_BufferInfos = BBnewArr(
		s_VulkanTempAllocator,
		a_Info.size(),
		VkDescriptorBufferInfo);
	VkWriteDescriptorSet* t_Writes = BBnewArr(s_VulkanTempAllocator,
		a_Info.size(),
		VkWriteDescriptorSet);

	for (size_t i = 0; i < a_Info.size(); i++)
	{
		t_BufferInfos[i].buffer = reinterpret_cast<VulkanBuffer*>(a_Info[i].buffer.ptrHandle)->buffer;
		t_BufferInfos[i].offset = a_Info[i].bufferOffset;
		t_BufferInfos[i].range = a_Info[i].bufferSize;

		t_Writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		t_Writes[i].dstBinding = a_Info[i].binding;
		t_Writes[i].dstArrayElement = a_Info[i].descriptorIndex;
		t_Writes[i].dstSet = reinterpret_cast<VulkanBindingSet*>(a_Info[i].set.ptrHandle)->set;
		t_Writes[i].descriptorCount = 1;
		t_Writes[i].descriptorType = VKConv::DescriptorBufferType(a_Info[i].type);
		t_Writes[i].pBufferInfo = &t_BufferInfos[i];
	}

	vkUpdateDescriptorSets(s_VKB.device,
		static_cast<uint32_t>(a_Info.size()),
		t_Writes,
		0,
		nullptr);
}

void BB::VulkanUpdateDescriptorImages(const Slice <UpdateDescriptorImageInfo> a_Info)
{
	VkDescriptorImageInfo* t_ImageInfos = BBnewArr(
		s_VulkanTempAllocator,
		a_Info.size(),
		VkDescriptorImageInfo);
	VkWriteDescriptorSet* t_Writes = BBnewArr(s_VulkanTempAllocator,
		a_Info.size(),
		VkWriteDescriptorSet);

	for (size_t i = 0; i < a_Info.size(); i++)
	{
		t_Writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		t_Writes[i].dstBinding = a_Info[i].binding;
		t_Writes[i].dstArrayElement = a_Info[i].descriptorIndex;
		t_Writes[i].dstSet = reinterpret_cast<VulkanBindingSet*>(a_Info[i].set.ptrHandle)->set;
		t_Writes[i].descriptorCount = 1;
		switch (a_Info[i].type)
		{
		case RENDER_DESCRIPTOR_TYPE::IMAGE:
			t_ImageInfos[i].imageLayout = VKConv::ImageLayout(a_Info[i].imageLayout);
			t_ImageInfos[i].imageView = reinterpret_cast<VulkanImage*>(a_Info[i].image.ptrHandle)->view;
			t_ImageInfos[i].sampler = VK_NULL_HANDLE;
			t_Writes[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			break;
		case RENDER_DESCRIPTOR_TYPE::SAMPLER:
			t_ImageInfos[i].imageView = VK_NULL_HANDLE;
			t_ImageInfos[i].sampler = reinterpret_cast<VkSampler>(a_Info[i].sampler.ptrHandle);
			t_Writes[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			break;
		default:
			BB_ASSERT(false, "Vulkan: Trying to update an invalid descriptor type for update image!");
			break;
		}
		t_Writes[i].pImageInfo = &t_ImageInfos[i];
	}

	vkUpdateDescriptorSets(s_VKB.device,
		static_cast<uint32_t>(a_Info.size()),
		t_Writes,
		0,
		nullptr);
}

ImageReturnInfo BB::VulkanGetImageInfo(const RImageHandle a_Handle)
{
	VulkanImage* t_Image = reinterpret_cast<VulkanImage*>(a_Handle.handle);

	ImageReturnInfo t_ReturnInfo{};
	t_ReturnInfo.allocInfo.imageAllocByteSize = static_cast<uint64_t>(
		t_Image->width *
		t_Image->height *
		4 * //4 is the amount of channels it has.
		t_Image->depth *
		t_Image->arrays *
		t_Image->mips);
	t_ReturnInfo.allocInfo.footRowPitch = t_Image->width * sizeof(uint32_t);
	t_ReturnInfo.allocInfo.footHeight = t_Image->height;

	t_ReturnInfo.width = t_Image->width;
	t_ReturnInfo.height = t_Image->height;
	t_ReturnInfo.depth = t_Image->depth;
	t_ReturnInfo.arrayLayers = t_Image->arrays;
	t_ReturnInfo.mips = t_Image->mips;

	return t_ReturnInfo;
}

PipelineBuilderHandle BB::VulkanPipelineBuilderInit(const PipelineInitInfo& a_InitInfo)
{
	VKPipelineBuildInfo* t_BuildInfo = BBnew(s_VulkanAllocator, VKPipelineBuildInfo)();

	//We do dynamic rendering to avoid having to handle renderpasses and such.
	t_BuildInfo->dynamicRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	t_BuildInfo->dynamicRenderingInfo.colorAttachmentCount = 1;
	t_BuildInfo->dynamicRenderingInfo.pColorAttachmentFormats = &s_VKB.swapChain.imageFormat;
	t_BuildInfo->dynamicRenderingInfo.pNext = nullptr;
	
	t_BuildInfo->pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	t_BuildInfo->pipeInfo.subpass = 0;
	t_BuildInfo->pipeInfo.renderPass = nullptr; //We not using em anymore! Dynamic rendering enabled.
	t_BuildInfo->pipeInfo.pNext = &t_BuildInfo->dynamicRenderingInfo;

	{ //Depth stencil create;
		VkPipelineDepthStencilStateCreateInfo* t_DepthCreateInfo = BBnew(
			t_BuildInfo->buildAllocator,
			VkPipelineDepthStencilStateCreateInfo);
		t_DepthCreateInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		if (a_InitInfo.enableDepthTest)
		{
			t_DepthCreateInfo->depthTestEnable = VK_TRUE;
			t_DepthCreateInfo->depthWriteEnable = VK_TRUE;
			t_DepthCreateInfo->depthCompareOp = VK_COMPARE_OP_LESS;
			t_DepthCreateInfo->depthBoundsTestEnable = VK_FALSE;
			t_DepthCreateInfo->minDepthBounds = 0.0f;
			t_DepthCreateInfo->maxDepthBounds = 1.0f;
			t_DepthCreateInfo->stencilTestEnable = VK_FALSE;
		}

		t_BuildInfo->pipeInfo.pDepthStencilState = t_DepthCreateInfo;
		t_BuildInfo->dynamicRenderingInfo.depthAttachmentFormat = DEPTH_FORMAT;
		t_BuildInfo->dynamicRenderingInfo.stencilAttachmentFormat = DEPTH_FORMAT;
	}


	{
		VkPipelineColorBlendAttachmentState* t_ColorBlendAttachment = BBnewArr(
			t_BuildInfo->buildAllocator,
			a_InitInfo.renderTargetBlendCount,
			VkPipelineColorBlendAttachmentState);
		for (size_t i = 0; i < a_InitInfo.renderTargetBlendCount; i++)
		{
			t_ColorBlendAttachment->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			t_ColorBlendAttachment->blendEnable = a_InitInfo.renderTargetBlends[0].blendEnable;
			t_ColorBlendAttachment->srcColorBlendFactor = VKConv::BlendFactors(a_InitInfo.renderTargetBlends[0].srcBlend);
			t_ColorBlendAttachment->dstColorBlendFactor = VKConv::BlendFactors(a_InitInfo.renderTargetBlends[0].dstBlend);
			t_ColorBlendAttachment->colorBlendOp = VKConv::BlendOp(a_InitInfo.renderTargetBlends[0].blendOp);
			t_ColorBlendAttachment->srcAlphaBlendFactor = VKConv::BlendFactors(a_InitInfo.renderTargetBlends[0].srcBlendAlpha);
			t_ColorBlendAttachment->dstAlphaBlendFactor = VKConv::BlendFactors(a_InitInfo.renderTargetBlends[0].dstBlendAlpha);
			t_ColorBlendAttachment->alphaBlendOp = VKConv::BlendOp(a_InitInfo.renderTargetBlends[0].blendOpAlpha);
		}

		VkPipelineColorBlendStateCreateInfo* t_ColorBlending = BBnew(
			t_BuildInfo->buildAllocator,
			VkPipelineColorBlendStateCreateInfo);
		t_ColorBlending->sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		t_ColorBlending->logicOpEnable = a_InitInfo.blendLogicOpEnable;
		t_ColorBlending->logicOp = VKConv::LogicOp(a_InitInfo.blendLogicOp);
		t_ColorBlending->attachmentCount = a_InitInfo.renderTargetBlendCount;
		t_ColorBlending->pAttachments = t_ColorBlendAttachment;

		t_BuildInfo->pipeInfo.pColorBlendState = t_ColorBlending;
	}

	{
		VkPipelineRasterizationStateCreateInfo* t_Rasterizer = BBnew(
			t_BuildInfo->buildAllocator,
			VkPipelineRasterizationStateCreateInfo);
		t_Rasterizer->sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		t_Rasterizer->depthClampEnable = VK_FALSE;
		t_Rasterizer->depthBiasEnable = VK_FALSE;
		t_Rasterizer->rasterizerDiscardEnable = VK_FALSE;
		t_Rasterizer->polygonMode = VK_POLYGON_MODE_FILL;
		t_Rasterizer->cullMode = VKConv::CullMode(a_InitInfo.rasterizerState.cullMode);
		if (a_InitInfo.rasterizerState.frontCounterClockwise)
			t_Rasterizer->frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		else
			t_Rasterizer->frontFace = VK_FRONT_FACE_CLOCKWISE;
		t_Rasterizer->lineWidth = 1.0f;
		t_Rasterizer->depthBiasConstantFactor = 0.0f; // Optional
		t_Rasterizer->depthBiasClamp = 0.0f; // Optional
		t_Rasterizer->depthBiasSlopeFactor = 0.0f; // Optional

		t_BuildInfo->pipeInfo.pRasterizationState = t_Rasterizer;
	}
	//If we have constant data we will add a push constant. We will create one that will manage all the push ranges.
	if (a_InitInfo.constantData.dwordSize > 0)
	{
		VkPushConstantRange* t_ConstantRanges = BBnew(
			t_BuildInfo->buildAllocator,
			VkPushConstantRange);
		t_ConstantRanges->offset = 0;
		t_ConstantRanges->size = a_InitInfo.constantData.dwordSize * sizeof(unsigned int);
		t_ConstantRanges->stageFlags = VKConv::ShaderVisibility(a_InitInfo.constantData.shaderStage);
		t_BuildInfo->pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		t_BuildInfo->pipeLayoutInfo.pushConstantRangeCount = 1;
		t_BuildInfo->pipeLayoutInfo.pPushConstantRanges = t_ConstantRanges;
	}
	else
	{
		t_BuildInfo->pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		t_BuildInfo->pipeLayoutInfo.pushConstantRangeCount = 0;
		t_BuildInfo->pipeLayoutInfo.pPushConstantRanges = nullptr;
	}

	return PipelineBuilderHandle(t_BuildInfo);
}

void BB::VulkanPipelineBuilderBindDescriptor(const PipelineBuilderHandle a_Handle, const RDescriptorHandle a_Descriptor)
{
	constexpr uint32_t MAX_PUSHCONSTANTSIZE = 128;
	VKPipelineBuildInfo* t_BuildInfo = reinterpret_cast<VKPipelineBuildInfo*>(a_Handle.ptrHandle);
	VulkanBindingSet* t_Set = reinterpret_cast<VulkanBindingSet*>(a_Descriptor.ptrHandle);

	const uint32_t t_BindingSet = static_cast<uint32_t>(t_Set->bindingSet);

	t_BuildInfo->layout[t_BindingSet] = t_Set->setLayout;
	++t_BuildInfo->layoutCount;
}

void BB::VulkanPipelineBuilderBindShaders(const PipelineBuilderHandle a_Handle, const Slice<BB::ShaderCreateInfo> a_ShaderInfo)
{
	VKPipelineBuildInfo* t_BuildInfo = reinterpret_cast<VKPipelineBuildInfo*>(a_Handle.ptrHandle);

	BB_ASSERT(t_BuildInfo->pipeInfo.pStages == nullptr, "Vulkan: Already bound pipeline stages to the pipeline builder!");

	VkPipelineShaderStageCreateInfo* t_PipelineShaderStageInfo = BBnewArr(t_BuildInfo->buildAllocator, 
		a_ShaderInfo.size(), 
		VkPipelineShaderStageCreateInfo);

	VkShaderModule* t_ShaderModules = BBnewArr(t_BuildInfo->buildAllocator, 
		a_ShaderInfo.size(), 
		VkShaderModule);

	VkShaderModuleCreateInfo t_ShaderModCreateInfo{};
	t_ShaderModCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	for (size_t i = 0; i < a_ShaderInfo.size(); i++)
	{
		t_ShaderModCreateInfo.codeSize = a_ShaderInfo[i].buffer.size;
		t_ShaderModCreateInfo.pCode = reinterpret_cast<const uint32_t*>(a_ShaderInfo[i].buffer.data);

		VKASSERT(vkCreateShaderModule(s_VKB.device, &t_ShaderModCreateInfo, nullptr, &t_ShaderModules[i]),
			"Vulkan: Failed to create shadermodule.");

		t_PipelineShaderStageInfo[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		t_PipelineShaderStageInfo[i].stage = VKConv::ShaderStageBits(a_ShaderInfo[i].shaderStage);
		t_PipelineShaderStageInfo[i].module = t_ShaderModules[i];
		t_PipelineShaderStageInfo[i].pName = "main";
		t_PipelineShaderStageInfo[i].pSpecializationInfo = nullptr;
	}

	t_BuildInfo->pipeInfo.pStages = t_PipelineShaderStageInfo;
	t_BuildInfo->pipeInfo.stageCount = static_cast<uint32_t>(a_ShaderInfo.size());
}

void BB::VulkanPipelineBuilderBindAttributes(const PipelineBuilderHandle a_Handle, const PipelineAttributes& a_AttributeInfo)
{
	VKPipelineBuildInfo* t_BuildInfo = reinterpret_cast<VKPipelineBuildInfo*>(a_Handle.ptrHandle);
	BB_ASSERT(t_BuildInfo->pipeInfo.pVertexInputState == nullptr, "Vulkan: Already bound attributes to this pipeline builder!");


	VkVertexInputBindingDescription* t_BindingDescription = BBnew(
		t_BuildInfo->buildAllocator,
		VkVertexInputBindingDescription);
	t_BindingDescription->binding = 0;
	t_BindingDescription->stride = a_AttributeInfo.stride;
	t_BindingDescription->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription* t_AttributeDescriptions = BBnewArr(
		t_BuildInfo->buildAllocator,
		a_AttributeInfo.attributes.size(),
		VkVertexInputAttributeDescription);
	for (size_t i = 0; i < a_AttributeInfo.attributes.size(); i++)
	{
		t_AttributeDescriptions[i].binding = 0;
		t_AttributeDescriptions[i].location = a_AttributeInfo.attributes[i].location;
		t_AttributeDescriptions[i].offset = a_AttributeInfo.attributes[i].offset;
		switch (a_AttributeInfo.attributes[i].format)
		{
		case RENDER_INPUT_FORMAT::R32:
			t_AttributeDescriptions[i].format = VK_FORMAT_R32_SFLOAT;
			break;
		case RENDER_INPUT_FORMAT::RG32:
			t_AttributeDescriptions[i].format = VK_FORMAT_R32G32_SFLOAT;
			break;
		case RENDER_INPUT_FORMAT::RGB32:
			t_AttributeDescriptions[i].format = VK_FORMAT_R32G32B32_SFLOAT;
			break;
		case RENDER_INPUT_FORMAT::RGBA32:
			t_AttributeDescriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			break;
		case RENDER_INPUT_FORMAT::RG8:
			t_AttributeDescriptions[i].format = VK_FORMAT_R8G8_UNORM;
			break;
		case RENDER_INPUT_FORMAT::RGBA8:
			t_AttributeDescriptions[i].format = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		default:
			BB_ASSERT(false, "Vulkan: Input format not supported!");
			break;
		}
	}

	VkVertexInputBindingDescription t_BindingDesc{};
	VkVertexInputAttributeDescription t_AttribDescription{};

	VkPipelineVertexInputStateCreateInfo* t_VertexInputInfo = BBnew(
		t_BuildInfo->buildAllocator,
		VkPipelineVertexInputStateCreateInfo);
	t_VertexInputInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	t_VertexInputInfo->vertexBindingDescriptionCount = 1;
	t_VertexInputInfo->pVertexBindingDescriptions = t_BindingDescription;
	t_VertexInputInfo->vertexAttributeDescriptionCount = static_cast<uint32_t>(a_AttributeInfo.attributes.size());
	t_VertexInputInfo->pVertexAttributeDescriptions = t_AttributeDescriptions;
	t_BuildInfo->pipeInfo.pVertexInputState = t_VertexInputInfo;
}



PipelineHandle BB::VulkanPipelineBuildPipeline(const PipelineBuilderHandle a_Handle)
{
	VulkanPipeline t_Pipeline{};
	VKPipelineBuildInfo* t_BuildInfo = reinterpret_cast<VKPipelineBuildInfo*>(a_Handle.ptrHandle);

	{
		t_BuildInfo->pipeLayoutInfo.setLayoutCount = t_BuildInfo->layoutCount;
		t_BuildInfo->pipeLayoutInfo.pSetLayouts = t_BuildInfo->layout;

		PipelineLayoutHash t_DescriptorHash = HashPipelineLayoutInfo(t_BuildInfo->pipeLayoutInfo);
		VkPipelineLayout* t_FoundLayout = s_VKB.pipelineLayouts.find(t_DescriptorHash);

		if (t_FoundLayout != nullptr)
		{ 
			t_Pipeline.layout = *t_FoundLayout;
		}
		else
		{
			VKASSERT(vkCreatePipelineLayout(s_VKB.device,
				&t_BuildInfo->pipeLayoutInfo,
				nullptr,
				&t_Pipeline.layout),
				"Vulkan: Failed to create pipelinelayout.");

			s_VKB.pipelineLayouts.insert(t_DescriptorHash, t_Pipeline.layout);
		}

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
		VkPipelineViewportStateCreateInfo t_ViewportState{};
		t_ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		t_ViewportState.viewportCount = 1;
		t_ViewportState.pViewports = nullptr;
		t_ViewportState.scissorCount = 1;
		t_ViewportState.pScissors = nullptr;

		VkPipelineInputAssemblyStateCreateInfo t_InputAssembly{};
		t_InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		t_InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		t_InputAssembly.primitiveRestartEnable = VK_FALSE;



		VkPipelineMultisampleStateCreateInfo t_Multisampling{};
		t_Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		t_Multisampling.sampleShadingEnable = VK_FALSE;
		t_Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		t_Multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		t_Multisampling.alphaToOneEnable = VK_FALSE; // Optional
		t_Multisampling.minSampleShading = 1.0f; // Optional
		t_Multisampling.pSampleMask = nullptr; // Optional

		//viewport is always controlled by the dynamic state so we just initialize them here.
		t_BuildInfo->pipeInfo.pViewportState = &t_ViewportState;
		t_BuildInfo->pipeInfo.pDynamicState = &t_DynamicPipeCreateInfo;
		t_BuildInfo->pipeInfo.pInputAssemblyState = &t_InputAssembly;
		t_BuildInfo->pipeInfo.pMultisampleState = &t_Multisampling;

		//Optimalization for later.
		t_BuildInfo->pipeInfo.basePipelineHandle = VK_NULL_HANDLE;
		t_BuildInfo->pipeInfo.basePipelineIndex = -1;

		VKASSERT(vkCreateGraphicsPipelines(s_VKB.device,
			VK_NULL_HANDLE,
			1,
			&t_BuildInfo->pipeInfo,
			nullptr,
			&t_Pipeline.pipeline),
			"Vulkan: Failed to create graphics Pipeline.");

		for (uint32_t i = 0; i < t_BuildInfo->pipeInfo.stageCount; i++)
		{
			vkDestroyShaderModule(s_VKB.device,
				t_BuildInfo->pipeInfo.pStages[i].module,
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

	vkResetCommandPool(s_VKB.device,
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

void BB::VulkanStartRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_RenderInfo)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);

	VkImageMemoryBarrier t_ColorBarrier{};
	//Color attachment.
	t_ColorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	t_ColorBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	t_ColorBarrier.oldLayout = VKConv::ImageLayout(a_RenderInfo.colorInitialLayout);
	t_ColorBarrier.newLayout = VKConv::ImageLayout(a_RenderInfo.colorFinalLayout);
	t_ColorBarrier.image = s_VKB.swapChain.frames[s_VKB.currentFrame].image;
	t_ColorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	t_ColorBarrier.subresourceRange.baseArrayLayer = 0;
	t_ColorBarrier.subresourceRange.layerCount = 1;
	t_ColorBarrier.subresourceRange.baseMipLevel = 0;
	t_ColorBarrier.subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(t_Cmdlist->Buffer(),
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&t_ColorBarrier);

	VkRenderingInfo t_RenderInfo{};
	VkRenderingAttachmentInfo t_RenderDepthAttach{};

	//If we handle the depth stencil we do that here. 
	if (a_RenderInfo.depthStencil.ptrHandle != nullptr) 
	{
		VkImageMemoryBarrier t_DepthBarrier{};
		t_DepthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		t_DepthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		t_DepthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		t_DepthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		t_DepthBarrier.image = reinterpret_cast<VulkanImage*>(a_RenderInfo.depthStencil.ptrHandle)->image;
		t_DepthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		t_DepthBarrier.subresourceRange.baseArrayLayer = 0;
		t_DepthBarrier.subresourceRange.layerCount = 1;
		t_DepthBarrier.subresourceRange.baseMipLevel = 0;
		t_DepthBarrier.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(t_Cmdlist->Buffer(),
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			&t_DepthBarrier);

		t_Cmdlist->depthImage = t_DepthBarrier.image;
		
		t_RenderDepthAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		t_RenderDepthAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		t_RenderDepthAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		t_RenderDepthAttach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		t_RenderDepthAttach.imageView = reinterpret_cast<VulkanImage*>(a_RenderInfo.depthStencil.ptrHandle)->view;
		t_RenderDepthAttach.clearValue.depthStencil = { 1.0f, 0 };
		t_RenderInfo.pDepthAttachment = &t_RenderDepthAttach;
		t_RenderInfo.pStencilAttachment = &t_RenderDepthAttach;
	}

	VkRenderingAttachmentInfo t_RenderColorAttach{};
	t_RenderColorAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	t_RenderColorAttach.loadOp = VKConv::LoadOP(a_RenderInfo.colorLoadOp);
	t_RenderColorAttach.storeOp = VKConv::StoreOp(a_RenderInfo.colorStoreOp);
	t_RenderColorAttach.imageLayout = VKConv::ImageLayout(a_RenderInfo.colorFinalLayout); //Get the layout after the memory barrier.
	t_RenderColorAttach.imageView = s_VKB.swapChain.frames[s_VKB.currentFrame].imageView;
	t_RenderColorAttach.clearValue.color.float32[0] = a_RenderInfo.clearColor[0];
	t_RenderColorAttach.clearValue.color.float32[1] = a_RenderInfo.clearColor[1];
	t_RenderColorAttach.clearValue.color.float32[2] = a_RenderInfo.clearColor[2];
	t_RenderColorAttach.clearValue.color.float32[3] = a_RenderInfo.clearColor[3];

	VkRect2D t_Scissor{};
	t_Scissor.offset = { 0, 0 };
	t_Scissor.extent.width = a_RenderInfo.viewportWidth;
	t_Scissor.extent.height = a_RenderInfo.viewportHeight;

	t_RenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	t_RenderInfo.renderArea = t_Scissor;
	t_RenderInfo.layerCount = 1;
	t_RenderInfo.pColorAttachments = &t_RenderColorAttach;
	t_RenderInfo.colorAttachmentCount = 1;
	t_RenderInfo.pNext = nullptr;

	vkCmdBeginRendering(t_Cmdlist->Buffer(), &t_RenderInfo);

	VkViewport t_Viewport{};
	t_Viewport.x = 0.0f;
	t_Viewport.y = 0.0f;
	t_Viewport.width = static_cast<float>(a_RenderInfo.viewportWidth);
	t_Viewport.height = static_cast<float>(a_RenderInfo.viewportHeight);
	t_Viewport.minDepth = 0.0f;
	t_Viewport.maxDepth = 1.0f;
	vkCmdSetViewport(t_Cmdlist->Buffer(), 0, 1, &t_Viewport);


	vkCmdSetScissor(t_Cmdlist->Buffer(), 0, 1, &t_Scissor);
}

void BB::VulkanSetScissor(const RecordingCommandListHandle a_RecordingCmdHandle, const ScissorInfo& a_ScissorInfo)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);

	VkRect2D t_Scissor{};
	t_Scissor.offset.x = a_ScissorInfo.offset.x;
	t_Scissor.offset.y = a_ScissorInfo.offset.y;
	t_Scissor.extent.width = a_ScissorInfo.extent.x;
	t_Scissor.extent.height = a_ScissorInfo.extent.y;

	vkCmdSetScissor(t_Cmdlist->Buffer(), 0, 1, &t_Scissor);
}

void BB::VulkanEndRendering(const RecordingCommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);
	vkCmdEndRendering(t_Cmdlist->Buffer());

	VkImageMemoryBarrier t_PresentBarrier{};
	t_PresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	t_PresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	t_PresentBarrier.oldLayout = VKConv::ImageLayout(a_EndInfo.colorInitialLayout);
	t_PresentBarrier.newLayout = VKConv::ImageLayout(a_EndInfo.colorFinalLayout);
	t_PresentBarrier.image = s_VKB.swapChain.frames[s_VKB.currentFrame].image;
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

	t_Cmdlist->depthImage = VK_NULL_HANDLE;
}

void BB::VulkanCopyBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferInfo& a_CopyInfo)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);
	VulkanBuffer* t_SrcBuffer = reinterpret_cast<VulkanBuffer*>(a_CopyInfo.src.handle);
	VulkanBuffer* t_DstBuffer = reinterpret_cast<VulkanBuffer*>(a_CopyInfo.dst.handle);

	VkBufferCopy t_CopyRegion{};
	t_CopyRegion.srcOffset = a_CopyInfo.srcOffset;
	t_CopyRegion.dstOffset = a_CopyInfo.dstOffset;
	t_CopyRegion.size = a_CopyInfo.size;

	vkCmdCopyBuffer(t_Cmdlist->Buffer(),
		t_SrcBuffer->buffer,
		t_DstBuffer->buffer,
		1,
		&t_CopyRegion);
}

void BB::VulkanCopyBufferImage(const RecordingCommandListHandle a_RecordingCmdHandle, const RenderCopyBufferImageInfo& a_CopyInfo)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);
	VulkanBuffer* t_SrcBuffer = reinterpret_cast<VulkanBuffer*>(a_CopyInfo.srcBuffer.handle);
	VulkanImage* t_DstImage = reinterpret_cast<VulkanImage*>(a_CopyInfo.dstImage.ptrHandle);

	VkBufferImageCopy t_CopyRegion{};
	t_CopyRegion.bufferOffset = a_CopyInfo.srcBufferOffset;
	t_CopyRegion.bufferRowLength = 0;
	t_CopyRegion.bufferImageHeight = 0;

	t_CopyRegion.imageExtent.width = a_CopyInfo.dstImageInfo.sizeX;
	t_CopyRegion.imageExtent.height = a_CopyInfo.dstImageInfo.sizeY;
	t_CopyRegion.imageExtent.depth = a_CopyInfo.dstImageInfo.sizeZ;

	t_CopyRegion.imageOffset.x = a_CopyInfo.dstImageInfo.offsetX;
	t_CopyRegion.imageOffset.y = a_CopyInfo.dstImageInfo.offsetY;
	t_CopyRegion.imageOffset.z = a_CopyInfo.dstImageInfo.offsetZ;

	t_CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	t_CopyRegion.imageSubresource.mipLevel = a_CopyInfo.dstImageInfo.mipLevel;
	t_CopyRegion.imageSubresource.baseArrayLayer = a_CopyInfo.dstImageInfo.baseArrayLayer;
	t_CopyRegion.imageSubresource.layerCount = a_CopyInfo.dstImageInfo.layerCount;

	vkCmdCopyBufferToImage(t_Cmdlist->Buffer(),
		t_SrcBuffer->buffer,
		t_DstImage->image,
		VKConv::ImageLayout(a_CopyInfo.dstImageInfo.layout),
		1,
		&t_CopyRegion);
}

void BB::VulkanTransitionImage(RecordingCommandListHandle a_RecordingCmdHandle, const RenderTransitionImageInfo& a_TransitionInfo)
{
	VkImageMemoryBarrier t_Barrier{};
	t_Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	t_Barrier.oldLayout = VKConv::ImageLayout(a_TransitionInfo.oldLayout);
	t_Barrier.newLayout = VKConv::ImageLayout(a_TransitionInfo.newLayout);
	t_Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	t_Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	t_Barrier.image = reinterpret_cast<VulkanImage*>(a_TransitionInfo.image.ptrHandle)->image;
	if (a_TransitionInfo.newLayout == RENDER_IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT || 
		a_TransitionInfo.oldLayout == RENDER_IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT)
		t_Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else
		t_Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	t_Barrier.subresourceRange.baseMipLevel = a_TransitionInfo.baseMipLevel;
	t_Barrier.subresourceRange.levelCount = a_TransitionInfo.levelCount;
	t_Barrier.subresourceRange.baseArrayLayer = a_TransitionInfo.baseArrayLayer;
	t_Barrier.subresourceRange.layerCount = a_TransitionInfo.layerCount;
	t_Barrier.srcAccessMask = VKConv::AccessMask(a_TransitionInfo.srcMask);
	t_Barrier.dstAccessMask = VKConv::AccessMask(a_TransitionInfo.dstMask);

	vkCmdPipelineBarrier(reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle)->Buffer(),
		VKConv::PipelineStage(a_TransitionInfo.srcStage),
		VKConv::PipelineStage(a_TransitionInfo.dstStage),
		0,
		0, nullptr,
		0, nullptr,
		1, &t_Barrier);
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

void BB::VulkanBindDescriptors(const RecordingCommandListHandle a_RecordingCmdHandle, const RDescriptorHandle* a_Sets, const uint32_t a_SetCount, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets)
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

void BB::VulkanBindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_Offset, const void* a_Data)
{
	VulkanCommandList* t_Cmdlist = reinterpret_cast<VulkanCommandList*>(a_RecordingCmdHandle.ptrHandle);
	
	vkCmdPushConstants(t_Cmdlist->Buffer(),
		t_Cmdlist->currentPipelineLayout,
		VK_SHADER_STAGE_ALL,
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

void BB::VulkanResizeWindow(const uint32_t a_X, const uint32_t a_Y)
{
	VulkanWaitDeviceReady();

	//Creates the swapchain with the image views.
	CreateSwapchain(s_VKB.surface,
		s_VKB.physicalDevice,
		s_VKB.device,
		a_X,
		a_Y,
		true);
}

void BB::VulkanStartFrame(const StartFrameInfo& a_StartInfo)
{
	FrameIndex t_CurrentFrame = s_VKB.currentFrame;

	VKASSERT(vkAcquireNextImageKHR(s_VKB.device,
		s_VKB.swapChain.swapChain,
		UINT64_MAX,
		s_VKB.swapChain.frames[s_VKB.currentFrame].imageAvailableSem,
		VK_NULL_HANDLE,
		&s_VKB.imageIndex),
		"Vulkan: failed to get next image.");

	//For now not wait for semaphores, may be required later.
	VkSemaphoreWaitInfo t_WaitInfo{};
	t_WaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	t_WaitInfo.semaphoreCount = 1;
	t_WaitInfo.pSemaphores = &s_VKB.swapChain.frames[s_VKB.currentFrame].frameTimelineSemaphore;
	t_WaitInfo.pValues = &s_VKB.swapChain.frames[s_VKB.currentFrame].frameWaitValue;
	vkWaitSemaphores(s_VKB.device, &t_WaitInfo, 1000000000);
}

void BB::VulkanExecuteCommands(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount)
{
	VkTimelineSemaphoreSubmitInfo* t_TimelineInfos = BBnewArr(s_VulkanTempAllocator,
		a_ExecuteInfoCount,
		VkTimelineSemaphoreSubmitInfo);
	VkSubmitInfo* t_SubmitInfos = BBnewArr(s_VulkanTempAllocator,
		a_ExecuteInfoCount,
		VkSubmitInfo);

	for (uint32_t i = 0; i < a_ExecuteInfoCount; i++)
	{
		VkCommandBuffer* t_CmdBuffers = BBnewArr(s_VulkanTempAllocator,
			a_ExecuteInfos[i].commandCount,
			VkCommandBuffer);
		for (uint32_t j = 0; j < a_ExecuteInfos[i].commandCount; j++)
		{
			t_CmdBuffers[j] = s_VKB.commandLists[a_ExecuteInfos[i].commands[j].handle].Buffer();
		}

		const uint32_t t_WaitSemCount = a_ExecuteInfos[i].waitQueueCount;
		const uint32_t t_SignalSemCount = a_ExecuteInfos[i].signalQueueCount;
		VkPipelineStageFlags* t_WaitStagesMask = nullptr;
		if (t_WaitSemCount != 0)
			t_WaitStagesMask = BBnewArr(s_VulkanTempAllocator,
				t_WaitSemCount,
				VkPipelineStageFlags);

		VkSemaphore* t_Semaphores = BBnewArr(s_VulkanTempAllocator,
			t_WaitSemCount + t_SignalSemCount + 1,
			VkSemaphore);
		uint64_t* t_SemValues = BBnewArr(s_VulkanTempAllocator,
			t_WaitSemCount + t_SignalSemCount + 1,
			uint64_t);

		//SETTING THE WAIT
		for (uint32_t j = 0; j < t_WaitSemCount; j++)
		{
			t_Semaphores[j] = reinterpret_cast<VulkanCommandQueue*>(
				a_ExecuteInfos[i].waitQueues[j].ptrHandle)->timelineSemaphore;
			t_SemValues[j] = a_ExecuteInfos[i].waitValues[j];;

			t_WaitStagesMask[j] = VKConv::PipelineStage(a_ExecuteInfos[i].waitStages[j]);
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

void BB::VulkanExecutePresentCommand(CommandQueueHandle a_ExecuteQueue, const ExecuteCommandsInfo& a_ExecuteInfo)
{
	VkCommandBuffer* t_CmdBuffers = BBnewArr(s_VulkanTempAllocator,
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

	VkPipelineStageFlags* t_WaitStagesMask = BBnewArr(s_VulkanTempAllocator,
		t_WaitSemCount,
		VkPipelineStageFlags);

	VkSemaphore* t_Semaphores = BBnewArr(s_VulkanTempAllocator,
		t_WaitSemCount + t_SignalSemCount,
		VkSemaphore);
	uint64_t* t_SemValues = BBnewArr(s_VulkanTempAllocator,
		t_WaitSemCount + t_SignalSemCount,
		uint64_t);

	//SETTING THE WAIT
	//Set the wait semaphore so that it must wait until it can present.
	t_Semaphores[0] = s_VKB.swapChain.frames[s_VKB.currentFrame].imageAvailableSem;
	t_SemValues[0] = 0;
	t_WaitStagesMask[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;


	//Get the semaphore from the queues.
	for (uint32_t i = 0; i < t_WaitSemCount - 1; i++)
	{
		t_Semaphores[i + 1] = reinterpret_cast<VulkanCommandQueue*>(
			a_ExecuteInfo.waitQueues[i].ptrHandle)->timelineSemaphore;
		t_SemValues[i + 1] = a_ExecuteInfo.waitValues[i];
		t_WaitStagesMask[i + 1] = VKConv::PipelineStage(a_ExecuteInfo.waitStages[i]);
	}

	//SETTING THE SIGNAL
	//signal the binary semaphore to signal that the image is being worked on.
	t_Semaphores[t_WaitSemCount] = s_VKB.swapChain.frames[s_VKB.currentFrame].imageRenderFinishedSem;
	t_SemValues[t_WaitSemCount] = 0;
	//signal the binary semaphore to signal that the image is being worked on.
	t_Semaphores[t_WaitSemCount + 1] = s_VKB.swapChain.frames[s_VKB.currentFrame].frameTimelineSemaphore;
	//Increment the semaphore by 1 for the next frame to get.
	t_SemValues[t_WaitSemCount + 1] = ++s_VKB.swapChain.frames[s_VKB.currentFrame].frameWaitValue;
	for (uint32_t i = 0; i < t_SignalSemCount - 2; i++)
	{
		t_Semaphores[t_WaitSemCount + i + 1] = reinterpret_cast<VulkanCommandQueue*>(
			a_ExecuteInfo.signalQueues[i].ptrHandle)->timelineSemaphore;
		//Increment the next sem value for signal
		t_SemValues[t_WaitSemCount + i + 1] = reinterpret_cast<VulkanCommandQueue*>(
			a_ExecuteInfo.signalQueues[i].ptrHandle)->nextSemValue++;
	}

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

FrameIndex BB::VulkanPresentFrame(const PresentFrameInfo& a_PresentInfo)
{
	const uint32_t t_CurrentFrame = s_VKB.currentFrame;

	VkPresentInfoKHR t_PresentInfo{};
	t_PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	t_PresentInfo.waitSemaphoreCount = 1;
	t_PresentInfo.pWaitSemaphores = &s_VKB.swapChain.frames[s_VKB.currentFrame].imageRenderFinishedSem;
	t_PresentInfo.swapchainCount = 1; //Swapchain will always be 1
	t_PresentInfo.pSwapchains = &s_VKB.swapChain.swapChain;
	t_PresentInfo.pImageIndices = &s_VKB.imageIndex;
	t_PresentInfo.pResults = nullptr;

	VKASSERT(vkQueuePresentKHR(s_VKB.presentQueue, &t_PresentInfo),
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
	vkDeviceWaitIdle(s_VKB.device);
}

void BB::VulkanDestroyFence(const RFenceHandle a_Handle)
{
	vkDestroyFence(s_VKB.device,
		reinterpret_cast<VkFence>(a_Handle.ptrHandle),
		nullptr);
}

void BB::VulkanDestroySampler(const RSamplerHandle a_Handle)
{
	vkDestroySampler(s_VKB.device,
		reinterpret_cast<VkSampler>(a_Handle.ptrHandle),
		nullptr);
}

void BB::VulkanDestroyImage(const RImageHandle a_Handle)
{
	VulkanImage* t_Image = reinterpret_cast<VulkanImage*>(a_Handle.ptrHandle);
	vkDestroyImageView(s_VKB.device, t_Image->view, nullptr);
	vmaDestroyImage(s_VKB.vma, t_Image->image, t_Image->allocation);
	s_VKB.imagePool.Free(t_Image);
}

void BB::VulkanDestroyBuffer(RBufferHandle a_Handle)
{
	VulkanBuffer* t_Buffer = reinterpret_cast<VulkanBuffer*>(a_Handle.ptrHandle);
	vmaDestroyBuffer(s_VKB.vma, t_Buffer->buffer, t_Buffer->allocation);
	s_VKB.bufferPool.Free(t_Buffer);
}

void BB::VulkanDestroyCommandQueue(const CommandQueueHandle a_Handle)
{
	VulkanCommandQueue* t_CmdQueue = reinterpret_cast<VulkanCommandQueue*>(a_Handle.ptrHandle);
	vkDestroySemaphore(s_VKB.device,
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
	vkDestroyCommandPool(s_VKB.device, t_CmdAllocator->pool, nullptr);
	s_VKB.cmdAllocators.Free(t_CmdAllocator);
}

void BB::VulkanDestroyCommandList(const CommandListHandle a_Handle)
{
	VulkanCommandList& a_List = s_VKB.commandLists[a_Handle.handle];
	a_List.cmdAllocator->FreeCommandList(a_List); //Place back in the freelist.
	s_VKB.commandLists.erase(a_Handle.handle);
}

void BB::VulkanDestroyDescriptor(const RDescriptorHandle a_Handle)
{
	VulkanBindingSet* t_Set = reinterpret_cast<VulkanBindingSet*>(a_Handle.ptrHandle);
	*t_Set = {}; //zero it for safety
	//maybe store the sets? For now we just get new ones.
	s_VKB.bindingSetPool.Free(t_Set);
}

void BB::VulkanDestroyPipeline(const PipelineHandle a_Handle)
{
	VulkanPipeline* t_Pipeline = reinterpret_cast<VulkanPipeline*>(a_Handle.handle);

	vkDestroyPipeline(s_VKB.device,
		t_Pipeline->pipeline,
		nullptr);
	vkDestroyDescriptorSetLayout(s_VKB.device,
		t_Pipeline->setLayout,
		nullptr);
}

void BB::VulkanDestroyBackend()
{
	for (auto t_It = s_VKB.pipelineLayouts.begin();
		t_It < s_VKB.pipelineLayouts.end(); t_It++)
	{
		vkDestroyPipelineLayout(s_VKB.device,
			*t_It->value,
			nullptr);
	}
	s_VKB.pipelineLayouts.clear();

	s_VKB.descriptorAllocator.Destroy();

	for (size_t i = 0; i < s_VKB.frameCount; i++)
	{
		vkDestroyImageView(s_VKB.device,
			s_VKB.swapChain.frames[i].imageView, nullptr);
		vkDestroySemaphore(s_VKB.device,
			s_VKB.swapChain.frames[i].frameTimelineSemaphore, nullptr);
		vkDestroySemaphore(s_VKB.device,
			s_VKB.swapChain.frames[i].imageAvailableSem, nullptr);
		vkDestroySemaphore(s_VKB.device,
			s_VKB.swapChain.frames[i].imageRenderFinishedSem, nullptr);
	}

	vkDestroySwapchainKHR(s_VKB.device,
		s_VKB.swapChain.swapChain,
		nullptr);
	vmaDestroyAllocator(s_VKB.vma);
	vkDestroyDevice(s_VKB.device, nullptr);

	if (s_VKB.vulkanDebug.debugMessenger != 0)
	{
		auto t_DestroyDebugFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_VKB.instance, 
			"vkDestroyDebugUtilsMessengerEXT");
		if (t_DestroyDebugFunc == nullptr)
		{
			BB_WARNING(false, "Failed to get the vkDestroyDebugUtilsMessengerEXT function pointer.", WarningType::HIGH);
		}
		t_DestroyDebugFunc(s_VKB.instance, s_VKB.vulkanDebug.debugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(s_VKB.instance, s_VKB.surface, nullptr);
	vkDestroyInstance(s_VKB.instance, nullptr);

	s_VKB.DestroyPools();

	//clear all the vulkan memory.
	//s_VulkanAllocator.Clear();
}
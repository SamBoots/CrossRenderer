#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1003000 // Vulkan 1.3
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

constexpr int VULKAN_VERSION = 3;
constexpr int RESOURCE_DESCRIPTOR_BUFFER_INDEX = 0;
constexpr int SAMPLER_DESCRIPTOR_BUFFER_INDEX = 1;

#include "Storage/Hashmap.h"
#include "Storage/Slotmap.h"
#include "Storage/Pool.h"
#include "Allocators/RingAllocator.h"
#include "Allocators/TemporaryAllocator.h"
#include "BBMemory.h"
#include "Math.inl"
#include "Program.h"

#include "VulkanCommon.h"

#include <iostream>

using namespace BB;
static PFN_vkGetDescriptorSetLayoutSizeEXT GetDescriptorSetLayoutSizeEXT;
static PFN_vkGetDescriptorSetLayoutBindingOffsetEXT GetDescriptorSetLayoutBindingOffsetEXT;
static PFN_vkGetDescriptorEXT GetDescriptorEXT;
static PFN_vkCmdBindDescriptorBuffersEXT CmdBindDescriptorBuffersEXT;
static PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT CmdBindDescriptorBufferEmbeddedSamplersEXT;
static PFN_vkCmdSetDescriptorBufferOffsetsEXT CmdSetDescriptorBufferOffsetsEXT;
static PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;

static inline void VulkanLoadFunctions(VkInstance a_Instance)
{
	GetDescriptorSetLayoutSizeEXT = (PFN_vkGetDescriptorSetLayoutSizeEXT)vkGetInstanceProcAddr(a_Instance, "vkGetDescriptorSetLayoutSizeEXT");
	GetDescriptorSetLayoutBindingOffsetEXT = (PFN_vkGetDescriptorSetLayoutBindingOffsetEXT)vkGetInstanceProcAddr(a_Instance, "vkGetDescriptorSetLayoutBindingOffsetEXT");
	GetDescriptorEXT = (PFN_vkGetDescriptorEXT)vkGetInstanceProcAddr(a_Instance, "vkGetDescriptorEXT");
	CmdBindDescriptorBuffersEXT = (PFN_vkCmdBindDescriptorBuffersEXT)vkGetInstanceProcAddr(a_Instance, "vkCmdBindDescriptorBuffersEXT");
	CmdBindDescriptorBufferEmbeddedSamplersEXT = (PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT)vkGetInstanceProcAddr(a_Instance, "vkCmdBindDescriptorBufferEmbeddedSamplersEXT");
	CmdSetDescriptorBufferOffsetsEXT = (PFN_vkCmdSetDescriptorBufferOffsetsEXT)vkGetInstanceProcAddr(a_Instance, "vkCmdSetDescriptorBufferOffsetsEXT");
	SetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(a_Instance, "vkSetDebugUtilsObjectNameEXT");
}

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

struct VKPipelineBuildInfo
{
	const char* name;
	//temporary allocator, this gets removed when we are finished building.
	TemporaryAllocator buildAllocator{ s_VulkanAllocator };

	VkGraphicsPipelineCreateInfo pipeInfo{};
	VkPipelineRenderingCreateInfo dynamicRenderingInfo{}; //attachment for dynamic rendering.

	VkPipelineLayoutCreateInfo pipeLayoutInfo{};

	uint32_t layoutCount;
	//Also accept immutable samplers
	VkDescriptorSetLayout layout[BINDING_MAX + 1];
};

using PipelineLayoutHash = uint64_t;
struct VulkanBackend_inst
{
	uint32_t currentFrame = 0;
	FrameIndex frameCount = 0;
	uint32_t imageIndex = 0;

	uint32_t minReadonlyConstantOffset;
	uint32_t minReadonlyBufferOffset;
	uint32_t minReadWriteBufferOffset;

	VkInstance instance{};
	VkSurfaceKHR surface{};

	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkQueue presentQueue;

	VulkanQueuesIndices queueIndices;

	VulkanSwapChain swapChain{};
	VmaAllocator vma{};
	Slotmap<VulkanCommandList> commandLists{ s_VulkanAllocator };

	Pool<VkCommandAllocator> cmdAllocators;
	Pool<VulkanPipeline> pipelinePool;
	Pool<VulkanBuffer> bufferPool;
	Pool<VulkanImage> imagePool;

	OL_HashMap<PipelineLayoutHash, VkPipelineLayout> pipelineLayouts{ s_VulkanAllocator };

	VulkanDebug vulkanDebug;
	VulkanPhysicalDeviceInfo deviceInfo;

	void CreatePools()
	{
		//JANK, will handle command allocators way more efficiently on the app side.
		cmdAllocators.CreatePool(s_VulkanAllocator, 128);
		pipelinePool.CreatePool(s_VulkanAllocator, 8);
		bufferPool.CreatePool(s_VulkanAllocator, 32);
		imagePool.CreatePool(s_VulkanAllocator, 16);
	}

	void DestroyPools()
	{
		cmdAllocators.DestroyPool(s_VulkanAllocator);
		pipelinePool.DestroyPool(s_VulkanAllocator);
		bufferPool.DestroyPool(s_VulkanAllocator);
		imagePool.DestroyPool(s_VulkanAllocator);
	}
};
static VulkanBackend_inst s_VKB;
static uint32_t s_DescriptorBiggestResourceType = 0;
static uint32_t s_DescriptorSamplerSize = 0;
static uint32_t s_DescriptorTypeSize[static_cast<uint32_t>(RENDER_DESCRIPTOR_TYPE::ENUM_SIZE)];
static uint32_t s_DescriptorBufferAlignment = 0;

#ifdef _DEBUG
static inline void SetDebugName_f(const char* a_Name, const uint64_t a_ObjectHandle, const VkObjectType a_ObjType)
{
	VkDebugUtilsObjectNameInfoEXT t_DebugName{};
	t_DebugName.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	t_DebugName.pNext = nullptr;
	t_DebugName.objectType = a_ObjType;
	t_DebugName.objectHandle = a_ObjectHandle;
	t_DebugName.pObjectName = a_Name;
	SetDebugUtilsObjectNameEXT(s_VKB.device, &t_DebugName);
}

#define SetDebugName(a_Name, a_ObjectHandle, a_ObjType) SetDebugName_f(a_Name, (uint64_t)a_ObjectHandle, a_ObjType)
#else
#define SetDebugName()
#endif _DEBUG

static inline VkDeviceSize PadUBOBufferSize(const VkDeviceSize a_BuffSize)
{
	VkPhysicalDeviceProperties t_Properties;
	vkGetPhysicalDeviceProperties(s_VKB.physicalDevice, &t_Properties);
	return Pointer::AlignPad(a_BuffSize, t_Properties.limits.minUniformBufferOffsetAlignment);
}

static inline VkDeviceSize GetBufferDeviceAddress(const VkBuffer a_Buffer)
{
	VkBufferDeviceAddressInfoKHR t_BuffAddressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	t_BuffAddressInfo.buffer = a_Buffer;
	return vkGetBufferDeviceAddress(s_VKB.device, &t_BuffAddressInfo);
}

static VkSampler CreateSampler(const SamplerCreateInfo& a_CreateInfo)
{
	VkSampler t_Sampler{};
	VkSamplerCreateInfo t_SamplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	t_SamplerInfo.addressModeU = VKConv::AddressMode(a_CreateInfo.addressModeU);
	t_SamplerInfo.addressModeV = VKConv::AddressMode(a_CreateInfo.addressModeV);
	t_SamplerInfo.addressModeW = VKConv::AddressMode(a_CreateInfo.addressModeW);
	switch (a_CreateInfo.filter)
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
	t_SamplerInfo.minLod = a_CreateInfo.minLod;
	t_SamplerInfo.maxLod = a_CreateInfo.maxLod;
	t_SamplerInfo.mipLodBias = 0;
	if (a_CreateInfo.maxAnistoropy > 0)
	{
		t_SamplerInfo.anisotropyEnable = VK_TRUE;
		t_SamplerInfo.maxAnisotropy = a_CreateInfo.maxAnistoropy;
	}

	VKASSERT(vkCreateSampler(s_VKB.device, &t_SamplerInfo, nullptr, &t_Sampler),
		"Vulkan: Failed to create image sampler!");
	SetDebugName(a_CreateInfo.name, t_Sampler, VK_OBJECT_TYPE_SAMPLER);

	return t_Sampler;
}

//maybe make this a freelist, make sure to free it in DX12DestroyPipeline if I decide to add this.
class VulkanDescriptorBuffer
{
public:
	VulkanDescriptorBuffer(const uint32_t a_BufferSize, const VkBufferUsageFlags a_Usage, const bool a_GPUHeap, const char* a_Name)
		: m_BufferSize(a_BufferSize)
	{
		if (a_GPUHeap)
		{
			VkBufferCreateInfo t_BufferInfo{};
			t_BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			t_BufferInfo.size = a_BufferSize;
			t_BufferInfo.usage = a_Usage;
			t_BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocationCreateInfo t_VmaAlloc{};
			t_VmaAlloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			t_VmaAlloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

			VKASSERT(vmaCreateBuffer(s_VKB.vma,
				&t_BufferInfo, &t_VmaAlloc,
				&m_Buffer, &m_Allocation,
				nullptr), "Vulkan::VMA, Failed to allocate memory for a descriptor buffer");

			VKASSERT(vmaMapMemory(s_VKB.vma, m_Allocation, &m_Start),
				"Vulkan: Failed to map memory for descriptor buffer");
			m_StartAddress = GetBufferDeviceAddress(m_Buffer);

			SetDebugName(a_Name, m_Buffer, VK_OBJECT_TYPE_BUFFER);
		}
		else
		{
			//Mark m_Buffer is NULL so that we can be sure that it's a CPU heap.
			m_Buffer = VK_NULL_HANDLE;
			m_Start = ReserveVirtualMemory(m_BufferSize);
			CommitVirtualMemory(m_Start, m_BufferSize);
		}
	}

	~VulkanDescriptorBuffer()
	{
		//If m_Buffer is initialized then it's a GPU heap.
		if (m_Buffer)
		{
			vmaUnmapMemory(s_VKB.vma, m_Allocation);
			vmaDestroyBuffer(s_VKB.vma, m_Buffer, m_Allocation);
		}
		else
		{
			ReleaseVirtualMemory(m_Start);
		}
		memset(this, 0, sizeof(VulkanDescriptorBuffer));
	}

	inline DescriptorAllocation Allocate(const RDescriptor a_Layout, const uint32_t a_HeapOffset)
	{
		VulkanDescriptor* t_Desc = reinterpret_cast<VulkanDescriptor*>(a_Layout.handle);
		const VkDeviceSize t_AllocSize = Pointer::AlignPad(static_cast<size_t>(s_DescriptorBiggestResourceType) * t_Desc->descriptorCount,
			static_cast<size_t>(s_DescriptorBufferAlignment) * 4);

		const uint32_t t_DescriptorCount = static_cast<uint32_t>(t_AllocSize / s_DescriptorBiggestResourceType);
		
		DescriptorAllocation t_Allocation{};
		t_Allocation.descriptorCount = t_DescriptorCount;
		t_Allocation.offset = a_HeapOffset * s_DescriptorBiggestResourceType;
		t_Allocation.descriptor = a_Layout;
		t_Allocation.bufferStart = m_Start;
		return t_Allocation;
	}

	void inline Reset()
	{
		memset(m_Start, 0, m_BufferSize);
	}

	const inline VkBuffer GetBuffer() const { return m_Buffer; }
	inline void* GetStartOfBuffer() const { return m_Start; }
	const inline VkDeviceAddress GetBaseAddress() const { return m_StartAddress; }

private:
	VkBuffer m_Buffer;
	VmaAllocation m_Allocation = VK_NULL_HANDLE;
	//using uint32_t since descriptor buffers on some drivers
	//only spend 32-bits virtual address.
	uint32_t m_BufferSize;
	void* m_Start;
	VkDeviceAddress m_StartAddress = 0;
};

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
	VkExtensionProperties* t_Extensions = (VkExtensionProperties*)_alloca(t_ExtensionCount * sizeof(VkExtensionProperties));
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
	VkLayerProperties* t_Layers = (VkLayerProperties*)_alloca(t_LayerCount * sizeof(VkLayerProperties));
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
		
		VkPhysicalDeviceDescriptorIndexingFeatures t_IndexingFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT };
		VkPhysicalDeviceTimelineSemaphoreFeatures t_SemFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES };
		t_SemFeatures.pNext = &t_IndexingFeatures;
		VkPhysicalDeviceSynchronization2Features t_SyncFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
		t_SyncFeatures.pNext = &t_SemFeatures;
		VkPhysicalDeviceFeatures2 t_DeviceFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		t_DeviceFeatures.pNext = &t_SyncFeatures;
		vkGetPhysicalDeviceFeatures2(t_PhysicalDevices[i], &t_DeviceFeatures);

		SwapchainSupportDetails t_SwapChainDetails = QuerySwapChainSupport(a_Surface, t_PhysicalDevices[i]);

		if (t_DeviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			t_SemFeatures.timelineSemaphore == VK_TRUE &&
			t_SyncFeatures.synchronization2 == VK_TRUE &&
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

	VkPhysicalDeviceBufferDeviceAddressFeatures t_AddressFeature{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
	t_AddressFeature.bufferDeviceAddress = VK_TRUE;
	t_AddressFeature.pNext = &t_IndexingFeatures;

	VkPhysicalDeviceDescriptorBufferFeaturesEXT  t_DescriptorBufferInfo{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
	t_DescriptorBufferInfo.descriptorBuffer = VK_TRUE;
	t_DescriptorBufferInfo.pNext = &t_AddressFeature;

	VkPhysicalDeviceSynchronization2Features t_SyncFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
	t_SyncFeatures.synchronization2 = VK_TRUE;
	t_SyncFeatures.pNext = &t_DescriptorBufferInfo;

	VkDeviceCreateInfo t_CreateInfo{};
	t_CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	t_CreateInfo.pQueueCreateInfos = t_QueueCreateInfos;
	t_CreateInfo.queueCreateInfoCount = t_DifferentQueues;
	t_CreateInfo.pEnabledFeatures = &t_DeviceFeatures;
	t_CreateInfo.ppEnabledExtensionNames = a_DeviceExtensions.data();
	t_CreateInfo.enabledExtensionCount = static_cast<uint32_t>(a_DeviceExtensions.size());
	t_CreateInfo.pNext = &t_SyncFeatures;

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
	//Returns some info to the global backend that is important.
	BackendInfo t_BackendInfo;

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
		VkApplicationInfo t_AppInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
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

		VulkanLoadFunctions(s_VKB.instance);
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

	{
		VkPhysicalDeviceDescriptorBufferPropertiesEXT t_DescBufferInfo{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT, nullptr };
		VkPhysicalDeviceProperties2 t_DeviceProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		t_DeviceProperties.pNext = &t_DescBufferInfo;

		vkGetPhysicalDeviceProperties2(s_VKB.physicalDevice, &t_DeviceProperties);

		s_DescriptorTypeSize[static_cast<uint32_t>(RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT)] = static_cast<uint32_t>(t_DescBufferInfo.uniformBufferDescriptorSize);
		s_DescriptorTypeSize[static_cast<uint32_t>(RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER)] = static_cast<uint32_t>(t_DescBufferInfo.storageBufferDescriptorSize);
		s_DescriptorTypeSize[static_cast<uint32_t>(RENDER_DESCRIPTOR_TYPE::READWRITE)] = static_cast<uint32_t>(t_DescBufferInfo.storageBufferDescriptorSize);
		s_DescriptorTypeSize[static_cast<uint32_t>(RENDER_DESCRIPTOR_TYPE::IMAGE)] = static_cast<uint32_t>(t_DescBufferInfo.sampledImageDescriptorSize);
	
		uint32_t t_BiggestDescriptorType = s_DescriptorTypeSize[0];
		for (size_t i = 1; i < static_cast<uint32_t>(RENDER_DESCRIPTOR_TYPE::ENUM_SIZE); i++)
		{
			if (s_DescriptorTypeSize[i] > t_BiggestDescriptorType)
				t_BiggestDescriptorType = s_DescriptorTypeSize[i];
		}
		s_DescriptorBiggestResourceType = t_BiggestDescriptorType;
		s_DescriptorSamplerSize = static_cast<uint32_t>(t_DescBufferInfo.samplerDescriptorSize);
		s_DescriptorBufferAlignment = static_cast<uint32_t>(t_DescBufferInfo.descriptorBufferOffsetAlignment);

		t_BackendInfo.minReadonlyConstantOffset = static_cast<uint32_t>(t_DeviceProperties.properties.limits.minUniformBufferOffsetAlignment);
		t_BackendInfo.minReadonlyBufferOffset = static_cast<uint32_t>(t_DeviceProperties.properties.limits.minStorageBufferOffsetAlignment);
		t_BackendInfo.minReadWriteBufferOffset = static_cast<uint32_t>(t_DeviceProperties.properties.limits.minStorageBufferOffsetAlignment);
	}

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
	t_AllocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	
	vmaCreateAllocator(&t_AllocatorCreateInfo, &s_VKB.vma);

	//Returns some info to the global backend that is important.
	t_BackendInfo.currentFrame = s_VKB.currentFrame;
	t_BackendInfo.framebufferCount = s_VKB.frameCount;

	return t_BackendInfo;
}

RDescriptorHeap BB::VulkanCreateDescriptorHeap(const DescriptorHeapCreateInfo& a_CreateInfo, const bool a_GpuVisible)
{
	VkBufferUsageFlags t_BufferUsage;
	uint32_t t_BufferSize;

	if (a_CreateInfo.isSampler)
	{
		t_BufferUsage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		t_BufferSize = a_CreateInfo.descriptorCount * s_DescriptorSamplerSize;
	}
	else
	{
		t_BufferUsage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		t_BufferSize = a_CreateInfo.descriptorCount * s_DescriptorBiggestResourceType;
	}

	return (uintptr_t)BBnew(s_VulkanAllocator, VulkanDescriptorBuffer)(t_BufferSize, t_BufferUsage, a_GpuVisible, a_CreateInfo.name);
}

RDescriptor BB::VulkanCreateDescriptor(const RenderDescriptorCreateInfo& a_CreateInfo)
{
	bool bindlessSet = false;

	VulkanDescriptor t_ReturnDesc{};
	
	{
		VkDescriptorSetLayoutBinding* t_LayoutBinds = BBnewArr(
			s_VulkanTempAllocator,
			a_CreateInfo.bindings.size(),
			VkDescriptorSetLayoutBinding);

		VkDescriptorBindingFlags* t_BindlessFlags = BBnewArr(
			s_VulkanTempAllocator,
			a_CreateInfo.bindings.size(),
			VkDescriptorBindingFlags);

		for (size_t i = 0; i < a_CreateInfo.bindings.size(); i++)
		{
			const DescriptorBinding& t_Binding = a_CreateInfo.bindings[i];
			t_LayoutBinds[i].binding = t_Binding.binding;
			t_LayoutBinds[i].descriptorCount = t_Binding.descriptorCount;
			t_LayoutBinds[i].descriptorType = VKConv::DescriptorBufferType(t_Binding.type);
			t_LayoutBinds[i].stageFlags = VKConv::ShaderStageBits(t_Binding.stage);

			t_ReturnDesc.descriptorCount += t_Binding.descriptorCount;
			if (t_Binding.descriptorCount > 1)
			{
				t_BindlessFlags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
					VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
				bindlessSet = true;
			}
			else
				t_BindlessFlags[i] = 0;
		}

		VkDescriptorSetLayoutCreateInfo t_LayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		t_LayoutInfo.pBindings = t_LayoutBinds;
		t_LayoutInfo.bindingCount = static_cast<uint32_t>(a_CreateInfo.bindings.size());
		t_LayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

		if (bindlessSet) //if bindless add another struct and return here.
		{
			VkDescriptorSetLayoutBindingFlagsCreateInfo t_LayoutExtInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT };
			t_LayoutExtInfo.bindingCount = static_cast<uint32_t>(a_CreateInfo.bindings.size());
			t_LayoutExtInfo.pBindingFlags = t_BindlessFlags;

			t_LayoutInfo.pNext = &t_LayoutExtInfo;

			//Do some algorithm to see if I already made a descriptorlayout like this one.
			VKASSERT(vkCreateDescriptorSetLayout(s_VKB.device,
				&t_LayoutInfo, nullptr, &t_ReturnDesc.layout),
				"Vulkan: Failed to create a descriptorsetlayout.");
		}
		else
		{
			//Do some algorithm to see if I already made a descriptorlayout like this one.
			VKASSERT(vkCreateDescriptorSetLayout(s_VKB.device,
				&t_LayoutInfo, nullptr, &t_ReturnDesc.layout),
				"Vulkan: Failed to create a descriptorsetlayout.");
		}
	}

	VulkanDescriptor* t_DescLayout = BBnew(s_VulkanAllocator, VulkanDescriptor)(t_ReturnDesc);
	return RDescriptor((uintptr_t)t_DescLayout);
}

CommandQueueHandle BB::VulkanCreateCommandQueue(const RenderCommandQueueCreateInfo& a_CreateInfo)
{
	uint32_t t_QueueIndex;

	switch (a_CreateInfo.queue)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		t_QueueIndex = s_VKB.queueIndices.graphics;
		break;
	case RENDER_QUEUE_TYPE::TRANSFER:
		t_QueueIndex = s_VKB.queueIndices.transfer;
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
		t_QueueIndex = s_VKB.queueIndices.compute;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Trying to get a device queue that you didn't setup yet.");
		break;
	}
	VkQueue t_Queue;
	vkGetDeviceQueue(s_VKB.device,
		t_QueueIndex,
		0,
		&t_Queue);

	SetDebugName(a_CreateInfo.name, t_Queue, VK_OBJECT_TYPE_QUEUE);

	return CommandQueueHandle((uintptr_t)t_Queue);
}

CommandAllocatorHandle BB::VulkanCreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo)
{
	VkCommandAllocator* t_CmdAllocator = s_VKB.cmdAllocators.Get();

	VkCommandPoolCreateInfo t_CreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	switch (a_CreateInfo.queueType)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		t_CreateInfo.queueFamilyIndex = s_VKB.queueIndices.graphics;
		break;
	case RENDER_QUEUE_TYPE::TRANSFER:
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

	VkCommandBufferAllocateInfo t_AllocCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	t_AllocCreateInfo.commandPool = t_CmdAllocator->pool;
	t_AllocCreateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	t_AllocCreateInfo.commandBufferCount = a_CreateInfo.commandListCount;

	VKASSERT(vkAllocateCommandBuffers(s_VKB.device,
		&t_AllocCreateInfo,
		t_CmdAllocator->buffers.data()),
		"Vulkan: Failed to allocate command buffers!");

	SetDebugName(a_CreateInfo.name, t_CmdAllocator->pool, VK_OBJECT_TYPE_COMMAND_POOL);
	return (uintptr_t)t_CmdAllocator; //Creates a handle from this.
}

CommandListHandle BB::VulkanCreateCommandList(const RenderCommandListCreateInfo& a_CreateInfo)
{
	BB_ASSERT(a_CreateInfo.commandAllocator.handle != NULL, "Sending a commandallocator handle that is null!");
	VulkanCommandList t_List = reinterpret_cast<VkCommandAllocator*>(a_CreateInfo.commandAllocator.ptrHandle)->GetCommandList();

	SetDebugName(a_CreateInfo.name, t_List.Buffer(), VK_OBJECT_TYPE_COMMAND_BUFFER);
	return CommandListHandle(s_VKB.commandLists.insert(t_List).handle);
}

RBufferHandle BB::VulkanCreateBuffer(const RenderBufferCreateInfo& a_CreateInfo)
{
	VulkanBuffer* t_Buffer = s_VKB.bufferPool.Get();

	VkBufferCreateInfo t_BufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	t_BufferInfo.size = a_CreateInfo.size;
	t_BufferInfo.usage = VKConv::RenderBufferUsage(a_CreateInfo.usage);
	t_BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo t_VmaAlloc{};
	t_VmaAlloc.usage = MemoryPropertyFlags(a_CreateInfo.memProperties);
	if (t_VmaAlloc.usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST)
		t_VmaAlloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	VKASSERT(vmaCreateBuffer(s_VKB.vma,
		&t_BufferInfo, &t_VmaAlloc,
		&t_Buffer->buffer, &t_Buffer->allocation,
		nullptr), "Vulkan::VMA, Failed to allocate memory");

	SetDebugName(a_CreateInfo.name, t_Buffer->buffer, VK_OBJECT_TYPE_BUFFER);

	return RBufferHandle((uintptr_t)t_Buffer);
}

constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D24_UNORM_S8_UINT;

RImageHandle BB::VulkanCreateImage(const RenderImageCreateInfo& a_CreateInfo)
{
	VulkanImage* t_Image = s_VKB.imagePool.Get();

	VkImageCreateInfo t_ImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

	VkImageViewCreateInfo t_ViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
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


	SetDebugName(a_CreateInfo.name, t_Image->image, VK_OBJECT_TYPE_IMAGE);
	SetDebugName(a_CreateInfo.name, t_Image->view, VK_OBJECT_TYPE_IMAGE_VIEW);

	return RImageHandle((uintptr_t)t_Image);
}

RSamplerHandle BB::VulkanCreateSampler(const SamplerCreateInfo& a_CreateInfo)
{
	return RSamplerHandle((uintptr_t)CreateSampler(a_CreateInfo));
}

RFenceHandle BB::VulkanCreateFence(const FenceCreateInfo& a_CreateInfo)
{
	VkSemaphoreTypeCreateInfo t_TimelineSemInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
	t_TimelineSemInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	t_TimelineSemInfo.initialValue = a_CreateInfo.initialValue;

	VkSemaphoreCreateInfo t_SemCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	t_SemCreateInfo.pNext = &t_TimelineSemInfo;

	VkSemaphore t_TimelineSemaphore;
	vkCreateSemaphore(s_VKB.device,
		&t_SemCreateInfo,
		nullptr,
		&t_TimelineSemaphore);

	SetDebugName(a_CreateInfo.name, t_TimelineSemaphore, VK_OBJECT_TYPE_SEMAPHORE);

	return RFenceHandle((uintptr_t)t_TimelineSemaphore);
}

DescriptorAllocation BB::VulkanAllocateDescriptor(const AllocateDescriptorInfo& a_AllocateInfo)
{
	return reinterpret_cast<VulkanDescriptorBuffer*>(a_AllocateInfo.heap.handle)->Allocate(a_AllocateInfo.descriptor, a_AllocateInfo.heapOffset);
}

void BB::VulkanCopyDescriptors(const CopyDescriptorsInfo& a_CopyInfo)
{
	VulkanDescriptorBuffer* t_SrcHeap = reinterpret_cast<VulkanDescriptorBuffer*>(a_CopyInfo.srcHeap.handle);
	VulkanDescriptorBuffer* t_DstHeap = reinterpret_cast<VulkanDescriptorBuffer*>(a_CopyInfo.dstHeap.handle);
	BB_ASSERT(t_SrcHeap->GetBuffer() == VK_NULL_HANDLE, "Trying to copy descriptors but the source is a GPU visible heap!");
	size_t t_DescriptorSize;
	if (a_CopyInfo.isSamplerHeap)
		t_DescriptorSize = s_DescriptorSamplerSize;
	else
		t_DescriptorSize = s_DescriptorBiggestResourceType;

	memcpy(Pointer::Add(t_DstHeap->GetStartOfBuffer(), a_CopyInfo.dstOffset * t_DescriptorSize),
		Pointer::Add(t_SrcHeap->GetStartOfBuffer(), a_CopyInfo.srcOffset * t_DescriptorSize),
		t_DescriptorSize * a_CopyInfo.descriptorCount);
}

static inline VkDescriptorAddressInfoEXT GetDescriptorAddressInfo(const WriteDescriptorBuffer& a_Buffer, const VkFormat a_Format = VK_FORMAT_UNDEFINED)
{
	VkDescriptorAddressInfoEXT t_Info{ VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
	t_Info.range = a_Buffer.range;
	t_Info.address = GetBufferDeviceAddress(reinterpret_cast<VulkanBuffer*>(a_Buffer.buffer.handle)->buffer);
	//offset the address.
	t_Info.address += a_Buffer.offset;
	t_Info.format = a_Format;
	return t_Info;
}

static inline VkDescriptorImageInfo GetDescriptorImageInfo(const WriteDescriptorImage& a_Image)
{
	VkDescriptorImageInfo t_Info{};
	t_Info.sampler = reinterpret_cast<VkSampler>(a_Image.sampler.handle);
	const VulkanImage* t_Image = reinterpret_cast<VulkanImage*>(a_Image.image.handle);
	t_Info.imageView = t_Image->view;
	t_Info.imageLayout = VKConv::ImageLayout(a_Image.layout);
	return t_Info;
}

void BB::VulkanWriteDescriptors(const WriteDescriptorInfos& a_WriteInfo)
{
	for (size_t i = 0; i < a_WriteInfo.data.size(); i++)
	{
		const WriteDescriptorData& t_WriteData = a_WriteInfo.data[i];

		VkDeviceSize t_Offset;
		GetDescriptorSetLayoutBindingOffsetEXT(s_VKB.device,
			reinterpret_cast<VulkanDescriptor*>(a_WriteInfo.descriptorHandle.handle)->layout,
			t_WriteData.binding,
			&t_Offset);
		
		t_Offset += static_cast<size_t>(s_DescriptorTypeSize[static_cast<uint32_t>(t_WriteData.type)]) * t_WriteData.descriptorIndex;
		void* t_DescriptorLocation = Pointer::Add(a_WriteInfo.allocation.bufferStart, a_WriteInfo.allocation.offset + t_Offset);

		union VkDescData
		{
			VkDescriptorAddressInfoEXT buffer;
			VkDescriptorImageInfo image;
		};

		VkDescData t_Data{};

		VkDescriptorGetInfoEXT t_DescInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
		switch (t_WriteData.type)
		{
		case BB::RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:
			t_Data.buffer = GetDescriptorAddressInfo(t_WriteData.buffer);
			t_DescInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			t_DescInfo.data.pUniformBuffer = &t_Data.buffer;
			break;
		case BB::RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:
		case BB::RENDER_DESCRIPTOR_TYPE::READWRITE:
			t_Data.buffer = GetDescriptorAddressInfo(t_WriteData.buffer);
			t_DescInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			t_DescInfo.data.pStorageBuffer = &t_Data.buffer;
			break;
		case BB::RENDER_DESCRIPTOR_TYPE::IMAGE:
			t_Data.image = GetDescriptorImageInfo(t_WriteData.image);
			t_DescInfo.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			t_DescInfo.data.pSampledImage = &t_Data.image;
			break;
		default:
			BB_ASSERT(false, "Vulkan: RENDER_DESCRIPTOR_TYPE failed to convert to a VkDescriptorType.");
			t_DescInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			break;
		}

		const size_t t_DescriptorSize = s_DescriptorTypeSize[static_cast<uint32_t>(t_WriteData.type)];
		GetDescriptorEXT(s_VKB.device, &t_DescInfo, t_DescriptorSize, t_DescriptorLocation);
	}
}

ImageReturnInfo BB::VulkanGetImageInfo(const RImageHandle a_Handle)
{
	VulkanImage* t_Image = reinterpret_cast<VulkanImage*>(a_Handle.handle);

	ImageReturnInfo t_ReturnInfo{};
	t_ReturnInfo.allocInfo.imageAllocByteSize = static_cast<uint64_t>(
		t_Image->width) *
		t_Image->height *
		4 * //4 is the amount of channels it has.
		t_Image->depth *
		t_Image->arrays *
		t_Image->mips;
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

	t_BuildInfo->name = a_InitInfo.name;

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
			VkPipelineDepthStencilStateCreateInfo) {};
		t_DepthCreateInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		if (a_InitInfo.enableDepthTest)
		{
			t_DepthCreateInfo->depthTestEnable = VK_TRUE;
			t_DepthCreateInfo->depthWriteEnable = VK_TRUE;
			t_DepthCreateInfo->depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			t_DepthCreateInfo->back.compareMask = VK_COMPARE_OP_ALWAYS;
			t_DepthCreateInfo->depthBoundsTestEnable = VK_FALSE;
			t_DepthCreateInfo->minDepthBounds = 0.0f;
			t_DepthCreateInfo->maxDepthBounds = 0.0f;
			t_DepthCreateInfo->stencilTestEnable = VK_FALSE;
			t_BuildInfo->dynamicRenderingInfo.depthAttachmentFormat = DEPTH_FORMAT;
			t_BuildInfo->dynamicRenderingInfo.stencilAttachmentFormat = DEPTH_FORMAT;
		}

		t_BuildInfo->pipeInfo.pDepthStencilState = t_DepthCreateInfo;
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

	//if we have static samplers, init a special layout for this. Jank but whatever vulkan lmao
	if (a_InitInfo.immutableSamplers.size() != 0)
	{
		VkDescriptorSetLayoutBinding* t_LayoutBinds = BBnewArr(t_BuildInfo->buildAllocator, 
			a_InitInfo.immutableSamplers.size(), VkDescriptorSetLayoutBinding);

		for (size_t i = 0; i < a_InitInfo.immutableSamplers.size(); i++)
		{
			VkSampler* t_Sampler = BBnew(t_BuildInfo->buildAllocator, VkSampler);
			*t_Sampler = CreateSampler(a_InitInfo.immutableSamplers[i]);
			t_LayoutBinds[i].binding = static_cast<uint32_t>(i);
			t_LayoutBinds[i].descriptorCount = 1;
			t_LayoutBinds[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			t_LayoutBinds[i].pImmutableSamplers = t_Sampler;
			t_LayoutBinds[i].stageFlags = VK_SHADER_STAGE_ALL;
		}
		VkDescriptorSetLayoutCreateInfo t_LayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		t_LayoutInfo.pBindings = t_LayoutBinds;
		t_LayoutInfo.bindingCount = static_cast<uint32_t>(a_InitInfo.immutableSamplers.size());
		t_LayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT | VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

		//Do some algorithm to see if I already made a descriptorlayout like this one.
		VKASSERT(vkCreateDescriptorSetLayout(s_VKB.device,
			&t_LayoutInfo, nullptr, &t_BuildInfo->layout[t_BuildInfo->layoutCount++]),
			"Vulkan: Failed to create a descriptorsetlayout for immutable samplers in pipeline init.");
	}
	else
		BB_ASSERT(false, "No sampler attached to a pipeline! Not supported for now as layout set 0 is sampler set.");

	return PipelineBuilderHandle((uintptr_t)t_BuildInfo);
}

void BB::VulkanPipelineBuilderBindDescriptor(const PipelineBuilderHandle a_Handle, const RDescriptor a_Descriptor)
{
	constexpr uint32_t MAX_PUSHCONSTANTSIZE = 128;
	VKPipelineBuildInfo* t_BuildInfo = reinterpret_cast<VKPipelineBuildInfo*>(a_Handle.ptrHandle);
	t_BuildInfo->layout[t_BuildInfo->layoutCount++] = reinterpret_cast<VulkanDescriptor*>(a_Descriptor.handle)->layout;
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
		VkPipelineDynamicStateCreateInfo t_DynamicPipeCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
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
		t_BuildInfo->pipeInfo.pInputAssemblyState = &t_InputAssembly;

		if (t_BuildInfo->pipeInfo.pVertexInputState == nullptr)
		{
			VkPipelineVertexInputStateCreateInfo* t_VertexInputInfo = BBnew(
				t_BuildInfo->buildAllocator,
				VkPipelineVertexInputStateCreateInfo) {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
			t_VertexInputInfo->vertexBindingDescriptionCount = 0;
			t_VertexInputInfo->vertexAttributeDescriptionCount = 0;
			t_BuildInfo->pipeInfo.pVertexInputState = t_VertexInputInfo;
		}


		VkPipelineMultisampleStateCreateInfo t_Multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		t_Multisampling.sampleShadingEnable = VK_FALSE;
		t_Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		t_Multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		t_Multisampling.alphaToOneEnable = VK_FALSE; // Optional
		t_Multisampling.minSampleShading = 1.0f; // Optional
		t_Multisampling.pSampleMask = nullptr; // Optional

		//viewport is always controlled by the dynamic state so we just initialize them here.
		t_BuildInfo->pipeInfo.pViewportState = &t_ViewportState;
		t_BuildInfo->pipeInfo.pDynamicState = &t_DynamicPipeCreateInfo;
		t_BuildInfo->pipeInfo.pMultisampleState = &t_Multisampling;

		//Optimalization for later.
		t_BuildInfo->pipeInfo.basePipelineHandle = VK_NULL_HANDLE;
		t_BuildInfo->pipeInfo.basePipelineIndex = -1;
		t_BuildInfo->pipeInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

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

	SetDebugName(t_BuildInfo->name, t_Pipeline.pipeline, VK_OBJECT_TYPE_PIPELINE);
	BBfree(s_VulkanAllocator, t_BuildInfo);

	return PipelineHandle((uintptr_t)t_ReturnPipeline);
}

void BB::VulkanResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle)
{
	//Wait for fence.
	VkCommandAllocator* t_CmdAllocator = reinterpret_cast<VkCommandAllocator*>(a_CmdAllocatorHandle.ptrHandle);

	vkResetCommandPool(s_VKB.device,
		t_CmdAllocator->pool,
		0);
}

void BB::VulkanStartCommandList(const CommandListHandle a_CmdHandle)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_CmdHandle.handle];

	//vkResetCommandBuffer(a_CmdList.buffers[a_CmdList.currentFree], 0);
	VkCommandBufferBeginInfo t_CmdBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VKASSERT(vkBeginCommandBuffer(t_Cmdlist.Buffer(),
		&t_CmdBeginInfo),
		"Vulkan: Failed to begin commandbuffer");
}

void BB::VulkanEndCommandList(const CommandListHandle a_RecordingCmdHandle)
{
	VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];

	t_Cmdlist.currentPipelineLayout = VK_NULL_HANDLE;

	VKASSERT(vkEndCommandBuffer(t_Cmdlist.Buffer()),
		"Vulkan: Error when trying to end commandbuffer!");
}

void BB::VulkanStartRendering(const CommandListHandle a_RecordingCmdHandle, const StartRenderingInfo& a_RenderInfo)
{
	VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];

	uint32_t t_BarrierCount = 1;
	VkImageMemoryBarrier2 t_Barriers[2]{};
	t_Barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	t_Barriers[0].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	t_Barriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	t_Barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	t_Barriers[0].oldLayout = VKConv::ImageLayout(a_RenderInfo.colorInitialLayout);
	t_Barriers[0].newLayout = VKConv::ImageLayout(a_RenderInfo.colorFinalLayout);
	t_Barriers[0].image = s_VKB.swapChain.frames[s_VKB.currentFrame].image;
	t_Barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	t_Barriers[0].subresourceRange.baseArrayLayer = 0;
	t_Barriers[0].subresourceRange.layerCount = 1;
	t_Barriers[0].subresourceRange.baseMipLevel = 0;
	t_Barriers[0].subresourceRange.levelCount = 1;

	VkRenderingInfo t_RenderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
	VkRenderingAttachmentInfo t_RenderDepthAttach{};

	//If we handle the depth stencil we do that here. 
	if (a_RenderInfo.depthStencil.ptrHandle != nullptr) 
	{
		++t_BarrierCount;
		t_Barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		t_Barriers[1].dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		t_Barriers[1].srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		t_Barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		t_Barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		t_Barriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		t_Barriers[1].image = reinterpret_cast<VulkanImage*>(a_RenderInfo.depthStencil.ptrHandle)->image;
		t_Barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		t_Barriers[1].subresourceRange.baseArrayLayer = 0;
		t_Barriers[1].subresourceRange.layerCount = 1;
		t_Barriers[1].subresourceRange.baseMipLevel = 0;
		t_Barriers[1].subresourceRange.levelCount = 1;

		t_Cmdlist.depthImage = reinterpret_cast<VulkanImage*>(a_RenderInfo.depthStencil.ptrHandle)->image;
		
		t_RenderDepthAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		t_RenderDepthAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		t_RenderDepthAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		t_RenderDepthAttach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		t_RenderDepthAttach.imageView = reinterpret_cast<VulkanImage*>(a_RenderInfo.depthStencil.ptrHandle)->view;
		t_RenderDepthAttach.clearValue.depthStencil = { 1.0f, 0 };
		t_RenderInfo.pDepthAttachment = &t_RenderDepthAttach;
		t_RenderInfo.pStencilAttachment = &t_RenderDepthAttach;
	}

	//Color and possibly a depth stencil barrier.
	VkDependencyInfo t_BarrierInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	t_BarrierInfo.imageMemoryBarrierCount = t_BarrierCount;
	t_BarrierInfo.pImageMemoryBarriers = t_Barriers;

	vkCmdPipelineBarrier2(t_Cmdlist.Buffer(), &t_BarrierInfo);

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

	t_RenderInfo.pNext = nullptr;
	t_RenderInfo.renderArea = t_Scissor;
	t_RenderInfo.layerCount = 1;
	t_RenderInfo.pColorAttachments = &t_RenderColorAttach;
	t_RenderInfo.colorAttachmentCount = 1;

	vkCmdBeginRendering(t_Cmdlist.Buffer(), &t_RenderInfo);

	VkViewport t_Viewport{};
	t_Viewport.x = 0.0f;
	t_Viewport.y = 0.0f;
	t_Viewport.width = static_cast<float>(a_RenderInfo.viewportWidth);
	t_Viewport.height = static_cast<float>(a_RenderInfo.viewportHeight);
	t_Viewport.minDepth = 0.0f;
	t_Viewport.maxDepth = 1.0f;
	vkCmdSetViewport(t_Cmdlist.Buffer(), 0, 1, &t_Viewport);


	vkCmdSetScissor(t_Cmdlist.Buffer(), 0, 1, &t_Scissor);
}

void BB::VulkanSetScissor(const CommandListHandle a_RecordingCmdHandle, const ScissorInfo& a_ScissorInfo)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];

	VkRect2D t_Scissor{};
	t_Scissor.offset.x = a_ScissorInfo.offset.x;
	t_Scissor.offset.y = a_ScissorInfo.offset.y;
	t_Scissor.extent.width = a_ScissorInfo.extent.x;
	t_Scissor.extent.height = a_ScissorInfo.extent.y;

	vkCmdSetScissor(t_Cmdlist.Buffer(), 0, 1, &t_Scissor);
}

void BB::VulkanEndRendering(const CommandListHandle a_RecordingCmdHandle, const EndRenderingInfo& a_EndInfo)
{
	VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];
	vkCmdEndRendering(t_Cmdlist.Buffer());

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

	vkCmdPipelineBarrier(t_Cmdlist.Buffer(),
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&t_PresentBarrier);

	t_Cmdlist.depthImage = VK_NULL_HANDLE;
}

void BB::VulkanCopyBuffer(const CommandListHandle a_RecordingCmdHandle, const RenderCopyBufferInfo& a_CopyInfo)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];
	VulkanBuffer* t_SrcBuffer = reinterpret_cast<VulkanBuffer*>(a_CopyInfo.src.handle);
	VulkanBuffer* t_DstBuffer = reinterpret_cast<VulkanBuffer*>(a_CopyInfo.dst.handle);

	VkBufferCopy t_CopyRegion{};
	t_CopyRegion.srcOffset = a_CopyInfo.srcOffset;
	t_CopyRegion.dstOffset = a_CopyInfo.dstOffset;
	t_CopyRegion.size = a_CopyInfo.size;

	vkCmdCopyBuffer(t_Cmdlist.Buffer(),
		t_SrcBuffer->buffer,
		t_DstBuffer->buffer,
		1,
		&t_CopyRegion);
}

void BB::VulkanCopyBufferImage(const CommandListHandle a_RecordingCmdHandle, const RenderCopyBufferImageInfo& a_CopyInfo)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];
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

	vkCmdCopyBufferToImage(t_Cmdlist.Buffer(),
		t_SrcBuffer->buffer,
		t_DstImage->image,
		VKConv::ImageLayout(a_CopyInfo.dstImageInfo.layout),
		1,
		&t_CopyRegion);
}

static uint32_t queueTransitionIndex(const RENDER_QUEUE_TRANSITION a_Transition)
{
	switch (a_Transition)
	{
	case RENDER_QUEUE_TRANSITION::GRAPHICS:
		return s_VKB.queueIndices.graphics;
		break;
	case RENDER_QUEUE_TRANSITION::TRANSFER:
		return s_VKB.queueIndices.transfer;
		break;
	case RENDER_QUEUE_TRANSITION::COMPUTE:
		return s_VKB.queueIndices.compute;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Queue transition not supported!");
		return VK_QUEUE_FAMILY_IGNORED;
		break;
	}
}

void BB::VulkanPipelineBarriers(const CommandListHandle a_RecordingCmdHandle, const PipelineBarrierInfo& a_BarrierInfo)
{
	VkMemoryBarrier2* t_GlobalBarriers = reinterpret_cast<VkMemoryBarrier2*>(
		_alloca(a_BarrierInfo.globalInfoCount * sizeof(VkMemoryBarrier2)));
	VkBufferMemoryBarrier2* t_BufferBarriers = reinterpret_cast<VkBufferMemoryBarrier2*>(
		_alloca(a_BarrierInfo.bufferInfoCount * sizeof(VkBufferMemoryBarrier2)));
	VkImageMemoryBarrier2* t_ImageBarriers = reinterpret_cast<VkImageMemoryBarrier2*>(
		_alloca(a_BarrierInfo.imageInfoCount * sizeof(VkImageMemoryBarrier2)));

	for (size_t i = 0; i < a_BarrierInfo.globalInfoCount; i++)
	{
		const PipelineBarrierGlobalInfo& t_BarrierInfo = a_BarrierInfo.globalInfos[i];

		t_GlobalBarriers[i].sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
		t_GlobalBarriers[i].pNext = nullptr;
		t_GlobalBarriers[i].srcAccessMask = VKConv::AccessMask(t_BarrierInfo.srcMask);
		t_GlobalBarriers[i].dstAccessMask = VKConv::AccessMask(t_BarrierInfo.dstMask);
		t_GlobalBarriers[i].srcStageMask = VKConv::PipelineStage(t_BarrierInfo.srcStage);
		t_GlobalBarriers[i].dstStageMask = VKConv::PipelineStage(t_BarrierInfo.dstStage);
	}

	for (size_t i = 0; i < a_BarrierInfo.bufferInfoCount; i++)
	{
		const PipelineBarrierBufferInfo& t_BarrierInfo = a_BarrierInfo.bufferInfos[i];

		t_BufferBarriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		t_BufferBarriers[i].pNext = nullptr;
		t_BufferBarriers[i].srcAccessMask = VKConv::AccessMask(t_BarrierInfo.srcMask);
		t_BufferBarriers[i].dstAccessMask = VKConv::AccessMask(t_BarrierInfo.dstMask);
		t_BufferBarriers[i].srcStageMask = VKConv::PipelineStage(t_BarrierInfo.srcStage);
		t_BufferBarriers[i].dstStageMask = VKConv::PipelineStage(t_BarrierInfo.dstStage);
		//if we do no transition on the source queue. Then set it all to false.
		if (t_BarrierInfo.srcQueue == RENDER_QUEUE_TRANSITION::NO_TRANSITION)
		{
			t_BufferBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			t_BufferBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		}
		else
		{
			t_BufferBarriers[i].srcQueueFamilyIndex = queueTransitionIndex(t_BarrierInfo.srcQueue);
			t_BufferBarriers[i].dstQueueFamilyIndex = queueTransitionIndex(t_BarrierInfo.dstQueue);
		}
		t_BufferBarriers[i].buffer = reinterpret_cast<VulkanBuffer*>(t_BarrierInfo.buffer.ptrHandle)->buffer;
		t_BufferBarriers[i].offset = t_BarrierInfo.offset;
		t_BufferBarriers[i].size = t_BarrierInfo.size;
	}

	for (size_t i = 0; i < a_BarrierInfo.imageInfoCount; i++)
	{
		const PipelineBarrierImageInfo& t_BarrierInfo = a_BarrierInfo.imageInfos[i];

		t_ImageBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		t_ImageBarriers[i].pNext = nullptr;
		t_ImageBarriers[i].srcAccessMask = VKConv::AccessMask(t_BarrierInfo.srcMask);
		t_ImageBarriers[i].dstAccessMask = VKConv::AccessMask(t_BarrierInfo.dstMask);
		t_ImageBarriers[i].srcStageMask = VKConv::PipelineStage(t_BarrierInfo.srcStage);
		t_ImageBarriers[i].dstStageMask = VKConv::PipelineStage(t_BarrierInfo.dstStage);
		//if we do no transition on the source queue. Then set it all to false.
		if (t_BarrierInfo.srcQueue == RENDER_QUEUE_TRANSITION::NO_TRANSITION)
		{
			t_ImageBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			t_ImageBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		}
		else
		{
			t_ImageBarriers[i].srcQueueFamilyIndex = queueTransitionIndex(t_BarrierInfo.srcQueue);
			t_ImageBarriers[i].dstQueueFamilyIndex = queueTransitionIndex(t_BarrierInfo.dstQueue);
		}
		t_ImageBarriers[i].oldLayout = VKConv::ImageLayout(t_BarrierInfo.oldLayout);
		t_ImageBarriers[i].newLayout = VKConv::ImageLayout(t_BarrierInfo.newLayout);
		t_ImageBarriers[i].image = reinterpret_cast<VulkanImage*>(t_BarrierInfo.image.ptrHandle)->image;
		if (t_BarrierInfo.newLayout == RENDER_IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT ||
			t_BarrierInfo.oldLayout == RENDER_IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT)
			t_ImageBarriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		else
			t_ImageBarriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		t_ImageBarriers[i].subresourceRange.baseMipLevel = t_BarrierInfo.baseMipLevel;
		t_ImageBarriers[i].subresourceRange.levelCount = t_BarrierInfo.levelCount;
		t_ImageBarriers[i].subresourceRange.baseArrayLayer = t_BarrierInfo.baseArrayLayer;
		t_ImageBarriers[i].subresourceRange.layerCount = t_BarrierInfo.layerCount;
	}

	VkDependencyInfo t_BarrierInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	t_BarrierInfo.memoryBarrierCount = a_BarrierInfo.globalInfoCount;
	t_BarrierInfo.pMemoryBarriers = t_GlobalBarriers;
	t_BarrierInfo.bufferMemoryBarrierCount = a_BarrierInfo.bufferInfoCount;
	t_BarrierInfo.pBufferMemoryBarriers = t_BufferBarriers;
	t_BarrierInfo.imageMemoryBarrierCount = a_BarrierInfo.imageInfoCount;
	t_BarrierInfo.pImageMemoryBarriers = t_ImageBarriers;

	vkCmdPipelineBarrier2(s_VKB.commandLists[a_RecordingCmdHandle.handle].Buffer(),
		&t_BarrierInfo);
}

void BB::VulkanBindDescriptorHeaps(const CommandListHandle a_RecordingCmdHandle, const RDescriptorHeap a_ResourceHeap, const RDescriptorHeap a_SamplerHeap)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];
	const VulkanDescriptorBuffer* t_DescBuffer = reinterpret_cast<VulkanDescriptorBuffer*>(a_ResourceHeap.handle);

	uint32_t t_HeapCount = 1;
	VkDescriptorBufferBindingInfoEXT t_BindingInfos[2];
	t_BindingInfos[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
	t_BindingInfos[0].pNext = nullptr;
	t_BindingInfos[0].usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
	t_BindingInfos[0].address = t_DescBuffer->GetBaseAddress();

	if (a_SamplerHeap.handle != 0) //sampler heap is optional
	{
		t_DescBuffer = reinterpret_cast<VulkanDescriptorBuffer*>(a_SamplerHeap.handle);
		t_BindingInfos[1].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
		t_BindingInfos[1].pNext = nullptr;
		t_BindingInfos[1].usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
		t_BindingInfos[1].address = t_DescBuffer->GetBaseAddress();
		++t_HeapCount;
	}

	CmdBindDescriptorBuffersEXT(t_Cmdlist.Buffer(), t_HeapCount, t_BindingInfos);
}

void BB::VulkanBindPipeline(const CommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline)
{
	VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];

	const VulkanPipeline* t_Pipeline = reinterpret_cast<VulkanPipeline*>(a_Pipeline.handle);

	vkCmdBindPipeline(t_Cmdlist.Buffer(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		t_Pipeline->pipeline);

	CmdBindDescriptorBufferEmbeddedSamplersEXT(t_Cmdlist.Buffer(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		t_Pipeline->layout,
		0);

	t_Cmdlist.currentPipelineLayout = t_Pipeline->layout;
}

void BB::VulkanSetDescriptorHeapOffsets(const CommandListHandle a_RecordingCmdHandle, const RENDER_DESCRIPTOR_SET a_FirstSet, const uint32_t a_SetCount, const uint32_t* a_HeapIndex, const size_t* a_Offsets)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];
	CmdSetDescriptorBufferOffsetsEXT(t_Cmdlist.Buffer(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		t_Cmdlist.currentPipelineLayout,
		static_cast<const uint32_t>(a_FirstSet) + 1,
		a_SetCount,
		a_HeapIndex,
		a_Offsets);
}

void BB::VulkanBindVertexBuffers(const CommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];

	//quick cheat.
	VkBuffer t_Buffers[12]{};
	for (size_t i = 0; i < a_BufferCount; i++)
	{
		t_Buffers[i] = reinterpret_cast<VulkanBuffer*>(a_Buffers[i].ptrHandle)->buffer;
	}

	vkCmdBindVertexBuffers(t_Cmdlist.Buffer(),
		0,
		static_cast<uint32_t>(a_BufferCount),
		t_Buffers,
		a_BufferOffsets);
}

void BB::VulkanBindIndexBuffer(const CommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];

	vkCmdBindIndexBuffer(t_Cmdlist.Buffer(),
		reinterpret_cast<VulkanBuffer*>(a_Buffer.ptrHandle)->buffer,
		a_Offset,
		VK_INDEX_TYPE_UINT32);
}

void BB::VulkanBindConstant(const CommandListHandle a_RecordingCmdHandle, const uint32_t a_ConstantIndex, const uint32_t a_DwordCount, const uint32_t a_DwordOffset, const void* a_Data)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];
	
	vkCmdPushConstants(t_Cmdlist.Buffer(),
		t_Cmdlist.currentPipelineLayout,
		VK_SHADER_STAGE_ALL,
		a_DwordOffset * sizeof(uint32_t),
		a_DwordCount * sizeof(uint32_t), //we do Dword count to help dx12 more.
		a_Data);
}

void BB::VulkanDrawVertex(const CommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];

	vkCmdDraw(t_Cmdlist.Buffer(), a_VertexCount, a_InstanceCount, a_FirstVertex, a_FirstInstance);
}

void BB::VulkanDrawIndexed(const CommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance)
{
	const VulkanCommandList& t_Cmdlist = s_VKB.commandLists[a_RecordingCmdHandle.handle];

	vkCmdDrawIndexed(t_Cmdlist.Buffer(), a_IndexCount, a_InstanceCount, a_FirstIndex, a_VertexOffset, a_FirstInstance);
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
	VkSemaphoreWaitInfo t_WaitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
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

		const uint32_t t_WaitSemCount = a_ExecuteInfos[i].waitCount;
		const uint32_t t_SignalSemCount = a_ExecuteInfos[i].signalCount;
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
			t_Semaphores[j] = reinterpret_cast<VkSemaphore>(a_ExecuteInfos[i].waitFences[j].ptrHandle);
			t_SemValues[j] = a_ExecuteInfos[i].waitValues[j];

			t_WaitStagesMask[j] = VKConv::PipelineStage(a_ExecuteInfos[i].waitStages[j]);
		}

		//SETTING THE SIGNAL
		for (uint32_t j = 0; j < t_SignalSemCount; j++)
		{
			t_Semaphores[j + t_WaitSemCount] = reinterpret_cast<VkSemaphore>(a_ExecuteInfos[i].signalFences[j].ptrHandle);
			//Increment the next sem value for signal
			t_SemValues[j + t_WaitSemCount] = a_ExecuteInfos[i].signalValues[j];
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

	VkQueue t_Queue = reinterpret_cast<VkQueue>(a_ExecuteQueue.ptrHandle);
	VKASSERT(vkQueueSubmit(t_Queue,
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
	const uint32_t t_WaitSemCount = a_ExecuteInfo.waitCount + 1;
	//add 1 more to signal the binary semaphore for image presenting
	//Add 1 additional more to signal if the rendering of this frame is complete. Hacky and not totally accurate however. Might use the queue values for it later.
	const uint32_t t_SignalSemCount = a_ExecuteInfo.signalCount + 2;

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
		t_Semaphores[i + 1] = reinterpret_cast<VkSemaphore>(a_ExecuteInfo.waitFences[i].ptrHandle);
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
		t_Semaphores[t_WaitSemCount + i + 1] = reinterpret_cast<VkSemaphore>(a_ExecuteInfo.signalFences[i].ptrHandle);
		//Increment the next sem value for signal
		t_SemValues[t_WaitSemCount + i + 1] = a_ExecuteInfo.signalValues[i];
	}

	VkTimelineSemaphoreSubmitInfo t_TimelineInfo{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	t_TimelineInfo.waitSemaphoreValueCount = t_WaitSemCount;
	t_TimelineInfo.pWaitSemaphoreValues = t_SemValues;
	t_TimelineInfo.signalSemaphoreValueCount = t_SignalSemCount;
	t_TimelineInfo.pSignalSemaphoreValues = &t_SemValues[t_WaitSemCount];
	t_TimelineInfo.pNext = nullptr;

	VkSubmitInfo t_SubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	t_SubmitInfo.waitSemaphoreCount = t_WaitSemCount;
	t_SubmitInfo.pWaitSemaphores = t_Semaphores;
	t_SubmitInfo.pWaitDstStageMask = t_WaitStagesMask;
	t_SubmitInfo.signalSemaphoreCount = t_SignalSemCount;
	t_SubmitInfo.pSignalSemaphores = &t_Semaphores[t_WaitSemCount]; //Get the semaphores after all the wait sems
	t_SubmitInfo.commandBufferCount = a_ExecuteInfo.commandCount;
	t_SubmitInfo.pCommandBuffers = t_CmdBuffers;
	t_SubmitInfo.pNext = &t_TimelineInfo;

	VkQueue t_Queue = reinterpret_cast<VkQueue>(a_ExecuteQueue.ptrHandle);
	VKASSERT(vkQueueSubmit(t_Queue,
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

void BB::VulkanWaitCommands(const RenderWaitCommandsInfo& a_WaitInfo)
{
	VkSemaphoreWaitInfo t_WaitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	t_WaitInfo.semaphoreCount = a_WaitInfo.waitCount;
	t_WaitInfo.pSemaphores = reinterpret_cast<const VkSemaphore*>(a_WaitInfo.waitFences);
	t_WaitInfo.pValues = a_WaitInfo.waitValues;

	vkWaitSemaphores(s_VKB.device, &t_WaitInfo, 1000000000);
}

void BB::VulkanDestroyFence(const RFenceHandle a_Handle)
{
	VkSemaphore t_Semaphore = reinterpret_cast<VkSemaphore>(a_Handle.ptrHandle);
	vkDestroySemaphore(s_VKB.device,
		t_Semaphore,
		nullptr);
	memset(t_Semaphore, 0, sizeof(t_Semaphore));
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
	memset(t_Image, 0, sizeof(VulkanImage));
	s_VKB.imagePool.Free(t_Image);
}

void BB::VulkanDestroyBuffer(RBufferHandle a_Handle)
{
	VulkanBuffer* t_Buffer = reinterpret_cast<VulkanBuffer*>(a_Handle.ptrHandle);
	vmaDestroyBuffer(s_VKB.vma, t_Buffer->buffer, t_Buffer->allocation);
	memset(t_Buffer, 0, sizeof(VulkanBuffer));
	s_VKB.bufferPool.Free(t_Buffer);
}

void BB::VulkanDestroyCommandQueue(const CommandQueueHandle a_Handle)
{
	//nothing to delete here.
}

void BB::VulkanDestroyCommandAllocator(const CommandAllocatorHandle a_Handle)
{
	VkCommandAllocator* t_CmdAllocator = reinterpret_cast<VkCommandAllocator*>(a_Handle.ptrHandle);
	t_CmdAllocator->buffers.DestroyPool(s_VulkanAllocator);
	vkDestroyCommandPool(s_VKB.device, t_CmdAllocator->pool, nullptr);
	memset(t_CmdAllocator, 0, sizeof(VkCommandAllocator));
	s_VKB.cmdAllocators.Free(t_CmdAllocator);
}

void BB::VulkanDestroyCommandList(const CommandListHandle a_Handle)
{
	VulkanCommandList& t_List = s_VKB.commandLists[a_Handle.handle];
	t_List.cmdAllocator->FreeCommandList(t_List); //Place back in the freelist.
	memset(&t_List, 0, sizeof(VulkanCommandList));
	s_VKB.commandLists.erase(a_Handle.handle);
}

void BB::VulkanDestroyDescriptorHeap(const RDescriptorHeap a_Handle)
{
	BBfree(s_VulkanAllocator, reinterpret_cast<VulkanDescriptorBuffer*>(a_Handle.handle));
}

void BB::VulkanDestroyDescriptor(const RDescriptor a_Handle)
{
	BBfree(s_VulkanAllocator, a_Handle.ptrHandle);
	//refcount a descriptor layout then delete it when we do not use it
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
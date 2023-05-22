#include "VulkanHelperTypes.h"


using namespace BB;


VulkanQueueDeviceInfo BB::FindQueueIndex(VkQueueFamilyProperties* a_QueueProperties, uint32_t a_FamilyPropertyCount, VkQueueFlags a_QueueFlags)
{
	VulkanQueueDeviceInfo t_ReturnInfo{};

	//Find specialized compute queue.
	if ((a_QueueFlags & VK_QUEUE_COMPUTE_BIT) == a_QueueFlags)
	{
		for (uint32_t i = 0; i < a_FamilyPropertyCount; i++)
		{
			if ((a_QueueProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
				((a_QueueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
				((a_QueueProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) == 0))
			{
				t_ReturnInfo.index = i;
				t_ReturnInfo.queueCount = a_QueueProperties[i].queueCount;
				return t_ReturnInfo;
			}
		}
	}

	//Find specialized transfer queue.
	if ((a_QueueFlags & VK_QUEUE_TRANSFER_BIT) == a_QueueFlags)
	{
		for (uint32_t i = 0; i < a_FamilyPropertyCount; i++)
		{
			if ((a_QueueProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
				((a_QueueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
				((a_QueueProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				t_ReturnInfo.index = i;
				t_ReturnInfo.queueCount = a_QueueProperties[i].queueCount;
				return t_ReturnInfo;
			}
		}
	}

	//If we didn't find a specialized transfer/compute queue or want a differen queue then get the first we find.
	for (uint32_t i = 0; i < a_FamilyPropertyCount; i++)
	{
		if ((a_QueueProperties[i].queueFlags & a_QueueFlags) == a_QueueFlags)
		{
			t_ReturnInfo.index = i;
			t_ReturnInfo.queueCount = a_QueueProperties[i].queueCount;
			return t_ReturnInfo;
		}
	}

	BB_ASSERT(false, "Vulkan: Failed to find required queue.");
	return t_ReturnInfo;
}


DescriptorHeap::DescriptorHeap(const VkBufferUsageFlags a_HeapType, const uint32_t a_BufferSize)
	:	m_BufferSize(a_BufferSize)
{
	VkBufferCreateInfo t_BufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	t_BufferInfo.usage = a_HeapType | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	t_BufferInfo.size = a_BufferSize;
	t_BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo t_VmaAlloc{};
	t_VmaAlloc.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	VKASSERT(vmaCreateBuffer(s_VKB.vma,
		&t_BufferInfo, &t_VmaAlloc,
		&m_Buffer, &m_Allocation,
		nullptr), "Vulkan::VMA, Failed to allocate memory");

	VkBufferDeviceAddressInfo t_Info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	t_Info.pNext = nullptr;
	t_Info.buffer = m_Buffer;
	m_DeviceAddress = vkGetBufferDeviceAddress(s_VKB.device, &t_Info);
}

DescriptorHeap::~DescriptorHeap()
{
	vmaDestroyBuffer(s_VKB.vma, m_Buffer, m_Allocation);
}

const DescriptorHeapHandle DescriptorHeap::Allocate(const RENDER_DESCRIPTOR_TYPE a_Type, const uint32_t a_Count)
{
	DescriptorHeapHandle t_DescHandle{};
	VkDeviceSize t_DescSize{};
	VkDescriptorGetInfoEXT t_GetInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT, nullptr };
	t_GetInfo.
	vkGetDescriptorEXT(s_VKB.device, &t_GetInfo, t_DescSize, );
}

void DescriptorHeap::Reset()
{
	//memset everything to 0 to be sure?
	m_Start = m_Position;
}
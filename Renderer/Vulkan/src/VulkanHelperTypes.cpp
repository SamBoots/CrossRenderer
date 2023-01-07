#include "VulkanHelperTypes.h"


using namespace BB;


VulkanQueueDeviceInfo BB::FindQueueIndex(VkQueueFamilyProperties* a_QueueProperties, uint32_t a_FamilyPropertyCount, VkQueueFlags a_QueueFlags)
{
	VulkanQueueDeviceInfo t_ReturnInfo;

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
}
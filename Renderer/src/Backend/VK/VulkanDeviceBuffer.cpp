#include "Precomp.h"
#include "DeviceBuffer.h"

#include "../../MainRenderer/VulkanDevice.h"
#include "../backendTypes/backend_commands.h"

namespace Render
{	
	void DeviceBuffer::Init(void* a_Device, const RLRenderBufferInfo& a_BufferInfo, void* a_Data)
	{
		VulkanDevice* t_Device = reinterpret_cast<VulkanDevice*>(a_Device);

		bufferSize = t_Device->AllignUniformBufferSize(a_BufferInfo.size);
		ASSERT(bufferSize > 0, "Buffer size is 0!");

		VkBufferUsageFlags t_Usage = 0;
		VkMemoryPropertyFlags t_MemoryProperties = 0;

		switch (a_BufferInfo.usage)
		{
		case E_RENDER_BUFFER_USAGE_VERTEX:
		{
			t_Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			t_MemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			t_Device->CreateBuffer(bufferSize, t_Usage, t_MemoryProperties, buffer, memory);

			DeviceBuffer t_StagingBuffer;
			t_StagingBuffer.Init(t_Device, RenderBufferInfo(bufferSize, E_RENDER_BUFFER_USAGE_STAGING), a_Data);

			t_Device->CopyBuffer(bufferSize, t_StagingBuffer.buffer, buffer);
			t_StagingBuffer.Destroy(a_Device);
			return;
			break;
		}
		case E_RENDER_BUFFER_USAGE_INDEX:
		{
			t_Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			t_MemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			t_Device->CreateBuffer(bufferSize, t_Usage, t_MemoryProperties, buffer, memory);

			DeviceBuffer t_StagingBuffer;
			t_StagingBuffer.Init(t_Device, RenderBufferInfo(bufferSize, E_RENDER_BUFFER_USAGE_STAGING), a_Data);

			t_Device->CopyBuffer(bufferSize, t_StagingBuffer.buffer, buffer);
			t_StagingBuffer.Destroy(a_Device);
			return;
			break;
		}
		case E_RENDER_BUFFER_USAGE_UNIFORM:
		{
			t_Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			t_MemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		}
		case E_RENDER_BUFFER_USAGE_STORAGE:
			t_Usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			t_MemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		case E_RENDER_BUFFER_USAGE_STAGING:
		{
			t_Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			t_MemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		}

		default:
			ASSERT(false, "buffer usage is not supported by the vulkan backend!");
			break;
		}

		t_Device->CreateBuffer(bufferSize, t_Usage, t_MemoryProperties, buffer, memory, a_Data);
	}

	void DeviceBuffer::Update(void* a_Device, const void* a_Data, uint64_t a_DataSize, uint64_t a_Offset) const
	{
		ASSERT(bufferSize >= (a_DataSize + a_Offset), "Trying to update a buffer that is too large.");

		VkDevice t_Device = reinterpret_cast<VulkanDevice*>(a_Device)->m_LogicalDevice;
		void* t_MapData;
		VKASSERT(vkMapMemory(t_Device, memory, a_Offset, a_DataSize, 0, &t_MapData));
		memcpy(t_MapData, a_Data, a_DataSize);
		vkUnmapMemory(t_Device, memory);
	}

	void* DeviceBuffer::MapMemory(void* a_Device, const DeviceBufferView& a_View) const
	{
		ASSERT(bufferSize >= (a_View.size + a_View.offset), "Trying to map a buffer that is too large, can cause undefined behaviour");

		VkDevice t_Device = reinterpret_cast<VulkanDevice*>(a_Device)->m_LogicalDevice;
		void* t_MapData;
		VKASSERT(vkMapMemory(t_Device, memory, a_View.offset, a_View.size, 0, &t_MapData));

		return t_MapData;
	}

	void DeviceBuffer::MemcpyMemory(void* a_MappedMemoryAddress, const void* a_Data, uint64_t a_DataSize) const
	{
		memcpy(a_MappedMemoryAddress, a_Data, a_DataSize);
	}

	void DeviceBuffer::UnMapMemory(void* a_Device) const
	{
		VkDevice t_Device = reinterpret_cast<VulkanDevice*>(a_Device)->m_LogicalDevice;
		vkUnmapMemory(t_Device, memory);
	}

	const bool DeviceBuffer::IsValid() const
	{
		if (buffer == VK_NULL_HANDLE)
			return false;

		return true;
	}

	void DeviceBuffer::Destroy(void* a_Device)
	{
		VulkanDevice* t_Device = reinterpret_cast<VulkanDevice*>(a_Device);
		vkDestroyBuffer(*t_Device, buffer, nullptr);
		vkFreeMemory(*t_Device, memory, nullptr);

		buffer = VK_NULL_HANDLE;
	}
}
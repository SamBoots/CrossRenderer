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
		BB_ASSERT(false, a_Msg)\

#else
#define VKASSERT(a_VKResult, a_Msg) a_VKResult
#endif //_DEBUG

	constexpr uint32_t EMPTY_FAMILY_INDICES = UINT32_MAX;

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
	};

	struct VulkanPipeline
	{
		VkPipeline pipeline;
		VkPipelineLayout layout;
	};

	struct VulkanFrameBuffer
	{
		uint32_t width;
		uint32_t height;
		VkFramebuffer* frameBuffers;
		VkRenderPass renderPass;
	};

	struct VulkanDevice
	{
		VkDevice logicalDevice;
		VkPhysicalDevice physicalDevice;

		struct VulkanQueue
		{
			VkQueue queue;
			uint32_t index;
		};

		VulkanQueue graphicsQueue;
		VulkanQueue presentQueue;
		VulkanQueue transferQueue;
	};

	struct VulkanDebug
	{
		VkDebugUtilsMessengerEXT debugMessenger;
		const char** extensions;
		size_t extensionCount;
	};

	//Functions
	BackendInfo VulkanCreateBackend(Allocator a_TempAllocator,const RenderBackendCreateInfo& a_CreateInfo);
	FrameBufferHandle VulkanCreateFrameBuffer(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo);

	RDescriptorHandle VulkanCreateDescriptor(Allocator a_TempAllocator, RDescriptorLayoutHandle& a_Layout, const RenderDescriptorCreateInfo& a_CreateInfo);
	PipelineHandle VulkanCreatePipeline(Allocator a_TempAllocator, const RenderPipelineCreateInfo& a_CreateInfo);
	CommandAllocatorHandle VulkanCreateCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CreateInfo);
	CommandListHandle VulkanCreateCommandList(Allocator a_TempAllocator, const RenderCommandListCreateInfo& a_CreateInfo);
	RBufferHandle VulkanCreateBuffer(const RenderBufferCreateInfo& a_Info);
	RSemaphoreHandle VulkanCreateSemaphore();
	RFenceHandle VulkanCreateFence(const FenceCreateInfo& a_Info);

	void VulkanResetCommandAllocator(const CommandAllocatorHandle a_CmdAllocatorHandle);

	RecordingCommandListHandle VulkanStartCommandList(const CommandListHandle a_CmdHandle);
	void VulkanEndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
	void VulkanStartRenderPass(const RecordingCommandListHandle a_RecordingCmdHandle, const FrameBufferHandle a_Framebuffer);
	void VulkanBindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
	void VulkanBindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
	void VulkanBindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
	void VulkanBindDescriptorSets(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_FirstSet, const uint32_t a_SetCount, const RDescriptorHandle* a_Sets, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets);
	void VulkanBindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const RENDER_SHADER_STAGE a_Stage, const uint32_t a_Offset, const uint32_t a_Size, const void* a_Data);

	void VulkanDrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
	void VulkanDrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);

	void VulkanBufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);
	void VulkanCopyBuffer(Allocator a_TempAllocator, const RenderCopyBufferInfo& a_CopyInfo);
	void* VulkanMapMemory(const RBufferHandle a_Handle);
	void VulkanUnMemory(const RBufferHandle a_Handle);

	void VulkanResizeWindow(Allocator a_TempAllocator, const uint32_t a_X, const uint32_t a_Y);
	
	void VulkanStartFrame(Allocator a_TempAllocator, const StartFrameInfo& a_StartInfo);
	void VulkanExecuteGraphicCommands(Allocator a_TempAllocator, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount, RFenceHandle a_SumbitFence);
	void VulkanExecuteTransferCommands(Allocator a_TempAllocator, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount, RFenceHandle a_SumbitFence);
	FrameIndex VulkanPresentFrame(Allocator a_TempAllocator, const PresentFrameInfo& a_PresentInfo);

	void VulkanWaitDeviceReady();

	void VulkanDestroyFence(const RFenceHandle a_Handle);
	void VulkanDestroySemaphore(const RSemaphoreHandle a_Handle);
	void VulkanDestroyBuffer(const RBufferHandle a_Handle);
	void VulkanDestroyDescriptorSetLayout(const RDescriptorLayoutHandle a_Handle);
	void VulkanDestroyDescriptorSet(const RDescriptorHandle a_Handle);
	void VulkanDestroyCommandAllocator(const CommandAllocatorHandle a_Handle);
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
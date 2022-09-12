#pragma once
#include "VulkanCommon.h"
#include "Utils/Slice.h"


namespace BB
{
	struct Allocator;

	struct VulkanPipeline
	{
		VkPipeline pipeline;
	};

	struct VulkanDevice
	{
		VkDevice logicalDevice;
		VkPhysicalDevice physicalDevice;

		VkQueue graphicsQueue;
		VkQueue presentQueue;
	};

	struct SwapChain
	{
		VkSwapchainKHR swapChain;
		VkFormat imageFormat;
		VkExtent2D extent;
		VkImage* images;
		VkImageView* imageViews;
		uint32_t imageCount;
	};

	struct VulkanFrameBuffer
	{
		uint32_t width;
		uint32_t height;
		VkFramebuffer* framebuffers;
		VkRenderPass renderPass;
		uint32_t frameBufferCount;
	};

	struct VulkanCommandList
	{
		VkCommandPool pool;
		VkCommandBuffer* recordedBuffers;
		uint32_t recordedBufferCount;
	};

	struct VulkanBackend
	{
		VkInstance instance;
		VulkanDevice device;
		VkSurfaceKHR surface;

		SwapChain mainSwapChain;

		//Debug
		VkDebugUtilsMessengerEXT debugMessenger;
		const char** extensions;
		uint32_t extensionCount;

		//The .cpp only object.
		struct VulkanBackend_o* object;
	};

	struct VulkanBackendCreateInfo
	{
		Slice<const char*> extensions;
		Slice<const char*> deviceExtensions;
#ifdef _WIN32
		HWND hwnd;
#endif //_WIN32
		const char* appName;
		const char* engineName;
		uint32_t windowWidth;
		uint32_t windowHeight;
		int version;
		bool validationLayers;
	};

	struct DepthCreateInfo
	{
		VkFormat depthFormat;
		VkImageLayout initialLayout;
		VkImageLayout finalLayout;
	};

	struct VulkanFrameBufferCreateInfo
	{
		VkFormat swapchainFormat;
		VkAttachmentLoadOp colorLoadOp;
		VkAttachmentStoreOp colorStoreOp;
		VkImageLayout colorInitialLayout;
		VkImageLayout colorFinalLayout;

		VkFormat depthFormat;

		uint32_t width;
		uint32_t height;
		VkImageView* swapChainViews;
		VkImageView depthTestView;
		uint32_t frameBufferCount;
	};

	struct VulkanPipelineCreateInfo
	{
		const VulkanFrameBuffer* pVulkanFrameBuffer;
		Slice<BB::ShaderCreateInfo> shaderCreateInfos;
	};

	VulkanBackend VulkanCreateBackend(Allocator a_TempAllocator,
		Allocator a_SysAllocator,
		const VulkanBackendCreateInfo& a_CreateInfo);

	//UNFINISHED, ONLY DOES RENDERPASS.
	VulkanFrameBuffer VulkanCreateFrameBuffer(Allocator a_SysAllocator,
		Allocator a_TempAllocator, 
		const VulkanBackend& a_VulkanBackend,
		const VulkanFrameBufferCreateInfo& a_FramebufferCreateInfo);

	VulkanPipeline VulkanCreatePipeline(Allocator a_TempAllocator,
		const VulkanBackend& a_VulkanBackend,
		const VulkanPipelineCreateInfo& a_CreateInfo);

	VulkanCommandList VulkanCreateCommandList(Allocator a_TempAllocator,
		const VulkanBackend& a_VulkanBackend);

	void VulkanDestroyCommandList(VulkanCommandList& a_CommandList,
		const VulkanBackend& a_VulkanBackend);
	void VulkanDestroyFramebuffer(Allocator a_SysAllocator,
		VulkanFrameBuffer& a_FrameBuffer,
		const VulkanBackend& a_VulkanBackend);
	void VulkanDestroyPipeline(VulkanPipeline& a_Pipeline,
		const VulkanBackend& a_VulkanBackend);
	void VulkanDestroyBackend(BB::Allocator a_SysAllocator,
		VulkanBackend& a_VulkanBackend);
}
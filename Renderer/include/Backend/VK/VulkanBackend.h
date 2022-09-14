#pragma once
#include "BackendCommands.h"
#include "Utils/Slice.h"

#include <Windows.h>


namespace BB
{
	//Common handles
	using VkBackendHandle = FrameworkHandle<struct VkBackendHandleTag>;
	using VkFrameBufferHandle = FrameworkHandle<struct VkFrameBufferHandleTag>;
	using VkPipelineHandle = FrameworkHandle<struct VkPipelineHandleTag>;
	using VkCommandListHandle = FrameworkHandle<struct VkCommandListHandleTag>;

	struct Allocator;

	struct VulkanBackendCreateInfo
	{
		Slice<RENDER_EXTENSIONS> extensions;
		Slice<RENDER_EXTENSIONS> deviceExtensions;
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

	struct VulkanFrameBufferCreateInfo
	{
		RENDER_LOAD_OP colorLoadOp;
		RENDER_STORE_OP colorStoreOp;
		RENDER_IMAGE_LAYOUT colorInitialLayout;
		RENDER_IMAGE_LAYOUT colorFinalLayout;
		uint32_t width;
		uint32_t height;
	};

	struct VulkanPipelineCreateInfo
	{
		VkFrameBufferHandle framebufferHandle;
		Slice<BB::ShaderCreateInfo> shaderCreateInfos;
	};

	void RenderFrame(Allocator a_TempAllocator,
		VkCommandListHandle a_CommandHandle,
		VkFrameBufferHandle a_FrameBufferHandle,
		VkPipelineHandle a_PipeHandle);

	VkBackendHandle VulkanCreateBackend(Allocator a_SysAllocator, 
		Allocator a_TempAllocator,
		const VulkanBackendCreateInfo& a_CreateInfo);

	VkFrameBufferHandle VulkanCreateFrameBuffer(Allocator a_TempAllocator, 
		const VulkanFrameBufferCreateInfo& a_FramebufferCreateInfo);

	VkPipelineHandle VulkanCreatePipeline(Allocator a_TempAllocator,
		const VulkanPipelineCreateInfo& a_CreateInfo);

	VkCommandListHandle VulkanCreateCommandList(Allocator a_TempAllocator,
		const uint32_t a_BufferCount);

	void VulkanDestroyCommandList(Allocator a_SysAllocator, 
		VkCommandListHandle a_Handle);
	void VulkanDestroyFramebuffer(Allocator a_SysAllocator,
		VkFrameBufferHandle a_Handle);
	void VulkanDestroyPipeline(VkPipelineHandle a_Handle);
	void VulkanDestroyBackend(VkBackendHandle a_Handle);
}
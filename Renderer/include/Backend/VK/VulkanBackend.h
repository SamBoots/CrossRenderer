#pragma once
#include "BackendCommands.h"
#include "Utils/Slice.h"

#include <Windows.h>


namespace BB
{
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
		FrameBufferHandle framebufferHandle;
		Slice<BB::ShaderCreateInfo> shaderCreateInfos;
	};

	void RenderFrame(Allocator a_TempAllocator,
		CommandListHandle a_CommandHandle,
		FrameBufferHandle a_FrameBufferHandle,
		PipelineHandle a_PipeHandle);

	APIRenderBackend VulkanCreateBackend(Allocator a_SysAllocator,
		Allocator a_TempAllocator,
		const VulkanBackendCreateInfo& a_CreateInfo);

	FrameBufferHandle VulkanCreateFrameBuffer(Allocator a_TempAllocator, 
		const VulkanFrameBufferCreateInfo& a_FramebufferCreateInfo);

	PipelineHandle VulkanCreatePipeline(Allocator a_TempAllocator,
		const VulkanPipelineCreateInfo& a_CreateInfo);

	CommandListHandle VulkanCreateCommandList(Allocator a_TempAllocator,
		const uint32_t a_BufferCount);

	void VulkanDestroyCommandList(CommandListHandle a_Handle);
	void VulkanDestroyFramebuffer(FrameBufferHandle a_Handle);
	void VulkanDestroyPipeline(PipelineHandle a_Handle);
	void VulkanDestroyBackend(APIRenderBackend a_Handle);
}
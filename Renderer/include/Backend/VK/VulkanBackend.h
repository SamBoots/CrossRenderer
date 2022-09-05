#pragma once
#include "Utils/Slice.h"

#include <vulkan/vulkan.h>


namespace BB
{
	struct Allocator;
}

struct VulkanDevice
{
	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;

	VkQueue graphicsQueue;
	VkQueue presentQueue;
};

struct VulkanBackend
{
	VkInstance instance;
	VulkanDevice device;
	VkSurfaceKHR surface;

	VkSwapchainKHR swapchain;

//Debug
	VkDebugUtilsMessengerEXT debugMessenger;
	const char** extensions;
	size_t extensionCount;
};

struct VulkanBackendCreateInfo
{
	BB::Slice<const char*> extensions;
	HWND hwnd;
	int version;
	bool validationLayers;
	const char* appName;
	const char* engineName;
	uint32_t windowWidth;
	uint32_t windowHeight;
};

VulkanBackend VKCreateBackend(BB::Allocator a_TempAllocator, BB::Allocator a_SysAllocator, const VulkanBackendCreateInfo& a_CreateInfo);
void VKDestroyBackend(BB::Allocator a_SysAllocator, VulkanBackend& a_VulkanBackend);
#pragma once
#include "VulkanBackend.h"
#include "VulkanInitializers.h"

#include "Utils/Logger.h"

#include <iostream>

#define VKASSERT(a_VKResult, a_Msg)\
	if (a_VKResult != VK_SUCCESS)\
		BB_ASSERT(false, a_Msg);\

constexpr uint32_t EMPTY_FAMILY_INDICES = UINT32_MAX;

struct SwapchainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR* formats;
	VkPresentModeKHR* presentModes;
	uint32_t formatCount;
	uint32_t presentModeCount;
};

struct QueueFamilyIndices
{
	uint32_t graphicsFamily;
	uint32_t presentFamily;
};

using namespace BB;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT a_MessageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT a_MessageType,
	const VkDebugUtilsMessengerCallbackDataEXT * a_pCallbackData,
	void* a_pUserData) {

	std::cerr << "validation layer: " << a_pCallbackData->pMessage << std::endl;

	return VK_FALSE;
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
	VkDebugUtilsMessengerEXT t_ReturnDebug;
	VKASSERT(t_CreateDebugFunc(a_Instance, &VkInit::DebugUtilsMessengerCreateInfoEXT(debugCallback), nullptr, &t_ReturnDebug), "Vulkan: Failed to create debug messenger.");
	return t_ReturnDebug;
}

static void DestroyVulkanDebug(VkInstance a_Instance, VkDebugUtilsMessengerEXT& a_DebugMsgger)
{
	auto t_DestroyDebugFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(a_Instance, "vkDestroyDebugUtilsMessengerEXT");
	if (t_DestroyDebugFunc == nullptr) 
	{
		BB_WARNING(false, "Failed to get the vkDestroyDebugUtilsMessengerEXT function pointer.", WarningType::HIGH);
	}
	t_DestroyDebugFunc(a_Instance, a_DebugMsgger, nullptr);
}

static SwapchainSupportDetails QuerySwapChainSupport(BB::Allocator a_TempAllocator, const VkSurfaceKHR a_Surface, const VkPhysicalDevice a_PhysicalDevice)
{
	SwapchainSupportDetails t_SwapDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.formatCount, nullptr);
	t_SwapDetails.formats = BBnewArr<VkSurfaceFormatKHR>(a_TempAllocator, t_SwapDetails.formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.formatCount, t_SwapDetails.formats);

	vkGetPhysicalDeviceSurfacePresentModesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.presentModeCount, nullptr);
	t_SwapDetails.presentModes = BBnewArr<VkPresentModeKHR>(a_TempAllocator, t_SwapDetails.presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(a_PhysicalDevice, a_Surface, &t_SwapDetails.presentModeCount, t_SwapDetails.presentModes);

	return t_SwapDetails;
}

static bool CheckExtensionSupport(BB::Allocator a_TempAllocator, BB::Slice<const char*> a_Extensions)
{
	// check extensions if they are available.
	uint32_t t_ExtensionCount;
	vkEnumerateInstanceExtensionProperties(nullptr, &t_ExtensionCount, nullptr);
	VkExtensionProperties* t_Extensions = BBnewArr<VkExtensionProperties>(a_TempAllocator, t_ExtensionCount);
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

static bool CheckValidationLayerSupport(BB::Allocator a_TempAllocator, BB::Slice<const char*> a_Layers)
{
	// check layers if they are available.
	uint32_t t_LayerCount;
	vkEnumerateInstanceLayerProperties(&t_LayerCount, nullptr);
	VkLayerProperties* t_Layers = BBnewArr<VkLayerProperties>(a_TempAllocator, t_LayerCount);
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

static QueueFamilyIndices FindQueueFamilies(Allocator a_TempAllocator, VkPhysicalDevice a_PhysicalDevice)
{
	QueueFamilyIndices t_Indices;
	t_Indices.graphicsFamily = EMPTY_FAMILY_INDICES;

	uint32_t t_QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, nullptr);

	VkQueueFamilyProperties* t_QueueFamilies = BBnewArr<VkQueueFamilyProperties>(a_TempAllocator, t_QueueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, t_QueueFamilies);

	for (uint32_t i = 0; i < t_QueueFamilyCount; i++)
	{
		if (t_QueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			t_Indices.graphicsFamily = i;
		}
	}

	return t_Indices;
}

static bool HasPresentQueueSupport(Allocator a_TempAllocator, VkPhysicalDevice a_PhysicalDevice, VkSurfaceKHR a_Surface, uint32_t& a_Index)
{
	uint32_t t_QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, nullptr);
	VkQueueFamilyProperties* t_QueueFamilies = BBnewArr<VkQueueFamilyProperties>(a_TempAllocator, t_QueueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(a_PhysicalDevice, &t_QueueFamilyCount, t_QueueFamilies);

	for (uint32_t i = 0; i < t_QueueFamilyCount; i++)
	{
		VkBool32 t_PresentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(a_PhysicalDevice, i, a_Surface, &t_PresentSupport);
		if (t_PresentSupport == true)
		{
			a_Index = i;
			return true;
		}
	}

	a_Index = EMPTY_FAMILY_INDICES;
	return false;
}

static VkPhysicalDevice FindPhysicalDevice(Allocator a_TempAllocator, const VkInstance a_Instance, const VkSurfaceKHR a_Surface)
{
	VkPhysicalDevice t_ReturnDevice;

	uint32_t t_DeviceCount = 0;
	vkEnumeratePhysicalDevices(a_Instance, &t_DeviceCount, nullptr);
	BB_ASSERT(t_DeviceCount != 0, "Failed to find any GPU's with vulkan support.");
	VkPhysicalDevice* t_PhysicalDevices = BBnewArr<VkPhysicalDevice>(a_TempAllocator, t_DeviceCount);
	vkEnumeratePhysicalDevices(a_Instance, &t_DeviceCount, t_PhysicalDevices);

	for (uint32_t i = 0; i < t_DeviceCount; i++)
	{
		VkPhysicalDeviceProperties t_DeviceProperties;
		VkPhysicalDeviceFeatures t_DeviceFeatures;
		vkGetPhysicalDeviceProperties(t_PhysicalDevices[i], &t_DeviceProperties);
		vkGetPhysicalDeviceFeatures(t_PhysicalDevices[i], &t_DeviceFeatures);

		SwapchainSupportDetails t_SwapChainDetails = QuerySwapChainSupport(a_TempAllocator, a_Surface, t_PhysicalDevices[i]);

		if (t_DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			t_DeviceFeatures.geometryShader &&
			FindQueueFamilies(a_TempAllocator, t_PhysicalDevices[i]).graphicsFamily != EMPTY_FAMILY_INDICES &&
			t_SwapChainDetails.formatCount != 0 &&
			t_SwapChainDetails.presentModeCount != 0)
		{
			return t_PhysicalDevices[i];
		}
	}

	BB_ASSERT(false, "Failed to find a suitable GPU that is discrete and has a geometry shader.");
	return VK_NULL_HANDLE;
}

static VkDevice CreateLogicalDevice(Allocator a_TempAllocator, BB::Slice<const char*>& a_DeviceExtensions, VkPhysicalDevice a_PhysicalDevice, VkQueue* a_GraphicsQueue)
{
	VkDevice t_ReturnDevice;

	QueueFamilyIndices t_Indices = FindQueueFamilies(a_TempAllocator, a_PhysicalDevice);

	VkDeviceQueueCreateInfo t_QueueCreateInfo{};
	t_QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	t_QueueCreateInfo.queueFamilyIndex = t_Indices.graphicsFamily;
	t_QueueCreateInfo.queueCount = 1;
	float t_QueuePriority = 1.0f;
	t_QueueCreateInfo.pQueuePriorities = &t_QueuePriority;


	VkPhysicalDeviceFeatures t_DeviceFeatures{};
	VkDeviceCreateInfo t_CreateInfo{};
	t_CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	t_CreateInfo.pQueueCreateInfos = &t_QueueCreateInfo;
	t_CreateInfo.queueCreateInfoCount = 1;
	t_CreateInfo.pEnabledFeatures = &t_DeviceFeatures;

	t_CreateInfo.ppEnabledExtensionNames = a_DeviceExtensions.data();
	t_CreateInfo.enabledExtensionCount = a_DeviceExtensions.size();

	VKASSERT(vkCreateDevice(a_PhysicalDevice, &t_CreateInfo, nullptr, &t_ReturnDevice),
		"Failed to create logical device Vulkan.");

	vkGetDeviceQueue(t_ReturnDevice, t_Indices.graphicsFamily, 0, a_GraphicsQueue);

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
		if (a_Modes[i] = VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}

	BB_WARNING(false, "Vulkan: Found no optimized Presentmode, choosing VK_PRESENT_MODE_FIFO_KHR.", WarningType::LOW);
	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkSwapchainKHR CreateSwapchain(BB::Allocator a_TempAllocator, VkSurfaceKHR a_Surface, VkPhysicalDevice a_PhysicalDevice, VkDevice a_Device, uint32_t t_SurfaceWidth, uint32_t t_SurfaceHeight)
{
	VkSwapchainKHR t_ReturnSwapchain;

	SwapchainSupportDetails t_SwapchainDetails = QuerySwapChainSupport(a_TempAllocator, a_Surface, a_PhysicalDevice);

	VkSurfaceFormatKHR t_ChosenFormat = ChooseSurfaceFormat(t_SwapchainDetails.formats, t_SwapchainDetails.formatCount);
	VkPresentModeKHR t_ChosenPresentMode = ChoosePresentMode(t_SwapchainDetails.presentModes, t_SwapchainDetails.presentModeCount);

	VkExtent2D t_ChosenExtent;
	t_ChosenExtent.width = Math::clamp(t_SurfaceWidth,
		t_SwapchainDetails.capabilities.minImageExtent.width,
		t_SwapchainDetails.capabilities.maxImageExtent.width);
	t_ChosenExtent.height = Math::clamp(t_SurfaceHeight,
		t_SwapchainDetails.capabilities.minImageExtent.height,
		t_SwapchainDetails.capabilities.maxImageExtent.height);

	//Now create the swapchain.
	uint32_t t_ImageCount = t_SwapchainDetails.capabilities.minImageCount + 1;
	if (t_SwapchainDetails.capabilities.maxImageCount > 0 && t_ImageCount > 
		t_SwapchainDetails.capabilities.maxImageCount) 
	{
		t_ImageCount = t_SwapchainDetails.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR t_SwapCreateInfo;
	t_SwapCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	t_SwapCreateInfo.surface = a_Surface;
	t_SwapCreateInfo.minImageCount = t_ImageCount;
	t_SwapCreateInfo.imageFormat = t_ChosenFormat.format;
	t_SwapCreateInfo.imageColorSpace = t_ChosenFormat.colorSpace;
	t_SwapCreateInfo.imageExtent = t_ChosenExtent;
	t_SwapCreateInfo.imageArrayLayers = 1;
	t_SwapCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	t_SwapCreateInfo.preTransform = t_SwapchainDetails.capabilities.currentTransform;
	t_SwapCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	t_SwapCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	QueueFamilyIndices t_Indices = FindQueueFamilies(a_TempAllocator, a_PhysicalDevice);
	uint32_t t_QueueFamilyIndices[] = { t_Indices.graphicsFamily, t_Indices.presentFamily };

	if (t_Indices.graphicsFamily != t_Indices.presentFamily)
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

	VKASSERT(vkCreateSwapchainKHR(a_Device, &t_SwapCreateInfo, nullptr, &t_ReturnSwapchain), "Vulkan: Failed to create swapchain.");
	return t_ReturnSwapchain;
}

VulkanBackend VKCreateBackend(BB::Allocator a_TempAllocator, BB::Allocator a_SysAllocator, const VulkanBackendCreateInfo& a_CreateInfo)
{
	VulkanBackend t_ReturnBackend;

	//Check if the extensions and layers work.
	BB_ASSERT(CheckExtensionSupport(a_TempAllocator, a_CreateInfo.extensions),
		"Vulkan: extension(s) not supported.");

#pragma region //Debug
	//For debug, we want to remember the extensions we have.
	t_ReturnBackend.extensions = BB::BBnewArr<const char*>(a_SysAllocator, a_CreateInfo.extensions.size());
	t_ReturnBackend.extensionCount = a_CreateInfo.extensions.size();
	for (size_t i = 0; i < t_ReturnBackend.extensionCount; i++)
	{
		t_ReturnBackend.extensions[i] = a_CreateInfo.extensions[i];
	}
#pragma endregion //Debug
	{
		VkApplicationInfo t_AppInfo{};
		t_AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		t_AppInfo.pApplicationName = a_CreateInfo.appName;
		t_AppInfo.pEngineName = a_CreateInfo.engineName;
		t_AppInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, a_CreateInfo.version, 0);
		t_AppInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, a_CreateInfo.version, 0);
		t_AppInfo.apiVersion = VK_MAKE_API_VERSION(0, 1, a_CreateInfo.version, 0);

		VkInstanceCreateInfo t_InstanceCreateInfo{};
		t_InstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		t_InstanceCreateInfo.pApplicationInfo = &t_AppInfo;
		if (a_CreateInfo.validationLayers)
		{
			const char* validationLayer = "VK_LAYER_KHRONOS_validation";
			BB_WARNING(CheckValidationLayerSupport(a_TempAllocator, Slice(&validationLayer, 1)), "Vulkan: Validation layer(s) not available.", WarningType::MEDIUM);
			t_InstanceCreateInfo.ppEnabledLayerNames = &validationLayer;
			t_InstanceCreateInfo.enabledLayerCount = 1;
			t_InstanceCreateInfo.pNext = &VkInit::DebugUtilsMessengerCreateInfoEXT(debugCallback);
		}
		else
		{
			t_InstanceCreateInfo.ppEnabledLayerNames = nullptr;
			t_InstanceCreateInfo.enabledLayerCount = 0;
		}
		t_InstanceCreateInfo.ppEnabledExtensionNames = a_CreateInfo.extensions.data();
		t_InstanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(a_CreateInfo.extensions.size());

		VKASSERT(vkCreateInstance(&t_InstanceCreateInfo, nullptr, &t_ReturnBackend.instance), "Failed to create Vulkan Instance!");

		if (a_CreateInfo.validationLayers)
		{
			t_ReturnBackend.debugMessenger = CreateVulkanDebugMsgger(t_ReturnBackend.instance);
		}
		else
		{
			t_ReturnBackend.debugMessenger = 0;
		}
	}

	{
		//Surface
		VkWin32SurfaceCreateInfoKHR t_SurfaceCreateInfo{};
		t_SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		t_SurfaceCreateInfo.hwnd = a_CreateInfo.hwnd;
		t_SurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
		VKASSERT(vkCreateWin32SurfaceKHR(t_ReturnBackend.instance, &t_SurfaceCreateInfo, nullptr, &t_ReturnBackend.surface),
			"Failed to create Win32 vulkan surface.");
	}

	//Get the physical Device
	t_ReturnBackend.device.physicalDevice = FindPhysicalDevice(a_TempAllocator, t_ReturnBackend.instance, t_ReturnBackend.surface);
	//Get the logical device and the graphics queue.
	t_ReturnBackend.device.logicalDevice = CreateLogicalDevice(a_TempAllocator,
		t_ReturnBackend.device.physicalDevice,
		&t_ReturnBackend.device.graphicsQueue);

	{
		uint32_t t_PresentIndex;
		if (HasPresentQueueSupport(a_TempAllocator,
			t_ReturnBackend.device.physicalDevice,
			t_ReturnBackend.surface,
			t_PresentIndex))
		{
			vkGetDeviceQueue(t_ReturnBackend.device.logicalDevice, t_PresentIndex, 0, &t_ReturnBackend.device.presentQueue);
		}
	}

	t_ReturnBackend.swapchain = CreateSwapchain(a_TempAllocator,
		t_ReturnBackend.surface,
		t_ReturnBackend.device.physicalDevice,
		t_ReturnBackend.device.logicalDevice,
		a_CreateInfo.windowWidth,
		a_CreateInfo.windowHeight);

	return t_ReturnBackend;
}

void VKDestroyBackend(BB::Allocator a_SysAllocator, VulkanBackend& a_VulkanBackend)
{
	BBfreeArr(a_SysAllocator, a_VulkanBackend.extensions);

	vkDestroySwapchainKHR(a_VulkanBackend.device.logicalDevice, a_VulkanBackend.swapchain, nullptr);
	vkDestroyDevice(a_VulkanBackend.device.logicalDevice, nullptr);

	if (a_VulkanBackend.debugMessenger != 0)
		DestroyVulkanDebug(a_VulkanBackend.instance, a_VulkanBackend.debugMessenger);

	vkDestroySurfaceKHR(a_VulkanBackend.instance, a_VulkanBackend.surface, nullptr);
	vkDestroyInstance(a_VulkanBackend.instance, nullptr);
}
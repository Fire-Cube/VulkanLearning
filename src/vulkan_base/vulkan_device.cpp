#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "vulkan_base.h"

#include "logger.h"
#include "utils.h"
#include "types.h"

VkBool32 VKAPI_CALL debugReportCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT messageTypes, const vk::DebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) {
	if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
		LOG_ERROR(callbackData->pMessage);
	}
	else if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
		LOG_WARNING(callbackData->pMessage);
	}
	else {
		assert(false);
	}

	return false;
}

vk::DebugUtilsMessengerEXT registerDebugCallback(vk::Instance instance) {
	vk::DebugUtilsMessengerCreateInfoEXT debugCallbackCreateInfo {};
	debugCallbackCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
	debugCallbackCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
	debugCallbackCreateInfo.pfnUserCallback = debugReportCallback;

	return instance.createDebugUtilsMessengerEXT(debugCallbackCreateInfo);
}

bool initVulkanInstance(VulkanContext* context, u32 instanceExtensionsCount, const char* const* instanceExtensions) {
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	// Vk Layers
	const auto layerProperties = VKA(vk::enumerateInstanceLayerProperties());
	LOG_DEBUG("Active Vulkan Layers: " + std::to_string(layerProperties.size()));
	for (auto const& layer : layerProperties) {
		LOG_DEBUG("Layer: "       + std::string(layer.layerName));
		LOG_DEBUG("Description: " + std::string(layer.description));
	}

	std::vector<const char*> enabledLayers;

#ifdef DEBUG_BUILD
	enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif
	if (!enabledLayers.empty()) {
		std::span<const char*> layerSpan(
			enabledLayers.data(),
			enabledLayers.size()
		);
		LOG_INFO("Enabled Layers: " + utils::join(layerSpan));
	}
	else {
		LOG_INFO("No enabled Layers");
	}

	// Vk Extensions
	const auto instanceExtensionProperties = VKA(vk::enumerateInstanceExtensionProperties());
	LOG_DEBUG("Vulkan Extensions: " + std::to_string(instanceExtensionProperties.size()));
	for (auto const& ext : instanceExtensionProperties) {
		LOG_DEBUG("Extension: " + std::string(ext.extensionName));
	}

#ifdef DEBUG_BUILD
	std::vector<vk::ValidationFeatureEnableEXT> enableValidationFeatures = {};

	// enableValidationFeatures.push_back(vk::ValidationFeatureEnableEXT::eBestPractices);
	enableValidationFeatures.push_back(vk::ValidationFeatureEnableEXT::eGpuAssisted);
	enableValidationFeatures.push_back(vk::ValidationFeatureEnableEXT::eSynchronizationValidation);
	enableValidationFeatures.push_back(vk::ValidationFeatureEnableEXT::eDebugPrintf);


	vk::ValidationFeaturesEXT validationFeatures = {};
	validationFeatures.enabledValidationFeatureCount = enableValidationFeatures.size();
	validationFeatures.pEnabledValidationFeatures = enableValidationFeatures.data();

#endif

	vk::ApplicationInfo applicationInfo{};
	applicationInfo.pApplicationName   = "VulkanLearning";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName        = "No Engine";
	applicationInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.apiVersion         = VK_API_VERSION_1_4;       // Tutorial was 1.2

	vk::InstanceCreateInfo instanceCreateInfo{};
#ifdef DEBUG_BUILD
	instanceCreateInfo.pNext = &validationFeatures;
#endif
	instanceCreateInfo.pApplicationInfo        = &applicationInfo;
	instanceCreateInfo.enabledLayerCount       = static_cast<u32>(std::size(enabledLayers));
	instanceCreateInfo.ppEnabledLayerNames     = enabledLayers.empty() ? nullptr : enabledLayers.data();
	instanceCreateInfo.enabledExtensionCount   = instanceExtensionsCount;
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions;

	context->instance = VKA(vk::createInstance(instanceCreateInfo));

	VULKAN_HPP_DEFAULT_DISPATCHER.init(context->instance);

#ifdef DEBUG_BUILD
	context->debugCallback = registerDebugCallback(context->instance);
#endif

	return true;
}

bool selectPhysicalDevice(VulkanContext* context) {
	vk::Instance instance{ context->instance };
	const auto devices = VKA(instance.enumeratePhysicalDevices());
	if (devices.empty()) {
		LOG_ERROR("No physical devices found with Vulkan support");
		context->physicalDevice = nullptr;
		return false;
	}

	LOG_INFO("Found " + std::to_string(devices.size()) + " GPUs:");

	std::size_t gpuIndex = 0;
	for (auto const& device : devices) {
		auto props = VK(vk::PhysicalDevice{ device }.getProperties());
		LOG_INFO("GPU " + std::to_string(gpuIndex) + ": " + std::string(props.deviceName));
		gpuIndex ++;
	}

	context->physicalDevice = devices.front();
	LOG_INFO("Selected GPU: " + std::to_string(gpuIndex - 1));

	context->physicalDeviceProperties = VK(vk::PhysicalDevice{ context->physicalDevice }.getProperties());

	return true;
}

bool createLogicalDevice(VulkanContext* context, u32 deviceExtensionsCount, const char* const* deviceExtensions) {
	auto queueFamilies = context->physicalDevice.getQueueFamilyProperties();

	u32 graphicsQueueIndex = UINT32_MAX;
	for (u32 i = 0; i < queueFamilies.size(); ++i) {
		auto queueFamily = queueFamilies[i];
		if (queueFamily.queueCount > 0) {
			if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
				graphicsQueueIndex = i;
				 break;
			}
		}
	}

	if (graphicsQueueIndex == UINT32_MAX) {
		LOG_ERROR("Failed to find graphics queue index");
		return false;
	}

	constexpr float queuePriorities[] = { 1.0f };

	vk::DeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.queueFamilyIndex = graphicsQueueIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = queuePriorities;

	vk::PhysicalDeviceFeatures enabledFeatures{};

	vk::DeviceCreateInfo createInfo{};
	createInfo.queueCreateInfoCount = 1;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.enabledExtensionCount = deviceExtensionsCount;
	createInfo.ppEnabledExtensionNames = deviceExtensions;
	createInfo.pEnabledFeatures = &enabledFeatures;

	try {
		context->device = context->physicalDevice.createDevice(createInfo);
	}
	catch (vk::SystemError const& e) {
		LOG_ERROR("Failed to create logical device: " + std::string(e.what()));
		return false;
	}

	VULKAN_HPP_DEFAULT_DISPATCHER.init(context->device);

	context->graphicsQueue.familyIndex = graphicsQueueIndex;
	context->graphicsQueue.queue = context->device.getQueue(graphicsQueueIndex, 0);

	vk::PhysicalDeviceMemoryProperties deviceMemoryProperties = VK(context->physicalDevice.getMemoryProperties());
	LOG_INFO("Num device memory heaps: " + std::to_string(deviceMemoryProperties.memoryHeapCount));
	for (u32 i = 0; i < deviceMemoryProperties.memoryHeapCount; ++i) {
		const char* isDeviceLocal = "false";
		if (deviceMemoryProperties.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
			isDeviceLocal = "true";
		}

		if (deviceMemoryProperties.memoryHeaps[i].size == 0) {
			continue;
		}

		LOG_INFO("Heap: " + std::to_string(i) + " | Size: " + utils::formatBytes(deviceMemoryProperties.memoryHeaps[i].size) + " | device local: " + isDeviceLocal);
	}

	LOG_INFO("Resizeable bar detected: " + std::string{ (detectResizeableBar(context) ? "true" : "false") });


	return true;
}

bool initVulkan(VulkanContext* context, u32 instanceExtensionsCount, const char* const* instanceExtensions, u32 deviceExtensionsCount, const char* const* deviceExtensions) {
	if (!initVulkanInstance(context, instanceExtensionsCount, instanceExtensions)) {
		return false;
	}

	if (!selectPhysicalDevice(context)) {
		return false;
	}

	if (!createLogicalDevice(context, deviceExtensionsCount, deviceExtensions)) {
		return false;
	}

	return true;
}

void exitVulkan(VulkanContext* context) {
	VKA(context->device.waitIdle());
	VKA(context->device.destroy());

#ifdef DEBUG_BUILD
	context->instance.destroyDebugUtilsMessengerEXT(context->debugCallback);
	context->debugCallback = nullptr;
#endif

	VKA(context->instance.destroy());
}

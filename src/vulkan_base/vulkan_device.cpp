#include "vulkan_base.h"

#include "../logger.h"
#include "../utils.hpp"

bool initVulkanInstance(VulkanContext* context, uint32_t instanceExtensionsCount, const char** instanceExtensions) {
	// Vk Layers
	const auto layerProperties = VKA(vk::enumerateInstanceLayerProperties());
	LOG_DEBUG("Active Vulkan Layers: " + std::to_string(layerProperties.size()));
	for (auto const& layer : layerProperties) {
		LOG_DEBUG("Layer: "       + std::string(layer.layerName));
		LOG_DEBUG("Description: " + std::string(layer.description));
	}

	const char* enabledLayers[] = {
		"VK_LAYER_KHRONOS_validation"
	};
	LOG_INFO("Enabled Layers: " + utils::join(enabledLayers));

	// Vk Extensions
	const auto instanceExtensionProperties = VKA(vk::enumerateInstanceExtensionProperties());
	LOG_INFO("Vulkan Extensions: " + std::to_string(instanceExtensionProperties.size()));
	for (auto const& ext : instanceExtensionProperties) {
		LOG_INFO("Extension: " + std::string(ext.extensionName));
	}

	vk::ApplicationInfo applicationInfo{};
	applicationInfo.pApplicationName   = "VulkanLearning";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName        = "No Engine";
	applicationInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.apiVersion        = VK_API_VERSION_1_4;       // Tutorial was 1.2


	vk::InstanceCreateInfo createInfo{};
	createInfo.pApplicationInfo        = &applicationInfo;
	createInfo.enabledLayerCount       = static_cast<uint32_t>(std::size(enabledLayers));
	createInfo.ppEnabledLayerNames     = enabledLayers;
	createInfo.enabledExtensionCount   = instanceExtensionsCount;
	createInfo.ppEnabledExtensionNames = instanceExtensions;


	context->instance = VKA(vk::createInstance(createInfo));

	return true;
}

bool selectPhysicalDevice(VulkanContext* context)
{
	vk::Instance instance{ context->instance };
	const auto devices = VKA(instance.enumeratePhysicalDevices());
	if (devices.empty()) {
		LOG_ERROR("No physical devices found with Vulkan support");
		context->physicalDevice = nullptr;
		return false;
	}

	LOG_INFO("Found " + std::to_string(devices.size()) + " GPUs:");

	std::size_t gpuIndex = 0;
	for (auto const& device : devices)
	{
		auto props = VK(vk::PhysicalDevice{ device }.getProperties());
		LOG_INFO("GPU " + std::to_string(gpuIndex) + ": " + std::string(props.deviceName));
		gpuIndex ++;
	}

	context->physicalDevice = devices.front();
	LOG_INFO("Selected GPU: " + std::to_string(gpuIndex - 1));

	context->physicalDeviceProperties = VK(vk::PhysicalDevice{ context->physicalDevice }.getProperties());

	return true;
}

std::unique_ptr<VulkanContext> initVulkan(uint32_t instanceExtensionsCount, const char** instanceExtensions) {
	auto context = std::make_unique<VulkanContext>();

	if (!initVulkanInstance(context.get(), instanceExtensionsCount, instanceExtensions)) {
		return nullptr;
	}

	if (!selectPhysicalDevice(context.get()))
	{
		return nullptr;
	}

	return context;
}

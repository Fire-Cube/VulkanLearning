#include <vulkan/vulkan.hpp>
#include <cassert>

#include "../logger.h"

#define VKA(expr)													\
	([&]() -> decltype(auto) {										\
		try {														\
			return (expr);											\
		}															\
		catch (const vk::SystemError& err) {						\
			LOG_DEBUG("Vulkan error: " + std::string(err.what()));	\
			assert(false);											\
			using ReturnType = decltype(expr);						\
			return ReturnType{}; /* fallback return */				\
		}															\
	}())

#define VK(func) (func)

struct VulkanContext {
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties physicalDeviceProperties;
};

std::unique_ptr<VulkanContext>  initVulkan(uint32_t instanceExtensionsCount, const char** instanceExtensions);

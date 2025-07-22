#pragma once

#include <cassert>

#include <vulkan/vulkan.hpp>

#include "logger.h"
#include "types.h"


#define VKA(expr)													\
	([&]() -> decltype(auto) {										\
		try {														\
			return (expr);											\
		}															\
		catch (const vk::SystemError& err) {						\
			LOG_DEBUG("Vulkan error: " + std::string(err.what()));	\
			throw;                                                  \
		}															\
	}())

#define VK(func) (func)

struct VulkanQueue {
	vk::Queue queue{};
	u32 familyIndex = 0;
};

struct VulkanContext {
	vk::Instance instance{};
	vk::PhysicalDevice physicalDevice{};
	vk::PhysicalDeviceProperties physicalDeviceProperties{};
	vk::Device device{};
	VulkanQueue graphicsQueue{};
};

std::unique_ptr<VulkanContext> initVulkan(u32 instanceExtensionsCount, const char* const* instanceExtensions, u32 deviceExtensionsCount, const char* const* deviceExtensions);

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
	vk::Queue queue {};
	u32 familyIndex = 0;
};

struct VulkanSwapchain {
	vk::SwapchainKHR swapchain {};
	u32 width;
	u32 height;
	vk::Format format;
	std::vector<vk::Image> images {};
	std::vector<vk::ImageView> imageViews {};
};

struct VulkanPipeline {
	vk::Pipeline pipeline {};
	vk::PipelineLayout pipelineLayout {};
};

struct VulkanContext {
	vk::Instance instance {};
	vk::PhysicalDevice physicalDevice {};
	vk::PhysicalDeviceProperties physicalDeviceProperties {};
	vk::Device device {};
	VulkanQueue graphicsQueue {};
	vk::DebugUtilsMessengerEXT debugCallback {};
};

struct VulkanBuffer {
	vk::Buffer buffer {};
	vk::DeviceMemory memory {};
	bool resizeableBar = false;
};

struct VulkanImage {
	vk::Image image {};
	vk::ImageView imageView {};
	vk::DeviceMemory memory {};
};

// vulkan_device.cpp
bool initVulkan(VulkanContext* context, u32 instanceExtensionsCount, const char* const* instanceExtensions, u32 deviceExtensionsCount, const char* const* deviceExtensions);
void exitVulkan(VulkanContext* context);

// vulkan_swapchain.cpp
VulkanSwapchain createSwapchain(VulkanContext* context, vk::SurfaceKHR surface, vk::ImageUsageFlags imageUsage, VulkanSwapchain* oldSwapchain=nullptr);
void destroySwapchain(VulkanContext* context, VulkanSwapchain* swapchain);

// vulkan_renderpass.cpp
vk::RenderPass createRenderPass(VulkanContext* context, vk::Format format, vk::SampleCountFlagBits sampleCount);
void destroyRenderPass(VulkanContext* context, vk::RenderPass renderPass);

// vulkan_pipeline.cpp
VulkanPipeline createPipeline(VulkanContext* context, const char* vertexShaderFilename, const char* fragmentShaderFilename,
								VkRenderPass renderPass, u32 width, u32 height, vk::VertexInputAttributeDescription* attributes,
								u32 numAttributes, vk::VertexInputBindingDescription* binding, u32 numSetLayout, vk::DescriptorSetLayout* setLayouts,
								vk::PushConstantRange* pushConstant, u32 subpassIndex = 0, vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1);

void destroyPipeline(VulkanContext* context, VulkanPipeline* pipeline);

// vulkan_utils.cpp
bool detectResizeableBar(VulkanContext* context);
void createBuffer(VulkanContext* context, VulkanBuffer* buffer, u64 size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryProperties);
void uploadDataToBuffer(VulkanContext* context, VulkanBuffer* buffer, void* data, size_t size);
void destroyBuffer(VulkanContext* context, VulkanBuffer* buffer);
void createImage(VulkanContext* context, VulkanImage* image, u32 width, u32 height, vk::Format format, vk::ImageUsageFlags usage, vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1);
void uploadDataToImage(VulkanContext* context, VulkanImage* image, void* data, size_t size, u32 width, u32 height, vk::ImageLayout finalLayout, vk::AccessFlags dstAccessMask);
void destroyImage(VulkanContext* context, VulkanImage* image);
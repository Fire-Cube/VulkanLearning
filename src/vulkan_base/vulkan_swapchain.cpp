#include "vulkan_base.h"

VulkanSwapchain createSwapchain(VulkanContext* context, vk::SurfaceKHR surface, vk::ImageUsageFlags imageUsage) {
    VulkanSwapchain result_swapchain{};

    auto supportsPresent = VKA(context->physicalDevice.getSurfaceSupportKHR(context->graphicsQueue.familyIndex, surface));
    if (!supportsPresent) {
        LOG_ERROR("Graphics queue does not support present");
        return result_swapchain;
    }

    auto availableFormats = VKA(context->physicalDevice.getSurfaceFormatsKHR(surface));
    if (availableFormats.empty()) {
        LOG_ERROR("No surface formats available");
        return result_swapchain;
    }

    auto format = availableFormats.front().format;
    auto colorSpace = availableFormats.front().colorSpace;

    LOG_INFO("Selected surface format: " + vk::to_string(format) + ", colorSpace: "+ vk::to_string(colorSpace));

    auto surfaceCapabilities = VKA(context->physicalDevice.getSurfaceCapabilitiesKHR(surface));
    if (surfaceCapabilities.currentExtent.width == 0xFFFFFFFF) {
        surfaceCapabilities.currentExtent.width = surfaceCapabilities.minImageExtent.width;
    }

    if (surfaceCapabilities.currentExtent.height != 0xFFFFFFFF) {
        surfaceCapabilities.currentExtent.height = surfaceCapabilities.minImageExtent.height;
    }

    if (surfaceCapabilities.maxImageCount == 0) {
        surfaceCapabilities.maxImageCount = 8; // Todo: Bigger than minImageCount
    }

    vk::SwapchainCreateInfoKHR createInfoSwapchain{};
    createInfoSwapchain.surface = surface;
    createInfoSwapchain.minImageCount = 3;
    createInfoSwapchain.imageFormat = format;
    createInfoSwapchain.imageColorSpace = colorSpace;
    createInfoSwapchain.imageExtent = surfaceCapabilities.currentExtent;
    createInfoSwapchain.imageArrayLayers = 1;
    createInfoSwapchain.imageUsage = imageUsage;
    createInfoSwapchain.imageSharingMode = vk::SharingMode::eExclusive;
    createInfoSwapchain.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    createInfoSwapchain.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfoSwapchain.presentMode = vk::PresentModeKHR::eFifo;
    VKA(context->device.createSwapchainKHR(&createInfoSwapchain, nullptr, &result_swapchain.swapchain));


    result_swapchain.format = format;
    result_swapchain.width = surfaceCapabilities.currentExtent.width;
    result_swapchain.height = surfaceCapabilities.currentExtent.height;

    const u32 numImages = context->device.getSwapchainImagesKHR(result_swapchain.swapchain).size();
    result_swapchain.images.resize(numImages);
    result_swapchain.images = context->device.getSwapchainImagesKHR(result_swapchain.swapchain);

    return result_swapchain;
}

void destroySwapchain(VulkanContext* context, VulkanSwapchain* swapchain) {
    VK(context->device.destroySwapchainKHR(swapchain->swapchain, nullptr));
}
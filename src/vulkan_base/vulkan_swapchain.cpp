#include "vulkan_base.h"

VulkanSwapchain createSwapchain(VulkanContext* context, vk::SurfaceKHR surface, vk::ImageUsageFlags imageUsage, VulkanSwapchain* oldSwapchain) {
    VulkanSwapchain result_swapchain {};

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

    vk::SwapchainCreateInfoKHR swapchainCreateInfo {};
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = 3;
    swapchainCreateInfo.imageFormat = format;
    swapchainCreateInfo.imageColorSpace = colorSpace;
    swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = imageUsage;
    swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapchainCreateInfo.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapchainCreateInfo.presentMode = vk::PresentModeKHR::eFifo;
    swapchainCreateInfo.oldSwapchain = oldSwapchain ? oldSwapchain->swapchain : VK_NULL_HANDLE;
    VKA(context->device.createSwapchainKHR(&swapchainCreateInfo, nullptr, &result_swapchain.swapchain));

    result_swapchain.format = format;
    result_swapchain.width = surfaceCapabilities.currentExtent.width;
    result_swapchain.height = surfaceCapabilities.currentExtent.height;

    const u32 numImages = context->device.getSwapchainImagesKHR(result_swapchain.swapchain).size();
    result_swapchain.images.resize(numImages);
    result_swapchain.images = context->device.getSwapchainImagesKHR(result_swapchain.swapchain);

    // ImageViews
    result_swapchain.imageViews.resize(numImages);
    for (u32 i = 0; i < numImages; i++) {
        vk::ImageViewCreateInfo imageViewCreateInfo {};
        imageViewCreateInfo.image = result_swapchain.images[i];
        imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
        imageViewCreateInfo.format = format;
        imageViewCreateInfo.components = { vk::ComponentSwizzle::eIdentity };
        imageViewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        result_swapchain.imageViews[i] = VKA(context->device.createImageView(imageViewCreateInfo));
    }

    return result_swapchain;
}

void destroySwapchain(VulkanContext* context, VulkanSwapchain* swapchain) {
    for (auto& view : swapchain->imageViews) {
        context->device.destroyImageView(view);
    }
    VK(context->device.destroySwapchainKHR(swapchain->swapchain, nullptr));
}

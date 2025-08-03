#include "vulkan_base.h"

u32 findMemoryType(VulkanContext* context, u32 typeFilter, vk::MemoryPropertyFlags memoryProperties) {
    vk::PhysicalDeviceMemoryProperties deviceMemoryProperties = VK(context->physicalDevice.getMemoryProperties());

    for (u32 i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
        // check if required memory type is allowed
        if (typeFilter & (1 << i)) {
            // check if required properties are satisfied
            if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & memoryProperties) == memoryProperties) {
                LOG_DEBUG("Using memory type index " + std::to_string(i) + " (heap " + std::to_string(deviceMemoryProperties.memoryTypes[i].heapIndex) + ")");
                return i;
            }
        }
    }
    assert(false);
    return UINT32_MAX;
}

bool detectResizeableBar(VulkanContext* context) {
    vk::PhysicalDeviceMemoryProperties deviceMemoryProperties = VK(context->physicalDevice.getMemoryProperties());

    for (u32 i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
        auto& memoryType = deviceMemoryProperties.memoryTypes[i];

        if ((memoryType.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) && (memoryType.propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible)) {
            return true;
        }
    }
    return false;
}

void createBuffer(VulkanContext* context, VulkanBuffer* buffer, u64 size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryProperties) {
    vk::BufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;

    buffer->buffer = VKA(context->device).createBuffer(bufferCreateInfo);

    vk::MemoryRequirements memoryRequirements = VK(context->device.getBufferMemoryRequirements(buffer->buffer));

    vk::MemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryType(context, memoryRequirements.memoryTypeBits, memoryProperties);

    buffer->memory = VKA(context->device.allocateMemory(memoryAllocateInfo));

    VKA(context->device.bindBufferMemory(buffer->buffer, buffer->memory, 0));
}

void uploadDataToBuffer(VulkanContext* context, VulkanBuffer* buffer, void* data, size_t size) {
    // if (detectResizeableBar(context)) {
    //     void* mapped;
    //     VKA(context->device.mapMemory(buffer->memory, 0, size, {}, &mapped));
    //     memcpy(mapped, data, size);
    //     VKA(context->device.unmapMemory(buffer->memory));
    //     return;
    // }

    VulkanQueue* queue = &context->graphicsQueue;
    vk::CommandPool commandPool;
    vk::CommandBuffer commandBuffer;
    VulkanBuffer stagingBuffer;

    createBuffer(context, &stagingBuffer, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* mappedData;
    VKA(context->device.mapMemory(stagingBuffer.memory, 0, size, {}, &mappedData));
    memcpy(mappedData, data, size);
    VKA(context->device.unmapMemory(stagingBuffer.memory));

    vk::CommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
    commandPoolCreateInfo.queueFamilyIndex = queue->familyIndex;

    commandPool = VKA(context->device.createCommandPool(commandPoolCreateInfo));

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
    commandBufferAllocateInfo.commandBufferCount = 1;

    auto commandBuffersCreated = VKA(context->device.allocateCommandBuffers(commandBufferAllocateInfo));
    commandBuffer = commandBuffersCreated.front();

    vk::CommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    VKA(commandBuffer.begin(&commandBufferBeginInfo));

    vk::BufferCopy copyRegion { 0, 0, size };
    VK(commandBuffer.copyBuffer(stagingBuffer.buffer, buffer->buffer, 1, &copyRegion));

    VKA(commandBuffer.end());

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VKA(queue->queue.submit(submitInfo));
    VKA(queue->queue.waitIdle());

    VK(context->device.destroyCommandPool(commandPool));
    destroyBuffer(context, &stagingBuffer);
}

void destroyBuffer(VulkanContext* context, VulkanBuffer* buffer) {
    VK(context->device.destroyBuffer(buffer->buffer));
    VK(context->device.freeMemory(buffer->memory));
}

void createImage(VulkanContext* context, VulkanImage* image, u32 width, u32 height, vk::Format format, vk::ImageUsageFlags usage) {
    vk::ImageCreateInfo imageCreateInfo{};
    imageCreateInfo.imageType = vk::ImageType::e2D;
    imageCreateInfo.extent = vk::Extent3D{ width, height, 1 };
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = format;
    imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
    imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageCreateInfo.usage = usage;
    imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
    imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;

    image->image = VKA(context->device.createImage(imageCreateInfo));

    vk::MemoryRequirements memoryRequirements = VK(context->device.getImageMemoryRequirements(image->image));

    vk::MemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryType(context, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    image->memory = VKA(context->device.allocateMemory(memoryAllocateInfo));
    VKA(context->device.bindImageMemory(image->image, image->memory, 0));

    vk::ImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.image = image->image;
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    image->imageView = VKA(context->device.createImageView(imageViewCreateInfo));
}

void uploadDataToImage(VulkanContext* context, VulkanImage* image, void* data, size_t size, u32 width, u32 height, vk::ImageLayout finalLayout, vk::AccessFlags dstAccessMask) {
    VulkanQueue* queue = &context->graphicsQueue;
    vk::CommandPool commandPool;
    vk::CommandBuffer commandBuffer;
    VulkanBuffer stagingBuffer;

    createBuffer(context, &stagingBuffer, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* mappedData;
    VKA(context->device.mapMemory(stagingBuffer.memory, 0, size, {}, &mappedData));
    memcpy(mappedData, data, size);
    VKA(context->device.unmapMemory(stagingBuffer.memory));

    vk::CommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
    commandPoolCreateInfo.queueFamilyIndex = queue->familyIndex;

    commandPool = VKA(context->device.createCommandPool(commandPoolCreateInfo));

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
    commandBufferAllocateInfo.commandBufferCount = 1;

    auto commandBuffersCreated = VKA(context->device.allocateCommandBuffers(commandBufferAllocateInfo));
    commandBuffer = commandBuffersCreated.front();

    vk::CommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    VKA(commandBuffer.begin(&commandBufferBeginInfo));

    vk::ImageMemoryBarrier imageMemoryBarrier {};
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
    imageMemoryBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    imageMemoryBarrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    imageMemoryBarrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    imageMemoryBarrier.image = image->image;
    imageMemoryBarrier.subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eNone;
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    VKA(commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));

    vk::BufferImageCopy imageCopyRegion {};
    imageCopyRegion.imageSubresource = vk::ImageSubresourceLayers { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    imageCopyRegion.imageExtent = vk::Extent3D{ width, height, 1};

    VKA(commandBuffer.copyBufferToImage(stagingBuffer.buffer, image->image, vk::ImageLayout::eTransferDstOptimal, 1, &imageCopyRegion));

    vk::ImageMemoryBarrier imageMemoryBarrier2 {};
    imageMemoryBarrier2.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    imageMemoryBarrier2.newLayout = finalLayout;
    imageMemoryBarrier2.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    imageMemoryBarrier2.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    imageMemoryBarrier2.image = image->image;
    imageMemoryBarrier2.subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    imageMemoryBarrier2.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    imageMemoryBarrier2.dstAccessMask = dstAccessMask;

    VKA(commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier2));

    VKA(commandBuffer.end());

    vk::SubmitInfo submitInfo {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VKA(queue->queue.submit(submitInfo));
    VKA(queue->queue.waitIdle());

    VK(context->device.destroyCommandPool(commandPool));
    destroyBuffer(context, &stagingBuffer);
}

void destroyImage(VulkanContext* context, VulkanImage* image) {
    VK(context->device.destroyImageView(image->imageView));
    VK(context->device.destroyImage(image->image));
}

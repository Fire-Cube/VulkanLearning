#include "vulkan_base.h"

u32 findMemoryType(VulkanContext* context, u32 typeFilter, vk::MemoryPropertyFlags memoryProperties) {
    vk::PhysicalDeviceMemoryProperties deviceMemoryProperties = VK(context->physicalDevice.getMemoryProperties());

    for (u32 i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
        // check if required memory type is allowed
        if (typeFilter & (1 << i)) {
            // check if required properties are satisfied
            if (deviceMemoryProperties.memoryTypes[i].propertyFlags & memoryProperties) {
                LOG_INFO("Using memory heap index " + std::to_string(deviceMemoryProperties.memoryTypes[i].heapIndex));
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
        auto& heap = deviceMemoryProperties.memoryHeaps[memoryType.heapIndex];

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

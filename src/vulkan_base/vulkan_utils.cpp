#include "vulkan_base.h"

u32 findMemoryType(VulkanContext* context, u32 typeFilter, vk::MemoryPropertyFlagBits memoryProperties) {
    vk::PhysicalDeviceMemoryProperties deviceMemoryProperties = VK(context->physicalDevice.getMemoryProperties());

    for (u32 i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
        // check if required memory type is allowed
        if (typeFilter & (1 << i)) {
            // check if required properties are satisfied
            if (deviceMemoryProperties.memoryTypes[i].propertyFlags & memoryProperties) {
                return i;
            }
        }
    }
    assert(false);
    return UINT32_MAX;
}

void createBuffer(VulkanContext* context, VulkanBuffer* buffer, u64 size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlagBits memoryProperties) {
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

void destroyBuffer(VulkanContext* context, VulkanBuffer* buffer) {
    VK(context->device.destroyBuffer(buffer->buffer));
    VK(context->device.freeMemory(buffer->memory));
}

#pragma once

#include <vulkan/vulkan.hpp>

struct commandLabelScope {
    vk::CommandBuffer commandBuffer;
    commandLabelScope(vk::CommandBuffer c, const char* name) : commandBuffer(c) {
        vk::DebugUtilsLabelEXT label; label.pLabelName = name;
        commandBuffer.beginDebugUtilsLabelEXT(label);
    }
    ~commandLabelScope(){ commandBuffer.endDebugUtilsLabelEXT(); }
};

#define SCOPE_LABEL(name) commandLabelScope scope(commandBuffer, name);
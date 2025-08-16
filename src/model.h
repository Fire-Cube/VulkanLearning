#include "vulkan_base/vulkan_base.h"

struct Model {
    VulkanBuffer vertexBuffer;
    VulkanBuffer indexBuffer;
    u64 numIndices;
    VulkanImage albedoTexture;
};

Model createModel(VulkanContext* context, const char* filename, const char* modelDir, cgltf_component_type componentType);
void destroyModel(VulkanContext* context, Model* model);

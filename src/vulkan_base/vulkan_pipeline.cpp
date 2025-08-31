#include <filesystem>

#include "utils.h"
#include "vulkan_base.h"

vk::ShaderModule createShaderModule(VulkanContext* context, const std::string shaderFilename) {
    vk::ShaderModule resultShaderModule {};

    if (!std::filesystem::exists(shaderFilename)) {
        LOG_ERROR("Shader not found: " + shaderFilename);
        return resultShaderModule;
    }

    std::ifstream file(shaderFilename, std::ios::binary);
    const size_t fileSize = std::filesystem::file_size(shaderFilename);
    assert((fileSize % 4) == 0);

    std::vector<char> bytes(fileSize);
    file.read(bytes.data(), fileSize);
    if (!file) {
        LOG_ERROR("Reading shader file failed: " + shaderFilename);
        return resultShaderModule;
    }
    file.close();

    vk::ShaderModuleCreateInfo shaderCreateInfo {};
    shaderCreateInfo.codeSize = fileSize;
    shaderCreateInfo.pCode = reinterpret_cast<u32*>(bytes.data());
    resultShaderModule = VKA(context->device.createShaderModule(shaderCreateInfo));

    return resultShaderModule;
}

VulkanPipeline createPipeline(VulkanContext* context, const char* vertexShaderFilename, const char* fragmentShaderFilename,
                                VkRenderPass renderPass, u32 width, u32 height, vk::VertexInputAttributeDescription* attributes,
                                u32 numAttributes, vk::VertexInputBindingDescription* binding, u32 numSetLayout, vk::DescriptorSetLayout* setLayouts,
                                vk::PushConstantRange* pushConstant, u32 subpassIndex, vk::SampleCountFlagBits sampleCount) {

    vk::ShaderModule vertexShaderModule { createShaderModule(context, vertexShaderFilename )};
    vk::ShaderModule fragmentShaderModule { createShaderModule(context, fragmentShaderFilename )};

    vk::PipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0].module = vertexShaderModule;
    shaderStages[0].stage = vk::ShaderStageFlagBits::eVertex;
    shaderStages[0].pName = "main";

    shaderStages[1].module = fragmentShaderModule;
    shaderStages[1].stage = vk::ShaderStageFlagBits::eFragment;
    shaderStages[1].pName = "main";

    vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo {};
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = binding ? 1 : 0;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = binding;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = numAttributes;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributes;

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo {};
    inputAssemblyStateCreateInfo.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportStateCreateInfo {};
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo {};
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo {};
    multisampleStateCreateInfo.rasterizationSamples = sampleCount;

    vk::PipelineColorBlendAttachmentState colorBlendAttachmentState {};
    colorBlendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachmentState.blendEnable = true;
    colorBlendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachmentState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachmentState.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachmentState.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo {};
    depthStencilStateCreateInfo.depthTestEnable = true;
    depthStencilStateCreateInfo.depthWriteEnable = true;
    depthStencilStateCreateInfo.depthCompareOp = vk::CompareOp::eGreaterOrEqual;
    depthStencilStateCreateInfo.minDepthBounds = 0.0f;
    depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

    vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo {};
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

    vk::PipelineLayoutCreateInfo layoutCreateInfo {};
    layoutCreateInfo.setLayoutCount = numSetLayout;
    layoutCreateInfo.pSetLayouts = setLayouts;
    layoutCreateInfo.pushConstantRangeCount = pushConstant ? 1 : 0;
    layoutCreateInfo.pPushConstantRanges = pushConstant;

    vk::PipelineLayout pipelineLayout = VKA(context->device.createPipelineLayout(layoutCreateInfo));

    vk::DynamicState dynamicStates[] = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo {};
    dynamicStateCreateInfo.dynamicStateCount = ARRAY_COUNT(dynamicStates);
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo {};
    pipelineCreateInfo.stageCount = ARRAY_COUNT(shaderStages);
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = subpassIndex;

    auto result = VKA(context->device.createGraphicsPipelines(nullptr, pipelineCreateInfo));
    std::vector<vk::Pipeline> pipelines = std::move(result.value);

    VK(context->device.destroyShaderModule(fragmentShaderModule));
    VK(context->device.destroyShaderModule(vertexShaderModule));

    VulkanPipeline pipeline {};
    pipeline.pipeline = pipelines.front();
    pipeline.pipelineLayout = pipelineLayout;

    return pipeline;
}

void destroyPipeline(VulkanContext* context, VulkanPipeline* pipeline) {
    VK(context->device.destroyPipeline(pipeline->pipeline));
    VK(context->device.destroyPipelineLayout(pipeline->pipelineLayout));
}

#include "utils.h"
#include "vulkan_base.h"

vk::RenderPass createRenderPass(VulkanContext* context, vk::Format format, vk::SampleCountFlagBits sampleCount) {
    vk::AttachmentDescription attachmentDescriptions [3] = {};
    attachmentDescriptions[0].format = format;
    attachmentDescriptions[0].samples = sampleCount;
    attachmentDescriptions[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachmentDescriptions[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachmentDescriptions[0].initialLayout = vk::ImageLayout::eUndefined;
    attachmentDescriptions[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

    attachmentDescriptions[1].format = vk::Format::eD32Sfloat;
    attachmentDescriptions[1].samples = sampleCount;
    attachmentDescriptions[1].loadOp = vk::AttachmentLoadOp::eClear;
    attachmentDescriptions[1].storeOp = vk::AttachmentStoreOp::eStore;
    attachmentDescriptions[1].initialLayout = vk::ImageLayout::eUndefined;
    attachmentDescriptions[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    attachmentDescriptions[2].format = format;
    attachmentDescriptions[2].samples = vk::SampleCountFlagBits::e1;
    attachmentDescriptions[2].loadOp = vk::AttachmentLoadOp::eDontCare;
    attachmentDescriptions[2].storeOp = vk::AttachmentStoreOp::eStore;
    attachmentDescriptions[2].initialLayout = vk::ImageLayout::eUndefined;
    attachmentDescriptions[2].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference attachmentReference { 0, vk::ImageLayout::eColorAttachmentOptimal };
    vk::AttachmentReference depthStencilReference = { 1, vk::ImageLayout::eDepthStencilAttachmentOptimal };
    vk::AttachmentReference resolveTargetReference = { 2, vk::ImageLayout::eColorAttachmentOptimal };
    vk::AttachmentReference postprocessTargetReference = { 2, vk::ImageLayout::eGeneral };

    vk::SubpassDescription resolveSubpassDescription {};
    resolveSubpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    resolveSubpassDescription.colorAttachmentCount = 1;
    resolveSubpassDescription.pColorAttachments = &attachmentReference;
    resolveSubpassDescription.pDepthStencilAttachment = &depthStencilReference;
    resolveSubpassDescription.pResolveAttachments = &resolveTargetReference;

    vk::SubpassDescription postprocessSubpassDescription {};
    postprocessSubpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    postprocessSubpassDescription.colorAttachmentCount = 1;
    postprocessSubpassDescription.pColorAttachments = &postprocessTargetReference;
    postprocessSubpassDescription.inputAttachmentCount = 1;
    postprocessSubpassDescription.pInputAttachments = &postprocessTargetReference;

    vk::SubpassDescription subpasses[] = {resolveSubpassDescription, postprocessSubpassDescription};

    vk::SubpassDependency readDependency{};
    readDependency.srcSubpass      = 0;
    readDependency.dstSubpass      = 1;
    readDependency.srcStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    readDependency.dstStageMask    = vk::PipelineStageFlagBits::eFragmentShader;
    readDependency.srcAccessMask   = vk::AccessFlagBits::eColorAttachmentWrite;
    readDependency.dstAccessMask   = vk::AccessFlagBits::eInputAttachmentRead;
    readDependency.dependencyFlags = vk::DependencyFlagBits::eByRegion;

    vk::SubpassDependency ext0Dependency{};
    ext0Dependency.srcSubpass      = VK_SUBPASS_EXTERNAL;
    ext0Dependency.dstSubpass      = 0;
    ext0Dependency.srcStageMask    = vk::PipelineStageFlagBits::eBottomOfPipe;
    ext0Dependency.dstStageMask    = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eColorAttachmentOutput;
    ext0Dependency.srcAccessMask   = {};
    ext0Dependency.dstAccessMask   = vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eColorAttachmentWrite;
    ext0Dependency.dependencyFlags = vk::DependencyFlagBits::eByRegion;

    vk::SubpassDependency wawDependency{};
    wawDependency.srcSubpass       = 0;
    wawDependency.dstSubpass       = 1;
    wawDependency.srcStageMask     = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    wawDependency.dstStageMask     = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    wawDependency.srcAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite;
    wawDependency.dstAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite;
    wawDependency.dependencyFlags  = vk::DependencyFlagBits::eByRegion;

    vk::SubpassDependency selfDependency{};
    selfDependency.srcSubpass      = 1;
    selfDependency.dstSubpass      = 1;
    selfDependency.srcStageMask    = vk::PipelineStageFlagBits::eFragmentShader;
    selfDependency.dstStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    selfDependency.srcAccessMask   = vk::AccessFlagBits::eInputAttachmentRead;
    selfDependency.dstAccessMask   = vk::AccessFlagBits::eColorAttachmentWrite;
    selfDependency.dependencyFlags = vk::DependencyFlagBits::eByRegion;

    vk::SubpassDependency subpassDependencies[] = { ext0Dependency, readDependency, wawDependency, selfDependency };

    vk::RenderPassCreateInfo renderPassCreateInfo {};
    renderPassCreateInfo.attachmentCount = ARRAY_COUNT(attachmentDescriptions);
    renderPassCreateInfo.pAttachments = attachmentDescriptions;
    renderPassCreateInfo.subpassCount = ARRAY_COUNT(subpasses);
    renderPassCreateInfo.pSubpasses = subpasses;
    renderPassCreateInfo.dependencyCount = ARRAY_COUNT(subpassDependencies);
    renderPassCreateInfo.pDependencies = subpassDependencies;
    vk::RenderPass result_renderpass = VKA(context->device.createRenderPass(renderPassCreateInfo, nullptr));

    return result_renderpass;
}

void destroyRenderPass(VulkanContext* context, vk::RenderPass renderPass) {
    VK(context->device.destroyRenderPass(renderPass));
}
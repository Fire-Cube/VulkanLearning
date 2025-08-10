#include "utils.h"
#include "vulkan_base.h"

vk::RenderPass createRenderPass(VulkanContext* context, vk::Format format) {
    vk::AttachmentDescription attachmentDescriptions [2] = {};
    attachmentDescriptions[0].format = format;
    attachmentDescriptions[0].samples = vk::SampleCountFlagBits::e1;
    attachmentDescriptions[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachmentDescriptions[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachmentDescriptions[0].initialLayout = vk::ImageLayout::eUndefined;
    attachmentDescriptions[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    attachmentDescriptions[1].format = vk::Format::eD32Sfloat;
    attachmentDescriptions[1].samples = vk::SampleCountFlagBits::e1;
    attachmentDescriptions[1].loadOp = vk::AttachmentLoadOp::eClear;
    attachmentDescriptions[1].storeOp = vk::AttachmentStoreOp::eStore;
    attachmentDescriptions[1].initialLayout = vk::ImageLayout::eUndefined;
    attachmentDescriptions[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference attachmentReference { 0, vk::ImageLayout::eColorAttachmentOptimal };
    vk::AttachmentReference depthStencilReference = { 1, vk::ImageLayout::eDepthStencilAttachmentOptimal };

    vk::SubpassDescription subpass {};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachmentReference;
    subpass.pDepthStencilAttachment = &depthStencilReference;

    vk::SubpassDependency dependency {};
    dependency.srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass      = 0;
    dependency.srcStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask   = {};
    dependency.dstAccessMask   = vk::AccessFlagBits::eColorAttachmentWrite;
    dependency.dependencyFlags = vk::DependencyFlagBits::eByRegion;

    vk::RenderPassCreateInfo renderPassCreateInfo {};
    renderPassCreateInfo.attachmentCount = ARRAY_COUNT(attachmentDescriptions);
    renderPassCreateInfo.pAttachments = attachmentDescriptions;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = 1;
    renderPassCreateInfo.pDependencies = &dependency;
    vk::RenderPass result_renderpass = VKA(context->device.createRenderPass(renderPassCreateInfo, nullptr));

    return result_renderpass;
}

void destroyRenderPass(VulkanContext* context, vk::RenderPass renderPass) {
    VK(context->device.destroyRenderPass(renderPass));
}
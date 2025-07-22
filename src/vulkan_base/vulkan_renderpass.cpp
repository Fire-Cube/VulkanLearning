#include "vulkan_base.h"

vk::RenderPass createRenderPass(VulkanContext* context, vk::Format format) {
    vk::AttachmentDescription attachmentDescription {};
    attachmentDescription.format = format;
    attachmentDescription.samples = vk::SampleCountFlagBits::e1;
    attachmentDescription.loadOp = vk::AttachmentLoadOp::eClear;
    attachmentDescription.storeOp = vk::AttachmentStoreOp::eStore;
    attachmentDescription.initialLayout = vk::ImageLayout::eUndefined;
    attachmentDescription.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference attachmentReference { 0, vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription subpass {};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachmentReference;

    vk::RenderPassCreateInfo renderPassCreateInfo {};
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachmentDescription;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    vk::RenderPass result_renderpass = VKA(context->device.createRenderPass(renderPassCreateInfo, nullptr));

    return result_renderpass;
}

void destroyRenderPass(VulkanContext* context, vk::RenderPass renderPass) {
    VK(context->device.destroyRenderPass(renderPass));
}
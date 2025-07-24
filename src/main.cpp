#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "logger.h"
#include "utils.h"
#include "vulkan_base/vulkan_base.h"

Logger globalLogger("VulkanLearning.log");

VulkanContext* context = nullptr;
VkSurfaceKHR surface;
VulkanSwapchain swapchain;
vk::RenderPass renderPass;
std::vector<vk::Framebuffer> framebuffers;
VulkanPipeline pipeline;
vk::CommandPool commandPool;
vk::CommandBuffer commandBuffer;
vk::Fence fence;
vk::Semaphore acquireSemaphore;
vk::Semaphore releaseSemaphore;

bool handleMessage() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_EVENT_QUIT:
			return false;
		}
	}
	return true;
}

void initApplication(SDL_Window* window) {
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

	u32 instanceExtensionCount = 0;
	const char *const *enabledInstanceExtensions = SDL_Vulkan_GetInstanceExtensions(&instanceExtensionCount);
	LOG_DEBUG("Vulkan Instance Extensions: " + std::to_string(instanceExtensionCount));
	LOG_DEBUG(utils::join(enabledInstanceExtensions, instanceExtensionCount));

	const char* enableDeviceExtensions[] { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	initVulkan(context, instanceExtensionCount, enabledInstanceExtensions, ARRAY_COUNT(enableDeviceExtensions), enableDeviceExtensions);

	SDL_Vulkan_CreateSurface(window, context->instance, nullptr, &surface);
	swapchain = createSwapchain(context, surface, vk::ImageUsageFlagBits::eColorAttachment);
	renderPass = createRenderPass(context, swapchain.format);

	framebuffers.resize(swapchain.images.size());
	for (u32 i = 0; i < swapchain.images.size(); i++) {
		vk::FramebufferCreateInfo framebufferCreateInfo {};
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = &swapchain.imageViews[i];
		framebufferCreateInfo.width = swapchain.width;
		framebufferCreateInfo.height = swapchain.height;
		framebufferCreateInfo.layers = 1;

		framebuffers[i] = VKA(context->device.createFramebuffer(framebufferCreateInfo));
	}

	pipeline = createPipeline(context, "shaders/triangle.vert.spv", "shaders/triangle.frag.spv", renderPass, swapchain.width, swapchain.height);

	vk::FenceCreateInfo fenceCreateInfo {};
	fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

	fence = VKA(context->device.createFence(fenceCreateInfo));

	vk::SemaphoreCreateInfo semaphoreCreateInfo {};
	acquireSemaphore = VKA(context->device.createSemaphore(semaphoreCreateInfo));
	releaseSemaphore = VKA(context->device.createSemaphore(semaphoreCreateInfo));

	vk::CommandPoolCreateInfo commandPoolCreateInfo {};
	commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
	commandPoolCreateInfo.queueFamilyIndex = context->graphicsQueue.familyIndex;

	commandPool = VKA(context->device.createCommandPool(commandPoolCreateInfo));

	vk::CommandBufferAllocateInfo commandBufferAllocateInfo {};
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
	commandBufferAllocateInfo.commandBufferCount = 1;

	auto commandBuffers = VKA(context->device.allocateCommandBuffers(commandBufferAllocateInfo));
	commandBuffer = commandBuffers.front();
}

void renderApplication() {
	VKA(context->device.waitForFences(fence, true, UINT64_MAX));
	VKA(context->device.resetFences(fence));

	u32 imageIndex = VK(context->device.acquireNextImageKHR(swapchain.swapchain, UINT64_MAX, acquireSemaphore, nullptr).value);

	VKA(context->device.resetCommandPool(commandPool));

	vk::CommandBufferBeginInfo commandBufferBeginInfo {};
	commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	VKA(commandBuffer.begin(commandBufferBeginInfo));

	vk::ClearValue clearValue = vk::ClearColorValue{1.0f, 0.0f, 1.0f, 1.0f};

	vk::RenderPassBeginInfo renderPassBeginInfo {};
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = framebuffers[imageIndex];
	renderPassBeginInfo.renderArea = vk::Rect2D( {0, 0}, {swapchain.width, swapchain.height} );
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearValue;

	commandBuffer.beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.pipeline);
	commandBuffer.draw(3, 1, 0, 0);

	commandBuffer.endRenderPass();

	VKA(commandBuffer.end());

	vk::PipelineStageFlags stageFlags {vk::PipelineStageFlagBits::eColorAttachmentOutput};

	vk::SubmitInfo submitInfo {};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &acquireSemaphore;
	submitInfo.pWaitDstStageMask = &stageFlags;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &releaseSemaphore;

	VKA(context->graphicsQueue.queue.submit(submitInfo, fence));

	vk::PresentInfoKHR presentInfo {};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain.swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &releaseSemaphore;

	VKA(context->graphicsQueue.queue.presentKHR(presentInfo));

}

void cleanupApplication() {
	VKA(context->device.waitIdle());

	VK(context->device.destroyFence(fence));
	VK(context->device.destroyCommandPool(commandPool));

	destroyPipeline(context, &pipeline);

	for (auto& framebuffer : framebuffers) {
		context->device.destroyFramebuffer(framebuffer);
	}
	framebuffers.clear();

	destroyRenderPass(context, renderPass);
	destroySwapchain(context, &swapchain);

	context->instance.destroySurfaceKHR(surface);
	exitVulkan(context);
}

int main() {
	LOG_INFO("--- Program started ---");
	LOG_INFO("Start time: " + utils::getCurrentTimeFormatted());

	if (!SDL_Init(SDL_INIT_VIDEO)) { // SDL returns true on success
		LOG_ERROR("Error initializing SDL: " + std::string(SDL_GetError()));
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("VulkanLearning", 1240, 720, SDL_WINDOW_VULKAN);
	if (!window) {
		LOG_ERROR("Error creating window: " + std::string(SDL_GetError()));
		return 1;
	}

	VulkanContext ctx;
	context = &ctx;

	initApplication(window);
	while (handleMessage()) {
		renderApplication();
	}

	cleanupApplication();

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

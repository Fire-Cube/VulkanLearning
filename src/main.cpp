#define SDL_MAIN_HANDLED

#include <chrono>
#include <thread>

#include <windows.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_system.h>

#include "logger.h"
#include "utils.h"
#include "vulkan_base/vulkan_base.h"

Logger globalLogger("VulkanLearning.log");

#define FRAMES_IN_FLIGHT 2

VulkanContext* context = nullptr;
VkSurfaceKHR surface;
VulkanSwapchain swapchain;
vk::RenderPass renderPass;
std::vector<vk::Framebuffer> framebuffers;
VulkanPipeline pipeline;
vk::CommandPool commandPools[FRAMES_IN_FLIGHT];
vk::CommandBuffer commandBuffers[FRAMES_IN_FLIGHT];
vk::Fence fences[FRAMES_IN_FLIGHT];
vk::Semaphore acquireSemaphores[FRAMES_IN_FLIGHT];
vk::Semaphore releaseSemaphores[FRAMES_IN_FLIGHT];

bool windowResized = false;
bool windowMinimized = false;

void recreateRenderPass() {
	if (renderPass) {
		for (auto& framebuffer : framebuffers) {
			context->device.destroyFramebuffer(framebuffer);
		}
		framebuffers.clear();

		destroyRenderPass(context, renderPass);
	}

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
}

void recreateSwapchain() {
	VulkanSwapchain oldSwapchain = swapchain;

	VKA(context->device.waitIdle());

	swapchain = createSwapchain(context, surface, vk::ImageUsageFlagBits::eColorAttachment, &oldSwapchain);

	destroySwapchain(context, &oldSwapchain);
	recreateRenderPass();
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

	recreateRenderPass();

	pipeline = createPipeline(context, "shaders/triangle.vert.spv", "shaders/triangle.frag.spv", renderPass, swapchain.width, swapchain.height);

	for (auto &fence : fences) {
		vk::FenceCreateInfo fenceCreateInfo {};
		fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

		fence = VKA(context->device.createFence(fenceCreateInfo));
	}

	for (auto &acquireSemaphore : acquireSemaphores) {
		vk::SemaphoreCreateInfo semaphoreCreateInfo {};
		acquireSemaphore = VKA(context->device.createSemaphore(semaphoreCreateInfo));
	}

	for (auto &releaseSemaphore : releaseSemaphores) {
		vk::SemaphoreCreateInfo semaphoreCreateInfo {};
		releaseSemaphore = VKA(context->device.createSemaphore(semaphoreCreateInfo));
	}

	for (auto &commandPool : commandPools) {
		vk::CommandPoolCreateInfo commandPoolCreateInfo {};
		commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
		commandPoolCreateInfo.queueFamilyIndex = context->graphicsQueue.familyIndex;

		commandPool = VKA(context->device.createCommandPool(commandPoolCreateInfo));
	}

	for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i) {
		vk::CommandBufferAllocateInfo commandBufferAllocateInfo {};
		commandBufferAllocateInfo.commandPool = commandPools[i];
		commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
		commandBufferAllocateInfo.commandBufferCount = 1;

		auto commandBuffersCreated = VKA(context->device.allocateCommandBuffers(commandBufferAllocateInfo));
		commandBuffers[i] = commandBuffersCreated.front();
	}
}

void renderApplication() {
	static u32 frameIndex = 0;

	vk::SurfaceCapabilitiesKHR surfaceCapabilities = VKA(context->physicalDevice.getSurfaceCapabilitiesKHR(surface));
	if (windowMinimized || (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return;
	}

	if (windowResized) {
		recreateSwapchain();
		windowResized = false;
	}

	VKA(context->device.waitForFences(fences[frameIndex], true, UINT64_MAX));
	VKA(context->device.resetFences(fences[frameIndex]));

	u32 imageIndex = VK(context->device.acquireNextImageKHR(swapchain.swapchain, UINT64_MAX, acquireSemaphores[frameIndex], nullptr).value);

	VKA(context->device.resetCommandPool(commandPools[frameIndex]));

	{
		auto commandBuffer = commandBuffers[frameIndex];

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

		vk::Viewport viewport { 0.0f, 0.0f, static_cast<float>(swapchain.width), static_cast<float>(swapchain.height)};
		vk::Rect2D scissor { {0, 0}, {swapchain.width, swapchain.height} } ;

		commandBuffer.setViewport(0, 1, &viewport);
		commandBuffer.setScissor(0, 1, &scissor);
		commandBuffer.draw(3, 1, 0, 0);

		commandBuffer.endRenderPass();

		VKA(commandBuffer.end());
	}

	vk::PipelineStageFlags stageFlags {vk::PipelineStageFlagBits::eColorAttachmentOutput};

	vk::SubmitInfo submitInfo {};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[frameIndex];
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &acquireSemaphores[frameIndex];
	submitInfo.pWaitDstStageMask = &stageFlags;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &releaseSemaphores[frameIndex];

	VKA(context->graphicsQueue.queue.submit(submitInfo, fences[frameIndex]));

	vk::PresentInfoKHR presentInfo {};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain.swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &releaseSemaphores[frameIndex];

	VKA(context->graphicsQueue.queue.presentKHR(presentInfo));

	frameIndex = (frameIndex + 1) % FRAMES_IN_FLIGHT;

}

void cleanupApplication() {
	VKA(context->device.waitIdle());

	for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i) {
		VK(context->device.destroyFence(fences[i]));
		VK(context->device.destroySemaphore(acquireSemaphores[i]));
		VK(context->device.destroySemaphore(releaseSemaphores[i]));
	}

	for (auto &commandPool : commandPools) {
		VK(context->device.destroyCommandPool(commandPool));
	}

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

bool handleMessage() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_EVENT_QUIT:
			return false;

		case SDL_EVENT_WINDOW_RESIZED:
			windowResized = true;
			break;

		case SDL_EVENT_WINDOW_MINIMIZED:
			windowMinimized = true;
			break;

		case SDL_EVENT_WINDOW_RESTORED:
			windowMinimized = false;
			windowResized = true;
			break;

		default: ;
		}
	}
	return true;
}

bool SDLCALL winMessageHook(void *userdata, MSG *msg) {
	if (msg->message == WM_SIZING) {
		windowResized = true;
		renderApplication();
	}
	return true;
}

int main() {
	LOG_INFO("--- Program started ---");
	LOG_INFO("Start time: " + utils::getCurrentTimeFormatted());

	if (!SDL_Init(SDL_INIT_VIDEO)) { // SDL returns true on success
		LOG_ERROR("Error initializing SDL: " + std::string(SDL_GetError()));
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("VulkanLearning", 1240, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!window) {
		LOG_ERROR("Error creating window: " + std::string(SDL_GetError()));
		return 1;
	}

	SDL_SetWindowsMessageHook(winMessageHook, window);

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

#define SDL_MAIN_HANDLED
#define CRT_SECURE_NO_WARNINGS

#include <ctime>
#include <iomanip>
#include <sstream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "logger.h"
#include "utils.h"
#include "vulkan_base/vulkan_base.h"

Logger globalLogger("VulkanLearning.log");

std::string getCurrentTimeFormatted() {
	std::time_t now = std::time(nullptr);
	std::tm* localTime = std::localtime(&now);
	std::ostringstream oss;
	oss << std::put_time(localTime, "%Y-%m-%d %H:%M:%S");
	return oss.str();
}

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

int main() {
	LOG_INFO("--- Program started ---");
	LOG_INFO("Start time: " + getCurrentTimeFormatted());

	if (!SDL_Init(SDL_INIT_VIDEO)) { // SDL returns true on success
		LOG_ERROR("Error initializing SDL: " + std::string(SDL_GetError()));
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("VulkanLearning", 1240, 720, SDL_WINDOW_VULKAN);
	if (!window) {
		LOG_ERROR("Error creating window: " + std::string(SDL_GetError()));
		return 1;
	}

	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

	u32 instanceExtensionCount = 0;
	const char *const *enabledInstanceExtensions = SDL_Vulkan_GetInstanceExtensions(&instanceExtensionCount);
	LOG_DEBUG("Vulkan Instance Extensions: " + std::to_string(instanceExtensionCount));
	LOG_DEBUG(utils::join(enabledInstanceExtensions, instanceExtensionCount));

	const char* enableDeviceExtensions[] { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	auto context = initVulkan(instanceExtensionCount, enabledInstanceExtensions, ARRAY_COUNT(enableDeviceExtensions), enableDeviceExtensions);

	VkSurfaceKHR surface;
	SDL_Vulkan_CreateSurface(window, context->instance, nullptr, &surface);
	VulkanSwapchain swapchain = createSwapchain(context.get(), surface, vk::ImageUsageFlagBits::eColorAttachment);
	vk::RenderPass renderPass = createRenderPass(context.get(), swapchain.format);

	std::vector<vk::Framebuffer> framebuffers;
	framebuffers.resize(swapchain.images.size());
	for (u32 i = 0; i < swapchain.images.size(); i++) {
		vk::FramebufferCreateInfo framebufferCreateInfo {};
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = &swapchain.imageViews[i];
		framebufferCreateInfo.width = swapchain.width;
		framebufferCreateInfo.height = swapchain.height;
		framebufferCreateInfo.layers = 1;

		VKA(framebuffers[i] = context->device.createFramebuffer(framebufferCreateInfo));
	}

	while (handleMessage()) {

	}

	VKA(context->device.waitIdle());

	for (auto& framebuffer : framebuffers) {
		context->device.destroyFramebuffer(framebuffer);
	}
	framebuffers.clear();

	destroyRenderPass(context.get(), renderPass);
	destroySwapchain(context.get(), &swapchain);

	context->instance.destroySurfaceKHR(surface);
	exitVulkan(context.get());

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

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

	if (SDL_Init(SDL_INIT_VIDEO) != true) { // SDL returns true on success
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

	LOG_INFO("Initializing Vulkan instance");
	auto context = initVulkan(instanceExtensionCount, enabledInstanceExtensions, 0, nullptr);
	LOG_DEBUG("Vulkan instance initialized");

	while (handleMessage()) {

	}

	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}

#define SDL_MAIN_HANDLED
#define STB_IMAGE_IMPLEMENTATION

#include <chrono>
#include <thread>

#include <windows.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_system.h>

#include <stb_image.h>

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
VulkanBuffer vertexBuffer;
VulkanBuffer indexBuffer;
VulkanImage image;

vk::Sampler sampler;
vk::DescriptorPool descriptorPool;
vk::DescriptorSet descriptorSet;
vk::DescriptorSetLayout descriptorSetLayout;

bool windowResized = false;
bool windowMinimized = false;

float vertexData[] = {
	0.5f, -0.5f,       // Position
	1.0f, 0.0f, 0.0f,  // Color
	1.0f, 0.0f,        // Texture coordinate

	0.5f, 0.5f,
	0.0f, 1.0f, 0.0f,
	1.0f, 1.0f,

	-0.5f, 0.5f,
	0.0f, 0.0f, 1.0f,
	0.0f, 1.0f,

	-0.5f, -0.5f,
	0.0f, 1.0f, 0.0f,
	0.0f, 0.0f,
};

u32 indexData[] = {
	0, 1, 2,
	3, 0, 2
};

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

	u32 sdlExtensionCount = 0;
	const char *const *sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

	std::vector<const char*> enabledInstanceExtensions;
	enabledInstanceExtensions.reserve(sdlExtensionCount + 2);
	enabledInstanceExtensions.insert(
			enabledInstanceExtensions.end(),
			sdlExtensions,
			sdlExtensions + sdlExtensionCount
		);

#ifdef DEBUG_BUILD
	enabledInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	enabledInstanceExtensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
#endif

	u32 instanceExtensionsCount = enabledInstanceExtensions.size();

	LOG_DEBUG("Vulkan Instance Extensions: " + std::to_string(instanceExtensionsCount));
	LOG_DEBUG(utils::join(enabledInstanceExtensions.data(), instanceExtensionsCount));

	const char* enableDeviceExtensions[] { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	initVulkan(context, instanceExtensionsCount, enabledInstanceExtensions.data(), ARRAY_COUNT(enableDeviceExtensions), enableDeviceExtensions);

	SDL_Vulkan_CreateSurface(window, context->instance, nullptr, &surface);
	swapchain = createSwapchain(context, surface, vk::ImageUsageFlagBits::eColorAttachment);
	renderPass = createRenderPass(context, swapchain.format);

	recreateRenderPass();

	vk::SamplerCreateInfo samplerCreateInfo {};
	samplerCreateInfo.magFilter = vk::Filter::eNearest;
	samplerCreateInfo.minFilter = vk::Filter::eNearest;
	samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
	samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samplerCreateInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 1.0f;

	sampler = VKA(context->device.createSampler(samplerCreateInfo));

	int width, height, channels;
	uint8_t* data = stbi_load("data/images/FireCube.png", &width, &height, &channels, STBI_rgb_alpha);
	if (!data) {
		LOG_ERROR("Failed to load image");
		assert(false);
	}

	createImage(context, &image, static_cast<u32>(width), static_cast<u32>(height), vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
	uploadDataToImage(context, &image, data, static_cast<u32>(width * height * STBI_rgb_alpha), static_cast<u32>(width), static_cast<u32>(height), vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone);
	stbi_image_free(data);

	vk::DescriptorPoolSize poolSizes[] = {
		{vk::DescriptorType::eCombinedImageSampler, 1}
	};

	vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {};
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = ARRAY_COUNT(poolSizes);
	descriptorPoolCreateInfo.pPoolSizes = poolSizes;

	descriptorPool = VKA(context->device.createDescriptorPool(descriptorPoolCreateInfo));

	vk::DescriptorSetLayoutBinding bindings[] = {
		{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr},
	};

	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
	descriptorSetLayoutCreateInfo.bindingCount = ARRAY_COUNT(bindings);
	descriptorSetLayoutCreateInfo.pBindings = bindings;

	descriptorSetLayout = VKA(context->device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo));

	vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {};
	descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

	descriptorSet = VKA(context->device.allocateDescriptorSets(descriptorSetAllocateInfo)).front();

	vk::DescriptorImageInfo descriptorImageInfo = { sampler, image.imageView, vk::ImageLayout::eReadOnlyOptimal};

	vk::WriteDescriptorSet descriptorWrites [1];
	descriptorWrites[0].dstSet = descriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
	descriptorWrites[0].pImageInfo = &descriptorImageInfo;

	VK(context->device.updateDescriptorSets(ARRAY_COUNT(descriptorWrites), descriptorWrites, 0, nullptr));

	vk::VertexInputAttributeDescription vertexAttributeDescriptions[3];
	vertexAttributeDescriptions[0].binding = 0;
	vertexAttributeDescriptions[0].location = 0;
	vertexAttributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
	vertexAttributeDescriptions[0].offset = 0;

	vertexAttributeDescriptions[1].binding = 0;
	vertexAttributeDescriptions[1].location = 1;
	vertexAttributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
	vertexAttributeDescriptions[1].offset = sizeof(float) * 2;

	vertexAttributeDescriptions[2].binding = 0;
	vertexAttributeDescriptions[2].location = 2;
	vertexAttributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
	vertexAttributeDescriptions[2].offset = sizeof(float) * 5;

	vk::VertexInputBindingDescription vertexInputBindingDescription {};
	vertexInputBindingDescription.binding = 0;
	vertexInputBindingDescription.inputRate = vk::VertexInputRate::eVertex;
	vertexInputBindingDescription.stride = sizeof(float) * 7;

	pipeline = createPipeline(context, "shaders/texture.vert.spv", "shaders/texture.frag.spv", renderPass, swapchain.width, swapchain.height,
								vertexAttributeDescriptions, ARRAY_COUNT(vertexAttributeDescriptions), &vertexInputBindingDescription, 1, &descriptorSetLayout);

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

	// vertex buffer
	createBuffer(context, &vertexBuffer, sizeof(vertexData), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
	uploadDataToBuffer(context, &vertexBuffer, vertexData, sizeof(vertexData));

	// index buffer
	createBuffer(context, &indexBuffer, sizeof(indexData), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
	uploadDataToBuffer(context, &indexBuffer, indexData, sizeof(indexData));
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

		vk::Viewport viewport { 0.0f, 0.0f, static_cast<float>(swapchain.width), static_cast<float>(swapchain.height)};
		vk::Rect2D scissor { {0, 0}, {swapchain.width, swapchain.height} } ;

		vk::DeviceSize offset = 0;

		commandBuffer.beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.pipeline);
		commandBuffer.setViewport(0, 1, &viewport);
		commandBuffer.setScissor(0, 1, &scissor);
		commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer.buffer, &offset);
		commandBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		commandBuffer.drawIndexed(ARRAY_COUNT(indexData), 1, 0, 0, 0);

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

	destroyBuffer(context, &vertexBuffer);
	destroyBuffer(context, &indexBuffer);

	destroyImage(context, &image);
	context->device.destroySampler(sampler);
	context->device.destroyDescriptorSetLayout(descriptorSetLayout);
	context->device.destroyDescriptorPool(descriptorPool);

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

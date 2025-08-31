#define SDL_MAIN_HANDLED
#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <chrono>
#include <thread>

#include <windows.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_system.h>

#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>

#include "cgltf.h"
#include "logger.h"
#include "utils.h"
#include "model.h"

#include "vulkan_base/vulkan_base.h"

Logger globalLogger("VulkanLearning.log");

#define FRAMES_IN_FLIGHT 2

auto msaaSamples = vk::SampleCountFlagBits::e8;

VulkanContext* context = nullptr;
VkSurfaceKHR surface;
VulkanSwapchain swapchain;
vk::RenderPass renderPass;
std::vector<VulkanImage> depthBuffers;
std::vector<VulkanImage> colorBuffers;
std::vector<vk::Framebuffer> framebuffers;
vk::CommandPool commandPools[FRAMES_IN_FLIGHT];
vk::CommandBuffer commandBuffers[FRAMES_IN_FLIGHT];
vk::Fence fences[FRAMES_IN_FLIGHT];
vk::Semaphore acquireSemaphores[FRAMES_IN_FLIGHT];
vk::Semaphore releaseSemaphores[FRAMES_IN_FLIGHT];

vk::Sampler sampler;
VulkanImage image;

vk::DescriptorPool spriteDescriptorPool;
vk::DescriptorSet spriteDescriptorSet;
vk::DescriptorSetLayout spriteDescriptorSetLayout;
VulkanPipeline spritePipeline;
VulkanBuffer spriteVertexBuffer;
VulkanBuffer spriteIndexBuffer;

Model model;
VulkanPipeline modelPipeline;
vk::DescriptorSetLayout modelDescriptorSetLayout;
vk::DescriptorPool modelDescriptorPool;
vk::DescriptorSet modelDescriptorSets[FRAMES_IN_FLIGHT];
VulkanBuffer modelUniformBuffers[FRAMES_IN_FLIGHT];

vk::DescriptorPool imguiDescriptorPool;

struct Camera {
	glm::vec3 position;
	glm::vec3 direction;
	glm::vec3 up;
	float yaw;
	float pitch;
	glm::mat4 viewProjection;
	glm::mat4 view;
	glm::mat4 projection;
} camera;

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
		for (auto& depthbuffer : depthBuffers) {
			destroyImage(context, &depthbuffer);
		}
		destroyRenderPass(context, renderPass);
		for (auto& colorbuffer : colorBuffers) {
			destroyImage(context, &colorbuffer);
		}
	}
	framebuffers.clear();
	depthBuffers.clear();
	colorBuffers.clear();

	renderPass = createRenderPass(context, swapchain.format, msaaSamples);

	framebuffers.resize(swapchain.images.size());
	depthBuffers.resize(swapchain.images.size());
	colorBuffers.resize(swapchain.images.size());

	for (u32 i = 0; i < swapchain.images.size(); i++) {
		createImage(context, &depthBuffers.data()[i], swapchain.width, swapchain.height, vk::Format::eD32Sfloat, vk::ImageUsageFlagBits::eDepthStencilAttachment, msaaSamples);
		createImage(context, &colorBuffers.data()[i], swapchain.width, swapchain.height, swapchain.format, vk::ImageUsageFlagBits::eColorAttachment, msaaSamples);

		vk::ImageView attachments[3] = {
			colorBuffers[i].imageView,
			depthBuffers[i].imageView,
			swapchain.imageViews[i]
		};
		vk::FramebufferCreateInfo framebufferCreateInfo {};
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = ARRAY_COUNT(attachments);
		framebufferCreateInfo.pAttachments = attachments;
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
	renderPass = createRenderPass(context, swapchain.format, msaaSamples);

	recreateRenderPass();

	model = createModel(context, "data/models/BoomBox.glb", "data/models", cgltf_component_type_r_16u);

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

	{
		vk::DescriptorPoolSize poolSizes[] = {
			{vk::DescriptorType::eCombinedImageSampler, 1}
		};

		vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {};
		descriptorPoolCreateInfo.maxSets = 1;
		descriptorPoolCreateInfo.poolSizeCount = ARRAY_COUNT(poolSizes);
		descriptorPoolCreateInfo.pPoolSizes = poolSizes;

		spriteDescriptorPool = VKA(context->device.createDescriptorPool(descriptorPoolCreateInfo));

		vk::DescriptorSetLayoutBinding bindings[] = {
			{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr},
		};

		vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
		descriptorSetLayoutCreateInfo.bindingCount = ARRAY_COUNT(bindings);
		descriptorSetLayoutCreateInfo.pBindings = bindings;

		spriteDescriptorSetLayout = VKA(context->device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo));

		vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {};
		descriptorSetAllocateInfo.descriptorPool = spriteDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &spriteDescriptorSetLayout;

		spriteDescriptorSet = VKA(context->device.allocateDescriptorSets(descriptorSetAllocateInfo)).front();

		vk::DescriptorImageInfo descriptorImageInfo = { sampler, image.imageView, vk::ImageLayout::eShaderReadOnlyOptimal};

		vk::WriteDescriptorSet descriptorWrites [1];
		descriptorWrites[0].dstSet = spriteDescriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[0].pImageInfo = &descriptorImageInfo;

		VK(context->device.updateDescriptorSets(ARRAY_COUNT(descriptorWrites), descriptorWrites, 0, nullptr));
	}

	{
		for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i) {
			createBuffer(context, &modelUniformBuffers[i], sizeof(glm::mat4) * 2, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		}
		vk::DescriptorPoolSize poolSizes[] = {
			{ vk::DescriptorType::eUniformBuffer, FRAMES_IN_FLIGHT },
			{ vk::DescriptorType::eCombinedImageSampler, FRAMES_IN_FLIGHT }
		};

		vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {};
		descriptorPoolCreateInfo.maxSets = FRAMES_IN_FLIGHT;
		descriptorPoolCreateInfo.poolSizeCount = ARRAY_COUNT(poolSizes);
		descriptorPoolCreateInfo.pPoolSizes = poolSizes;

		modelDescriptorPool = VKA(context->device.createDescriptorPool(descriptorPoolCreateInfo));

		vk::DescriptorSetLayoutBinding bindings[] = {
			{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr },
			{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, &sampler }
		};

		vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
		descriptorSetLayoutCreateInfo.bindingCount = ARRAY_COUNT(bindings);
		descriptorSetLayoutCreateInfo.pBindings = bindings;

		modelDescriptorSetLayout = VKA(context->device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo));

		for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i) {
			vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {};
			descriptorSetAllocateInfo.descriptorPool = modelDescriptorPool;
			descriptorSetAllocateInfo.descriptorSetCount = 1;
			descriptorSetAllocateInfo.pSetLayouts = &modelDescriptorSetLayout;

			modelDescriptorSets[i] = VKA(context->device.allocateDescriptorSets(descriptorSetAllocateInfo)).front();

			vk::DescriptorBufferInfo descriptorBufferInfo = { modelUniformBuffers[i].buffer, 0, sizeof(glm::mat4) * 2 };
			vk::DescriptorImageInfo descriptorImageInfo = { sampler, model.albedoTexture.imageView, vk::ImageLayout::eShaderReadOnlyOptimal };

			vk::WriteDescriptorSet descriptorWrites [2];
			descriptorWrites[0].dstSet = modelDescriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
			descriptorWrites[0].pBufferInfo = &descriptorBufferInfo;

			descriptorWrites[1].dstSet = modelDescriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
			descriptorWrites[1].pImageInfo = &descriptorImageInfo;

			VK(context->device.updateDescriptorSets(ARRAY_COUNT(descriptorWrites), descriptorWrites, 0, nullptr));
		}
	}

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

	vk::VertexInputAttributeDescription modelAttributeDescriptions[3];
	modelAttributeDescriptions[0].binding = 0;
	modelAttributeDescriptions[0].location = 0;
	modelAttributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
	modelAttributeDescriptions[0].offset = 0;

	modelAttributeDescriptions[1].binding = 0;
	modelAttributeDescriptions[1].location = 1;
	modelAttributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
	modelAttributeDescriptions[1].offset = sizeof(float) * 3;

	modelAttributeDescriptions[2].binding = 0;
	modelAttributeDescriptions[2].location = 2;
	modelAttributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
	modelAttributeDescriptions[2].offset = sizeof(float) * 6;

	vk::VertexInputBindingDescription modelInputBindingDescription {};
	modelInputBindingDescription.binding = 0;
	modelInputBindingDescription.inputRate = vk::VertexInputRate::eVertex;
	modelInputBindingDescription.stride = sizeof(float) * 8;

	vk::PushConstantRange pushConstant {};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(glm::mat4);
	pushConstant.stageFlags = vk::ShaderStageFlagBits::eVertex;

	spritePipeline = createPipeline(context, "shaders/texture.vert.spv", "shaders/texture.frag.spv", renderPass, swapchain.width, swapchain.height,
									vertexAttributeDescriptions, ARRAY_COUNT(vertexAttributeDescriptions), &vertexInputBindingDescription, 1, &spriteDescriptorSetLayout, nullptr, msaaSamples);
	modelPipeline = createPipeline(context, "shaders/model.vert.spv", "shaders/model.frag.spv", renderPass, swapchain.width, swapchain.height,
									modelAttributeDescriptions, ARRAY_COUNT(modelAttributeDescriptions), &modelInputBindingDescription, 1, &modelDescriptorSetLayout, nullptr, msaaSamples);
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
	createBuffer(context, &spriteVertexBuffer, sizeof(vertexData), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
	uploadDataToBuffer(context, &spriteVertexBuffer, vertexData, sizeof(vertexData));

	// index buffer
	createBuffer(context, &spriteIndexBuffer, sizeof(indexData), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
	uploadDataToBuffer(context, &spriteIndexBuffer, indexData, sizeof(indexData));

	// Init camera

	camera.position = glm::vec3(0.0f);
	camera.direction = glm::vec3(0.0f, 0.0f, 1.0f);
	camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
	camera.yaw = 0.0f;
	camera.pitch = 0.0f;

	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForVulkan(window);

	ImGui_ImplVulkan_InitInfo imguiInitInfo {};
	imguiInitInfo.Instance = context->instance;
	imguiInitInfo.PhysicalDevice = context->physicalDevice;
	imguiInitInfo.Device = context->device;
	imguiInitInfo.QueueFamily = context->graphicsQueue.familyIndex;
	imguiInitInfo.Queue = context->graphicsQueue.queue;
	imguiInitInfo.DescriptorPool = nullptr;
	imguiInitInfo.DescriptorPoolSize = 1000;
	imguiInitInfo.RenderPass = renderPass;
	imguiInitInfo.MinImageCount = 2;
	imguiInitInfo.ImageCount = swapchain.images.size();
	imguiInitInfo.MSAASamples = static_cast<VkSampleCountFlagBits>(msaaSamples);

	ImGui_ImplVulkan_Init(&imguiInitInfo);

}

void renderApplication() {
	static u32 frameIndex = 0;
	static float time = 0.0f;
	time += 0.005f;

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

		vk::ClearValue clearValues[2] = {
			vk::ClearColorValue{0.5f, 0.0f, 0.5f, 1.0f},
			vk::ClearDepthStencilValue{0.0f, 0}
		};

		vk::RenderPassBeginInfo renderPassBeginInfo {};
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = framebuffers[imageIndex];
		renderPassBeginInfo.renderArea = vk::Rect2D( {0, 0}, {swapchain.width, swapchain.height} );
		renderPassBeginInfo.clearValueCount = ARRAY_COUNT(clearValues);
		renderPassBeginInfo.pClearValues = clearValues;

		vk::Viewport viewport { 0.0f, 0.0f, static_cast<float>(swapchain.width), static_cast<float>(swapchain.height), 0.0f, 1.0f};
		vk::Rect2D scissor { {0, 0}, {swapchain.width, swapchain.height} } ;

		vk::DeviceSize offset = 0;

		glm::mat4 tranlationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 5.0f));
		glm::mat4 scalingMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(100.0f));
		glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), -time, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 modelMatrix = tranlationMatrix * scalingMatrix * rotationMatrix;

		// glm::mat4 projectionMatrix = glm::ortho(0.0f, static_cast<float>(swapchain.width), 0.0f, static_cast<float>(swapchain.height), 0.0f, 1.0f);
		// glm::mat4 projectionMatrix = utils::getProjectionInverseZ(glm::radians(90.0f), swapchain.width, swapchain.height, 0.01f);
		glm::mat4 modelViewProjection = camera.viewProjection * modelMatrix;
		glm::mat4 modelView = camera.view * modelMatrix;

		commandBuffer.beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);

		commandBuffer.setViewport(0, 1, &viewport);
		commandBuffer.setScissor(0, 1, &scissor);

		// commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, spritePipeline.pipeline);
		// commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer.buffer, &offset);
		// commandBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
		// commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, spritePipeline.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		// commandBuffer.drawIndexed(ARRAY_COUNT(indexData), 1, 0, 0, 0);

		void* mapped;
		VK(context->device.mapMemory(modelUniformBuffers[frameIndex].memory, 0, sizeof(glm::mat4) * 2, {}, &mapped));
		memcpy(mapped, &modelViewProjection, sizeof(modelViewProjection));
		memcpy(static_cast<u8*>(mapped) + sizeof(glm::mat4), &modelView, sizeof(modelView));
		VK(context->device.unmapMemory((modelUniformBuffers[frameIndex].memory)));

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, modelPipeline.pipeline);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, modelPipeline.pipelineLayout, 0, 1, &modelDescriptorSets[frameIndex], 0, nullptr);
		commandBuffer.bindVertexBuffers(0, 1, &model.vertexBuffer.buffer, &offset);
		commandBuffer.bindIndexBuffer(model.indexBuffer.buffer, 0, vk::IndexType::eUint16);
		commandBuffer.drawIndexed(model.numIndices, 1, 0, 0, 0);

		// Imgui
		ImGui::Render();
		ImDrawData* drawData = ImGui::GetDrawData();
		ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffers[frameIndex]);

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

void updateApplication(SDL_Window* window, float delta) {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	SDL_PumpEvents();

	const bool* keys = SDL_GetKeyboardState(nullptr);
	float mouseX, mouseY;
	SDL_GetRelativeMouseState(&mouseX, &mouseY);

	if (SDL_GetWindowRelativeMouseMode(window)) {
		float cameraSpeed = 5.0f;
		float mouseSensitivity = 0.27f;

		if (keys[SDL_SCANCODE_W]) {
			camera.position += glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f) * camera.direction) * delta * cameraSpeed;
		}
		if (keys[SDL_SCANCODE_S]) {
			camera.position -= glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f) * camera.direction) * delta * cameraSpeed;
		}
		if (keys[SDL_SCANCODE_A]) {
			camera.position += glm::normalize(glm::cross(camera.direction, camera.up)) * delta * cameraSpeed;
		}
		if (keys[SDL_SCANCODE_D]) {
			camera.position -= glm::normalize(glm::cross(camera.direction, camera.up)) * delta * cameraSpeed;
		}
		if (keys[SDL_SCANCODE_SPACE]) {
			camera.position += glm::normalize(camera.up) * delta * cameraSpeed;
		}
		if (keys[SDL_SCANCODE_LSHIFT]) {
			camera.position -= glm::normalize(camera.up) * delta * cameraSpeed;
		}

		camera.yaw += mouseX * mouseSensitivity;
		camera.pitch -= mouseY * mouseSensitivity;
	}

	if (camera.pitch > 89.0f) {
		camera.pitch = 89.0f;
	}
	if (camera.pitch < -89.0f) {
		camera.pitch = -89.0f;
	}

	glm::vec3 front;

	front.x = cos(glm::radians(camera.pitch)) * sin(glm::radians(camera.yaw));
	front.y = sin(glm::radians(camera.pitch));
	front.z = cos(glm::radians(camera.pitch)) * cos(glm::radians(camera.yaw));

	camera.direction = glm::normalize(front);
	camera.projection = utils::getProjectionInverseZ(glm::radians(45.0f), swapchain.width, swapchain.height, 0.01f);
	camera.view = glm::lookAtLH(camera.position, camera.position + camera.direction, camera.up);
	camera.viewProjection = camera.projection * camera.view;

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
						   | ImGuiWindowFlags_AlwaysAutoResize
						   | ImGuiWindowFlags_NoSavedSettings
						   | ImGuiWindowFlags_NoFocusOnAppearing
						   | ImGuiWindowFlags_NoNav
						   | ImGuiWindowFlags_NoMove;

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
	if (ImGui::Begin("##overlay", nullptr, flags)) {
		float fps_now = (delta > 0.0f) ? (1.0f / delta) : 0.0f;
		static float fps_smooth = fps_now;
		fps_smooth += (fps_now - fps_smooth) * 0.1f;
		ImGui::Text("FPS: %.1f (%.2f ms)", fps_smooth, fps_smooth > 0.f ? 1000.f / fps_smooth : 0.f);
	}

	ImGui::End();
}

void cleanupApplication() {
	VKA(context->device.waitIdle());

	// Imgui
	ImGui_ImplVulkan_Shutdown();
	context->device.destroyDescriptorPool(imguiDescriptorPool);
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	destroyBuffer(context, &spriteVertexBuffer);
	destroyBuffer(context, &spriteIndexBuffer);

	destroyImage(context, &image);
	destroyModel(context, &model);

	context->device.destroySampler(sampler);

	context->device.destroyDescriptorPool(spriteDescriptorPool);
	context->device.destroyDescriptorSetLayout(spriteDescriptorSetLayout);

	context->device.destroyDescriptorPool(modelDescriptorPool);
	context->device.destroyDescriptorSetLayout(modelDescriptorSetLayout);

	for (auto & modelUniformBuffer : modelUniformBuffers) {
		destroyBuffer(context, &modelUniformBuffer);
	}

	for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i) {
		VK(context->device.destroyFence(fences[i]));
		VK(context->device.destroySemaphore(acquireSemaphores[i]));
		VK(context->device.destroySemaphore(releaseSemaphores[i]));
	}

	for (auto &commandPool : commandPools) {
		VK(context->device.destroyCommandPool(commandPool));
	}

	destroyPipeline(context, &spritePipeline);
	destroyPipeline(context, &modelPipeline);

	for (auto& framebuffer : framebuffers) {
		context->device.destroyFramebuffer(framebuffer);
	}
	framebuffers.clear();

	for (auto& depthbuffer : depthBuffers) {
		destroyImage(context, &depthbuffer);
	}
	depthBuffers.clear();

	for (auto& colorbuffer : colorBuffers) {
		destroyImage(context, &colorbuffer);
	}
	colorBuffers.clear();

	destroyRenderPass(context, renderPass);
	destroySwapchain(context, &swapchain);

	context->instance.destroySurfaceKHR(surface);
	exitVulkan(context);
}

bool handleMessage(SDL_Window* window) {
	ImGuiIO& io = ImGui::GetIO();

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL3_ProcessEvent(&event);

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

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				if (event.button.button == SDL_BUTTON_LEFT && !io.WantCaptureMouse) {
					SDL_SetWindowRelativeMouseMode(window, true);
				}
				break;

			case SDL_EVENT_KEY_DOWN:
				if (event.key.key == SDLK_ESCAPE) {
					SDL_SetWindowRelativeMouseMode(window, false);
				}
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

	float delta = 0.0f;
	u64 perfCounterFrequency = SDL_GetPerformanceFrequency();
	u64 lastCounter = SDL_GetPerformanceCounter();

	while (handleMessage(window)) {
		updateApplication(window, delta);
		renderApplication();

		u64 endCounter = SDL_GetPerformanceCounter();
		u64 counterElapsed = endCounter - lastCounter;
		delta = static_cast<float>(counterElapsed) / static_cast<float>(perfCounterFrequency);
		lastCounter = endCounter;
	}

	cleanupApplication();

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

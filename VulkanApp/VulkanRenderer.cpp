#include "VulkanRenderer.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>
#include <array>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

VulkanRenderer::VulkanRenderer() :
	m_window(nullptr), 
	m_instance(nullptr), 
	m_device{}, 
	m_graphicsQueue(nullptr)
{
}

VulkanRenderer::~VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* window)
{
	m_window = window;
	try {
		// The order matters! E.g. an instance
		// is needed to get the phsyical device,
		// a physical device to get the logical 
		// device etc.
		createInstance();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createSynchronisation();
		recordCommands();
	}
	catch (std::runtime_error& e) {
		std::cout << "ERROR: " << e.what() << std::endl;
		return EXIT_FAILURE;
	};

	return EXIT_SUCCESS;
}

void VulkanRenderer::cleanup()
{
	// wait until the device is idle before destroying anything
	vkDeviceWaitIdle(m_device.logicalDevice);

	// cleanup in reverse creation order
	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++) {
		vkDestroySemaphore(m_device.logicalDevice, m_imageAvailable[i], nullptr);
		vkDestroySemaphore(m_device.logicalDevice, m_renderFinished[i], nullptr);
		vkDestroyFence(m_device.logicalDevice, m_drawFences[i], nullptr);
	}
	vkDestroyCommandPool(m_device.logicalDevice, m_graphicsCommandPool, nullptr);
	for (auto framebuffer : m_swapChainFramebuffers) {
		vkDestroyFramebuffer(m_device.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(m_device.logicalDevice, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device.logicalDevice, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_device.logicalDevice, m_renderPass, nullptr);
	for (const auto& swapChainImage : m_swapChainImages) {
		vkDestroyImageView(m_device.logicalDevice, swapChainImage.imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_device.logicalDevice, m_swapchain, nullptr);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyDevice(m_device.logicalDevice, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

void VulkanRenderer::draw()
{
	// Get next image
	uint32_t imageIndex;
	vkAcquireNextImageKHR(m_device.logicalDevice, m_swapchain, std::numeric_limits<uint64_t>::max(),
		m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

	vkWaitForFences(m_device.logicalDevice, 1, &m_drawFences[m_currentFrame], 
		VK_TRUE, std::numeric_limits<uint32_t>::max());
	vkResetFences(m_device.logicalDevice, 1, &m_drawFences[m_currentFrame]);

	// Submit command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAvailable[m_currentFrame];
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderFinished[m_currentFrame];

	VkResult result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_drawFences[m_currentFrame]);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit command buffer to queue.");
	}

	// Present rendered image to screen
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderFinished[m_currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(m_presentationQueue, &presentInfo);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to present image");
	}

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAME_DRAWS;

}

void VulkanRenderer::createInstance()
{
	// Application metadata
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Course Application";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Balengine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0; // 1.2 is the latest

	// Instance parameters
	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// Query instance extensions
	NameList_t instanceExtensions{};

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (size_t i = 0; i < glfwExtensionCount; i++) {
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	if (!checkInstanceExtensionSupport(instanceExtensions)) {
		throw std::runtime_error("VkInstance does not support the required extensions");
	}

	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// Check and create validation layers if needed
	if (enableValidationLayers) {

		if (!checkValidationLayers()) {
			throw std::runtime_error("Validation layers requested, but not available!");
		}

		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a Vulkan instance");
	}
}

void VulkanRenderer::getPhysicalDevice()
{
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("No Vulkan physical devices available");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	// Pick the first suitable device
	for (const auto& device : devices) {
		if (checkDeviceSuitable(device)) {
			m_device.physicalDevice = device;
		}
	}

	if (m_device.physicalDevice == nullptr) {
		throw std::runtime_error("No suitable physical device found");
	}

	VkPhysicalDeviceProperties deviceProperties{};
	vkGetPhysicalDeviceProperties(m_device.physicalDevice, &deviceProperties);
	std::cout << "Selected physical device " << deviceProperties.deviceName << std::endl;
}

void VulkanRenderer::createLogicalDevice()
{
	QueueFamilyIndices indices = getQueueFamilyIndices(m_device.physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };

	for (int index : queueFamilyIndices) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex =index;
		queueCreateInfo.queueCount = 1;
		float priority = 1.0;
		queueCreateInfo.pQueuePriorities = &priority;
		queueCreateInfos.push_back(queueCreateInfo);
	}
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t> (queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	VkPhysicalDeviceFeatures deviceFeatures{};

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	VkResult result = vkCreateDevice(m_device.physicalDevice, &deviceCreateInfo, nullptr, &m_device.logicalDevice);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create the logical device");
	}

	vkGetDeviceQueue(m_device.logicalDevice, indices.graphicsFamily, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device.logicalDevice, indices.presentationFamily, 0, &m_presentationQueue);
}

void VulkanRenderer::createSurface()
{
	VkResult result = glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create surface");
	}
}

void VulkanRenderer::createSwapChain()
{
	auto swapChainDetails = getSwapChainDetails(m_device.physicalDevice);

	auto surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
	auto presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
	auto extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

	// Image count is min+1 to allow triple buffering, unless
	// it exceeds the maximum. Note that a max of 0 means that there
	// is no upper limit to the image count (hence the check for > 0 in the if).
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;
	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 
		&& imageCount > swapChainDetails.surfaceCapabilities.maxImageCount) {
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.presentMode = presentMode;
	createInfo.imageExtent = extent;
	createInfo.minImageCount = imageCount;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.clipped = VK_TRUE;

	QueueFamilyIndices indices = getQueueFamilyIndices(m_device.physicalDevice);
	// this is only needed if the indices are different
	uint32_t queueFamilyIndices[] = {
		static_cast<uint32_t>(indices.graphicsFamily),
		static_cast<uint32_t>(indices.presentationFamily)
	};

	if (indices.graphicsFamily != indices.presentationFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VkResult result = vkCreateSwapchainKHR(m_device.logicalDevice, &createInfo, nullptr, &m_swapchain);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create swapchain!");
	}

	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;

	uint32_t swapChainImageCount = 0;
	vkGetSwapchainImagesKHR(m_device.logicalDevice, m_swapchain, &swapChainImageCount, nullptr);
	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(m_device.logicalDevice, m_swapchain, &swapChainImageCount, images.data());

	for (const auto& image : images) {
		SwapChainImage swapChainImage{};
		swapChainImage.image = image;
		swapChainImage.imageView = createImageView(image, m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		m_swapChainImages.push_back(swapChainImage);
	}
}

void VulkanRenderer::createRenderPass()
{
	// describe color attachment
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// reference to the attachment in the render pass, for subpass
	VkAttachmentReference colorAttachmentReference{};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// describe subpass (could be many, we have only one)
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;

	// Subpass dependenciues

	// Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	//
	// Must happen after the input pipeline has completed and before the subpass color attachment
	// output
	std::array<VkSubpassDependency, 2> subpassDependencies;
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	// Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	//
	// Must happen after the subpass color attachment output and before the input pipeline
	// has completed
	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	// Create information for renderpass
	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = 1;
	createInfo.pAttachments = &colorAttachment;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;
	createInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	createInfo.pDependencies = subpassDependencies.data();

	VkResult result = vkCreateRenderPass(m_device.logicalDevice, &createInfo, nullptr, &m_renderPass);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create render pass");
	}


}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags flags)
{
	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.format = format;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

	// components
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// subresources
	createInfo.subresourceRange.aspectMask = flags;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	VkResult result = vkCreateImageView(m_device.logicalDevice, &createInfo, nullptr, &imageView);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Could not create image view");
	}

	return imageView;
}

VkShaderModule VulkanRenderer::createShaderModule(std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<uint32_t*>(code.data());

	VkShaderModule module;
	VkResult result = vkCreateShaderModule(m_device.logicalDevice, &createInfo, nullptr, &module);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create shader module");
	}

	return module;
}

void VulkanRenderer::createGraphicsPipeline()
{
	auto vertexShaderCode = readFile("../shaders/vert.spv");
	auto fragmentShaderCode = readFile("../shaders/frag.spv");

	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

	// Create pipeline stages
	
	// Vertex stage
	VkPipelineShaderStageCreateInfo vertexStageCreateInfo{};
	vertexStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexStageCreateInfo.module = vertexShaderModule;
	vertexStageCreateInfo.pName = "main";

	// Fragment stage
	VkPipelineShaderStageCreateInfo fragmentStageCreateInfo{};
	fragmentStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentStageCreateInfo.module = fragmentShaderModule;
	fragmentStageCreateInfo.pName = "main";

	// Put all stages in an array as required by the pipeline creation
	VkPipelineShaderStageCreateInfo shaderStageInfos[] = { vertexStageCreateInfo, fragmentStageCreateInfo };

	// Vertex Input
	VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;

	// Input Assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	// Viewport & scissor
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapChainExtent.width);
	viewport.height = static_cast<float>(m_swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportInfo{};
	viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportInfo.viewportCount = 1;
	viewportInfo.pViewports = &viewport;
	viewportInfo.scissorCount = 1;
	viewportInfo.pScissors = &scissor;

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
	rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerInfo.depthClampEnable = VK_FALSE;
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerInfo.lineWidth = 1.0f;
	rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerInfo.depthBiasEnable = VK_FALSE;

	// Multi-sampling
	VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.sampleShadingEnable = VK_FALSE;
	multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Blending
	VkPipelineColorBlendAttachmentState colorState{};
	colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorState.blendEnable = VK_TRUE;
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;
	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorState;

	// Pipeline layout - TODO
	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.pPushConstantRanges = nullptr;
	layoutInfo.pushConstantRangeCount = 0;
layoutInfo.pSetLayouts = nullptr;
layoutInfo.setLayoutCount = 0;

VkResult result = vkCreatePipelineLayout(m_device.logicalDevice, &layoutInfo, nullptr, &m_pipelineLayout);
if (result != VK_SUCCESS) {
	throw std::runtime_error("Could not create pipeline layout");
}

// TODO - add depth stencil testing
VkGraphicsPipelineCreateInfo createInfo{};
createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
createInfo.stageCount = 2;
createInfo.pStages = shaderStageInfos;
createInfo.pVertexInputState = &vertexInputInfo;
createInfo.pInputAssemblyState = &inputAssemblyInfo;
createInfo.pViewportState = &viewportInfo;
createInfo.pDynamicState = nullptr;
createInfo.pRasterizationState = &rasterizerInfo;
createInfo.pMultisampleState = &multisamplingInfo;
createInfo.pColorBlendState = &colorBlendInfo;
createInfo.pDepthStencilState = nullptr;
createInfo.layout = m_pipelineLayout;
createInfo.renderPass = m_renderPass;
createInfo.subpass = 0;
createInfo.basePipelineHandle = VK_NULL_HANDLE;
createInfo.basePipelineIndex = -1;

result = vkCreateGraphicsPipelines(m_device.logicalDevice, VK_NULL_HANDLE, 1,
	&createInfo, nullptr, &m_graphicsPipeline);

if (result != VK_SUCCESS) {
	throw std::runtime_error("Failed to create graphics pipeline");
}

// Destroy shader modules
vkDestroyShaderModule(m_device.logicalDevice, fragmentShaderModule, nullptr);
vkDestroyShaderModule(m_device.logicalDevice, vertexShaderModule, nullptr);
}

void VulkanRenderer::createFramebuffers()
{
	m_swapChainFramebuffers.resize(m_swapChainImages.size());
	for (size_t i = 0; i < m_swapChainFramebuffers.size(); i++) {

		std::array<VkImageView, 1> attachments = {
			m_swapChainImages[i].imageView
		};

		VkFramebufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = m_renderPass;
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.width = m_swapChainExtent.width;
		createInfo.height = m_swapChainExtent.height;
		createInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(m_device.logicalDevice, &createInfo, nullptr, &m_swapChainFramebuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer!");
		}
	}
}

void VulkanRenderer::createCommandPool()
{
	auto indices = getQueueFamilyIndices(m_device.physicalDevice);
	VkCommandPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = indices.graphicsFamily;

	VkResult result = vkCreateCommandPool(m_device.logicalDevice, &createInfo, nullptr, &m_graphicsCommandPool);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a command pool!");
	}
}

void VulkanRenderer::createCommandBuffers()
{
	m_commandBuffers.resize(m_swapChainFramebuffers.size());
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_graphicsCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Can only be submitted directly to a queue
	allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

	VkResult result = vkAllocateCommandBuffers(m_device.logicalDevice, &allocInfo, m_commandBuffers.data());

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate command buffers");
	}
}

void VulkanRenderer::createSynchronisation()
{
	m_imageAvailable.resize(MAX_FRAME_DRAWS);
	m_renderFinished.resize(MAX_FRAME_DRAWS);
	m_drawFences.resize(MAX_FRAME_DRAWS);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++) {
		if ((vkCreateSemaphore(m_device.logicalDevice, &semaphoreInfo, nullptr, &m_imageAvailable[i]) != VK_SUCCESS) ||
			(vkCreateSemaphore(m_device.logicalDevice, &semaphoreInfo, nullptr, &m_renderFinished[i]) != VK_SUCCESS) ||
			(vkCreateFence(m_device.logicalDevice, &fenceInfo, nullptr, &m_drawFences[i]) != VK_SUCCESS))
		{
			throw std::runtime_error("Failed to create semaphore or fence");
		}
	}
}

void VulkanRenderer::recordCommands()
{
	// information about how to begin each command buffer (same for each command)

	VkCommandBufferBeginInfo bufferBeginInfo{};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // Buffer can be resubmitted to the same queue

	// information about how to begin a render pass
	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_renderPass;
	renderPassBeginInfo.renderArea.offset = { 0,0 };
	renderPassBeginInfo.renderArea.extent = m_swapChainExtent;

	VkClearValue clearValues[] = {
		{0.6f, 0.65f, 0.4f, 1.0f}
	};
	renderPassBeginInfo.pClearValues = clearValues;
	renderPassBeginInfo.clearValueCount = 1;

	for (size_t i = 0; i < m_commandBuffers.size(); i++) {

		// associate this command buffer with the corresponding framebuffer.
		renderPassBeginInfo.framebuffer = m_swapChainFramebuffers[i];

		VkResult result = vkBeginCommandBuffer(m_commandBuffers[i], &bufferBeginInfo);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to start recording a command buffer");
		}

		vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Actually draw something using the graphics pipeline
		vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
		vkCmdDraw(m_commandBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(m_commandBuffers[i]);

		result = vkEndCommandBuffer(m_commandBuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to stop recording a command buffer");
		}
	}

}

bool VulkanRenderer::checkInstanceExtensionSupport(const NameList_t& requiredExtensions) 
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

	for (const auto& requiredExtension : requiredExtensions) {
		bool hasExtension = false;
		for (const auto& extensionProperties : availableExtensions) {
			if (strcmp(extensionProperties.extensionName, requiredExtension)) {
				hasExtension = true;
					break;
			}
		}
		if (!hasExtension) {
			return false;
		}
	}
	return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	if (extensionCount == 0) {
		return false;
	}

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	for (const auto& requiredExtension : deviceExtensions) {
		bool hasExtension = false;
		for (const auto& extension : availableExtensions) {
			if (strcmp(extension.extensionName, requiredExtension)) {
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension) {
			return false;
		}
	}
	return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	auto indices = getQueueFamilyIndices(device);
	auto swapChainDetails = getSwapChainDetails(device);

	return	indices.isValid() && 
			checkDeviceExtensionSupport(device) &&
			swapChainDetails.isValid();
}

bool VulkanRenderer::checkValidationLayers()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const auto& layerName : validationLayers) {

		bool layerFound = false;
		for (const auto& availableLayer : availableLayers) {
			if (strcmp(layerName, availableLayer.layerName)) {
				layerFound = true;
				break;
			}
		}
		if (!layerFound) {
			return false;
		}
	}

	return true;
}

VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	// "undefined" actually means all formats are supported
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& format : formats) {
		if ( (format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) 
			&& format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
			return format;
		}
	}

	return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	for (const auto& presentationMode : presentationModes) {
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return presentationMode;
		}
	}

	// The standard says that FIFO presentation mode must always 
	// be supported
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return surfaceCapabilities.currentExtent;
	}

	// return window size clamped to min/max image size
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	uint32_t width32 = static_cast<uint32_t>(width);
	uint32_t height32 = static_cast<uint32_t>(width);

	VkExtent2D newExtent = {};
	newExtent.height = std::clamp(height32, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
	newExtent.width = std::clamp(width32, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);

	return newExtent;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilyIndices(VkPhysicalDevice device)
{
	QueueFamilyIndices indices{};

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& family : queueFamilies) 
	{
		if (family.queueCount > 0 && family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentationSupported = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentationSupported);
		if (family.queueCount > 0 && presentationSupported) {
			indices.presentationFamily = i;
		}

		if (indices.isValid()) {
			break;
		}

		i++;
	}
	return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device)
{
	SwapChainDetails swapChainDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &swapChainDetails.surfaceCapabilities);
	
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
	if (formatCount != 0) {
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, swapChainDetails.formats.data());
	}

	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationCount, nullptr);
	if (presentationCount != 0) {
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationCount, swapChainDetails.presentationModes.data());
	}
	return swapChainDetails;
}

std::vector<char> VulkanRenderer::readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file");
	}

	size_t fileSize = static_cast<size_t>(file.tellg());

	std::vector<char> fileBuffer(fileSize);
	file.seekg(0);
	file.read(fileBuffer.data(), fileSize);
	file.close();

	return fileBuffer;
}

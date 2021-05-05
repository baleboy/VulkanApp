#include "VulkanRenderer.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>


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
		createGraphicsPipeline();
	}
	catch (std::runtime_error& e) {
		std::cout << "ERROR: " << e.what() << std::endl;
		return EXIT_FAILURE;
	};

	return EXIT_SUCCESS;
}

void VulkanRenderer::cleanup()
{
	// cleanup in reverse creation order
	for (const auto& swapChainImage : m_swapChainImages) {
		vkDestroyImageView(m_device.logicalDevice, swapChainImage.imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_device.logicalDevice, m_swapChain, nullptr);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyDevice(m_device.logicalDevice, nullptr);
	vkDestroyInstance(m_instance, nullptr);
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

	VkResult res = vkCreateInstance(&createInfo, nullptr, &m_instance);

	if (res != VK_SUCCESS) {
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
		indices.graphicsFamily,
		indices.presentationFamily
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

	VkResult result = vkCreateSwapchainKHR(m_device.logicalDevice, &createInfo, nullptr, &m_swapChain);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create swapchain!");
	}

	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;

	uint32_t swapChainImageCount = 0;
	vkGetSwapchainImagesKHR(m_device.logicalDevice, m_swapChain, &swapChainImageCount, nullptr);
	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(m_device.logicalDevice, m_swapChain, &swapChainImageCount, images.data());

	for (const auto& image : images) {
		SwapChainImage swapChainImage{};
		swapChainImage.image = image;
		swapChainImage.imageView = createImageView(image, m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		m_swapChainImages.push_back(swapChainImage);
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
	vertexStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	vertexStageCreateInfo.module = fragmentShaderModule;
	vertexStageCreateInfo.pName = "main";

	// Put all stages in an array as required by the pipeline creation
	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = { vertexStageCreateInfo, fragmentStageCreateInfo };

	// Create the pipeline
	


	// Clean up shader modules
	vkDestroyShaderModule(m_device.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(m_device.logicalDevice, vertexShaderModule, nullptr);
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

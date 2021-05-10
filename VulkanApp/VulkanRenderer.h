#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

#include "Utils.h"

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();

	int init(GLFWwindow* window);
	void cleanup();

private:
	GLFWwindow* m_window;

	// Vulkan data structures
	VkInstance m_instance;

	struct {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} m_device;

	VkQueue m_graphicsQueue;
	VkQueue m_presentationQueue;
	VkSurfaceKHR m_surface;
	VkSwapchainKHR m_swapChain;

	std::vector<SwapChainImage> m_swapChainImages;

	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	VkPipelineLayout m_pipelineLayout;
	VkRenderPass m_renderPass;
	VkPipeline m_graphicsPipeline;

	// Vulkan helpers

	// Create/get
	void createInstance();
	void getPhysicalDevice();
	void createLogicalDevice();
	void createSurface();
	void createSwapChain();
	void createRenderPass();
	void createGraphicsPipeline();

	// Check
	using NameList_t = std::vector<const char*>;
	bool checkInstanceExtensionSupport(const NameList_t& extensionNames);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkValidationLayers();

	// Choose
	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

	// Utility
	QueueFamilyIndices getQueueFamilyIndices(VkPhysicalDevice device);
	SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags flags);
	VkShaderModule createShaderModule(std::vector<char>& code);

	static std::vector<char> readFile(const std::string& filename);

};


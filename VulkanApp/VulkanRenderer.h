#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <vector>

#include "Utils.h"
#include "Mesh.h"

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();

	int init(GLFWwindow* window);
	void cleanup();
	void draw();

	void updateModel(int modelId, glm::mat4 newModel);

private:
	GLFWwindow* m_window;

	int m_currentFrame = 0;

	// Meshes
	std::vector<Mesh> m_meshList;

	// Scene settings
	struct UboViewProjection {
		glm::mat4 projection;
		glm::mat4 view;
	} m_uboViewProjection;

	// Vulkan data structures
	VkInstance m_instance;

	struct {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} m_device;

	VkQueue m_graphicsQueue;
	VkQueue m_presentationQueue;
	VkSurfaceKHR m_surface;
	VkSwapchainKHR m_swapchain;

	// Elements in the below vectors are mapped 1:1 - command buffer
	// at index i will only ever use framebuffer i and swapchainimage i
	std::vector<SwapChainImage> m_swapChainImages;
	std::vector<VkFramebuffer> m_swapChainFramebuffers;
	std::vector<VkCommandBuffer> m_commandBuffers;

	// Descriptors
	VkDescriptorSetLayout m_descriptorSetLayout;

	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;

	std::vector<VkBuffer> m_vpUniformBuffers; // one per swapchain, to avoid updating a uniform while it's bound
	std::vector<VkDeviceMemory> m_vpUniformBufferMemory;

	std::vector<VkBuffer> m_modelDynamicUniformBuffers; // see above
	std::vector<VkDeviceMemory> m_modelDynamicUniformBufferMemory;


	VkDeviceSize m_minUniformBufferOffset;
	size_t m_modelUniformAlignment;
	UboModel* m_modelTransferSpace;

	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	VkPipelineLayout m_pipelineLayout;
	VkRenderPass m_renderPass;
	VkPipeline m_graphicsPipeline;

	VkCommandPool m_graphicsCommandPool;

	// Synchronization structures
	std::vector<VkSemaphore> m_imageAvailable;
	std::vector<VkSemaphore> m_renderFinished;
	std::vector<VkFence> m_drawFences;

	// Vulkan helpers

	// Create/get
	void createInstance();
	void getPhysicalDevice();
	void createLogicalDevice();
	void createSurface();
	void createSwapChain();
	void createRenderPass();
	void createDescriptorSetLayout();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSynchronisation();

	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();

	void updateUniformBuffers(uint32_t imageIndex);
	void allocateDynamicBufferTransferSpace();

	// Record commands
	void recordCommands();

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


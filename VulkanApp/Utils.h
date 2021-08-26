#pragma once

#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

const std::vector<const char*> deviceExtensions { 
	VK_KHR_SWAPCHAIN_EXTENSION_NAME 
};

const int MAX_FRAME_DRAWS = 2;

const int MAX_OBJECTS = 2;

struct Vertex {
	glm::vec3 pos;
	glm::vec3 col;
};

struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentationFamily = -1;

	bool isValid() {
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapChainDetails {
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentationModes;

	bool isValid() {
		return !formats.empty() && !presentationModes.empty();
	}
};

struct SwapChainImage {
	VkImage image;
	VkImageView imageView;
};

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
		// Check if bit i corresponds to an allowed type and the memory type i
		// has all the needed properties indicated in the memory properties flags
		if (allowedTypes & (1 << i) &&
			(memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Suitable memory type not found");
}


static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
	VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
	VkBufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = bufferSize;
	createInfo.usage = bufferUsage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(device, &createInfo, nullptr, buffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create vertex buffer");
	}

	// Get memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

	// Allocate memory to buffer
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits,
		bufferProperties);

	// allocate memory
	result = vkAllocateMemory(device, &allocInfo, nullptr, bufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate vertex buffer memory");
	}

	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize) 
{
	// Allocate a one-time copy command buffer (will be released at the end)
	VkCommandBuffer transferCommandBuffer;

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandBufferCount = 1;
	allocInfo.commandPool = transferCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer);

	VkCommandBufferBeginInfo commandBeginInfo{};
	commandBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(transferCommandBuffer, &commandBeginInfo);
	
	VkBufferCopy copyRegion{};
	copyRegion.dstOffset = 0;
	copyRegion.srcOffset = 0;
	copyRegion.size = bufferSize;

	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	vkEndCommandBuffer(transferCommandBuffer);

	// Submit the command buffer to the transfer queue and wait that queue goes idle
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffer;

	vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transferQueue);

	vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}
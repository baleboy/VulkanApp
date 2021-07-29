#pragma once

#include <glm/glm.hpp>

const std::vector<const char*> deviceExtensions { 
	VK_KHR_SWAPCHAIN_EXTENSION_NAME 
};

const int MAX_FRAME_DRAWS = 2;

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
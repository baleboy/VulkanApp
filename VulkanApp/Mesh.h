#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include "Utils.h"

class Mesh
{
public:
	Mesh() {};
	Mesh(VkPhysicalDevice physicalDevice, VkDevice device, std::vector<Vertex>* vertices);

	~Mesh() {};

	int getVertexCount();
	VkBuffer getVertexBuffer();
	void destroyBuffer();

private:
	int m_vertexCount;
	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexBufferMemory;
	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;

	void createVertexBuffer(std::vector<Vertex>* vertices);
	uint32_t findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties);

};


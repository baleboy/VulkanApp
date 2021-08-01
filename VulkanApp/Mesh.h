#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include "Utils.h"

class Mesh
{
public:
	Mesh() {};
	Mesh(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
		std::vector<Vertex>* vertices);

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
	VkQueue m_transferQueue;
	VkCommandPool m_transferCommandPool;

	void createVertexBuffer(std::vector<Vertex>* vertices);
};


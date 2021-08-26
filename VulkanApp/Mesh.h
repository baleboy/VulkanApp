#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include "Utils.h"

struct UboModel {
	glm::mat4 model;
};

class Mesh
{
public:

	Mesh() {};
	Mesh(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
		std::vector<Vertex>* vertices, std::vector<uint32_t>* indices);

	~Mesh() {};

	void setModel(glm::mat4 newModel);
	UboModel getModel();

	int getVertexCount();
	VkBuffer getVertexBuffer();
	int getIndexCount();
	VkBuffer getIndexBuffer();
	void destroyBuffers();

private:

	UboModel m_uboModel;

	int m_vertexCount;
	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexBufferMemory;

	int m_indexCount;
	VkBuffer m_indexBuffer;
	VkDeviceMemory m_indexBufferMemory;

	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;
	VkQueue m_transferQueue;
	VkCommandPool m_transferCommandPool;

	void createVertexBuffer(std::vector<Vertex>* vertices);
	void createIndexBuffer(std::vector<uint32_t>* indices);
};


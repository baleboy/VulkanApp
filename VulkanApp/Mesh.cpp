#include <stdexcept>
#include <cstring>

#include "Mesh.h"

Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	std::vector<Vertex>* vertices, std::vector<uint32_t>* indices)
{
	m_model.model = glm::mat4(1.0f);
	m_vertexCount = vertices->size();
	m_indexCount = indices->size();
	m_physicalDevice = physicalDevice;
	m_device = device;
	m_transferQueue = transferQueue;
	m_transferCommandPool = transferCommandPool;

	createVertexBuffer(vertices);
	createIndexBuffer(indices);
}

void Mesh::setModel(glm::mat4 newModel)
{
	m_model.model = newModel;
}

Model Mesh::getModel()
{
	return m_model;
}

int Mesh::getVertexCount()
{
	return m_vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
	return m_vertexBuffer;
}

int Mesh::getIndexCount()
{
	return m_indexCount;
}

VkBuffer Mesh::getIndexBuffer()
{
	return m_indexBuffer;
}

void Mesh::destroyBuffers()
{
	vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
	vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
	vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
	vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
}

void Mesh::createVertexBuffer(std::vector<Vertex>* vertices)
{

	VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();

	// Create a staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(m_physicalDevice, m_device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	//Map memory to staging buffer and copy vertex data into it
	void* data;
	vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data); // data now points to the buffer memory
	memcpy(data, vertices->data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(m_device, stagingBufferMemory);

	// Create vertex buffer in memory only visible by the GPU. The buffer is set up to be
	// both a vertex buffer and the destination for a memory transfer from the staging buffer
	createBuffer(m_physicalDevice, m_device, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m_vertexBuffer, &m_vertexBufferMemory);

	copyBuffer(m_device, m_transferQueue, m_transferCommandPool, stagingBuffer, m_vertexBuffer, bufferSize);

	vkDestroyBuffer(m_device, stagingBuffer, nullptr);
	vkFreeMemory(m_device, stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer(std::vector<uint32_t>* indices)
{
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices->size();

	// Create a staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(m_physicalDevice, m_device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	//Map memory to staging buffer and copy index data into it
	void* data;
	vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data); // data now points to the buffer memory
	memcpy(data, indices->data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(m_device, stagingBufferMemory);

	// Create index buffer in memory only visible by the GPU. The buffer is set up to be
	// both an index buffer and the destination for a memory transfer from the staging buffer
	createBuffer(m_physicalDevice, m_device, bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m_indexBuffer, &m_indexBufferMemory);

	copyBuffer(m_device, m_transferQueue, m_transferCommandPool, stagingBuffer, m_indexBuffer, bufferSize);

	vkDestroyBuffer(m_device, stagingBuffer, nullptr);
	vkFreeMemory(m_device, stagingBufferMemory, nullptr);
}

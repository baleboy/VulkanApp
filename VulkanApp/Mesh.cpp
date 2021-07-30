
#include <stdexcept>
#include "Mesh.h"

Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device, std::vector<Vertex>* vertices)
{
	m_vertexCount = vertices->size();
	m_physicalDevice = physicalDevice;
	m_device = device;
	createVertexBuffer(vertices);
}

int Mesh::getVertexCount()
{
	return m_vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
	return m_vertexBuffer;
}

void Mesh::destroyBuffer()
{
	vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
	vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
}

void Mesh::createVertexBuffer(std::vector<Vertex>* vertices)
{

	VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();
	VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	createBuffer(m_physicalDevice, m_device, bufferSize, usageFlags, memProperties, &m_vertexBuffer, &m_vertexBufferMemory);

	//Map memory to vertex buffer
	void* data;
	vkMapMemory(m_device, m_vertexBufferMemory, 0, bufferSize, 0, &data); // data now points to the buffer memory
	memcpy(data, vertices->data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(m_device, m_vertexBufferMemory);
}

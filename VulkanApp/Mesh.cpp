
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
	VkBufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = sizeof(Vertex) * vertices->size();
	createInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(m_device, &createInfo, nullptr, &m_vertexBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create vertex buffer");
	}

	// Get memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_device, m_vertexBuffer, &memRequirements);

	// Allocate memory to buffer
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryTypeIndex(memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// allocate memory
	result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_vertexBufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate vertex buffer memory");
	}

	vkBindBufferMemory(m_device, m_vertexBuffer, m_vertexBufferMemory, 0);

	//Map memory to vertex buffer
	void* data;
	vkMapMemory(m_device, m_vertexBufferMemory, 0, createInfo.size, 0, &data); // data now points to the buffer memory
	memcpy(data, vertices->data(), static_cast<size_t>(createInfo.size));
	vkUnmapMemory(m_device, m_vertexBufferMemory);
}

uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);

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

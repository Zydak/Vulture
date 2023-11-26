#include "pch.h"
#include "Quad.h"

namespace Vulture
{

	Quad::Quad()
	{

	}

	Quad::~Quad()
	{

	}

	void Quad::Init()
	{
		// Vertices for a simple quad
		const std::vector<Vertex> vertices = {
			Vertex(glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)),  // Vertex 1 Bottom Left
			Vertex(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f)), // Vertex 2 Top Left
			Vertex(glm::vec3(1.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f)),  // Vertex 3 Top Right
			Vertex(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f))    // Vertex 4 Bottom Right
		};

		const std::vector<uint32_t> indices = {
			0, 1, 2,  // First triangle
			0, 2, 3   // Second triangle
		};

		CreateVertexBuffer(vertices);
		CreateIndexBuffer(indices);
	}

	void Quad::Bind(VkCommandBuffer commandBuffer)
	{
		VkBuffer buffers[] = { m_VertexBuffer->GetBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
	}

	void Quad::Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount)
	{
		vkCmdDrawIndexed(commandBuffer, 6, instanceCount, 0, 0, 0);
	}

	void Quad::CreateVertexBuffer(const std::vector<Vertex>& vertices)
	{
		int vertexCount = static_cast<uint32_t>(vertices.size());
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
		uint32_t vertexSize = sizeof(vertices[0]);

		/*
			We need to be able to write our vertex data to memory.
			This property is indicated with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT property.
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is Used to get memory heap that is host coherent.
			We use this to copy the data into the buffer memory immediately.
		*/
		Buffer stagingBuffer(vertexSize, vertexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		/*
			When buffer is created It is time to copy the vertex data to the buffer.
			This is done by mapping the buffer memory into CPU accessible memory with vkMapMemory.
		*/
		stagingBuffer.Map();
		stagingBuffer.WriteToBuffer((void*)vertices.data());

		/*
			The vertexBuffer is now allocated from a memory type that is device
			local, which generally means that we're not able to use vkMapMemory.
			However, we can copy data from the stagingBuffer to the vertexBuffer.
			We have to indicate that we intend to do that by specifying the transfer
			source flag(VK_BUFFER_USAGE_TRANSFER_SRC_BIT) for the stagingBuffer
			and the transfer destination flag(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
			for the vertexBuffer, along with the vertex buffer usage flag.
		*/

		m_VertexBuffer = std::make_shared<Buffer>(vertexSize, vertexCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		Buffer::CopyBuffer(stagingBuffer.GetBuffer(), m_VertexBuffer->GetBuffer(), bufferSize, Device::GetGraphicsQueue(), Device::GetCommandPool());
	}

	void Quad::CreateIndexBuffer(const std::vector<uint32_t>& indices)
	{
		int indexCount = static_cast<uint32_t>(indices.size());

		VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
		uint32_t indexSize = sizeof(indices[0]);

		/*
			We need to be able to write our index data to memory.
			This property is indicated with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT property.
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is Used to get memory heap that is host coherent.
			We use this to copy the data into the buffer memory immediately.
		*/
		Buffer stagingBuffer(indexSize, indexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		/*
			When buffer is created It is time to copy the index data to the buffer.
			This is done by mapping the buffer memory into CPU accessible memory with vkMapMemory.
		*/
		stagingBuffer.Map();
		stagingBuffer.WriteToBuffer((void*)indices.data());

		/*
			The IndexBuffer is now allocated from a memory type that is device
			local, which generally means that we're not able to use vkMapMemory.
			However, we can copy data from the stagingBuffer to the IndexBuffer.
			We have to indicate that we intend to do that by specifying the transfer
			source flag(VK_BUFFER_USAGE_TRANSFER_SRC_BIT) for the stagingBuffer
			and the transfer destination flag(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
			for the IndexBuffer, along with the IndexBuffer usage flag.
		*/
		m_IndexBuffer = std::make_shared<Buffer>(indexSize, indexCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		Buffer::CopyBuffer(stagingBuffer.GetBuffer(), m_IndexBuffer->GetBuffer(), bufferSize, Device::GetGraphicsQueue(), Device::GetCommandPool());
	}

	std::vector<VkVertexInputBindingDescription> Quad::Vertex::GetBindingDescriptions()
	{
		std::vector<VkVertexInputBindingDescription> bindingDescription(1);
		bindingDescription[0].binding = 0;
		bindingDescription[0].stride = sizeof(Vertex);
		bindingDescription[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	std::vector<VkVertexInputAttributeDescription> Quad::Vertex::GetAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
		attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) });
		attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord) });

		return attributeDescriptions;
	}

}
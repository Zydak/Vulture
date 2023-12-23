#include "pch.h"
#include "Mesh.h"

namespace Vulture
{
	void Mesh::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	{
		CreateVertexBuffer(vertices);
		CreateIndexBuffer(indices);
	}

	void Mesh::CreateVertexBuffer(const std::vector<Vertex>& vertices)
	{
		m_VertexCount = static_cast<uint32_t>(vertices.size());
		VkDeviceSize bufferSize = sizeof(vertices[0]) * m_VertexCount;
		uint32_t vertexSize = sizeof(vertices[0]);

		/*
			We need to be able to write our vertex data to memory.
			This property is indicated with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT property.
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is Used to get memory heap that is host coherent.
			We use this to copy the data into the buffer memory immediately.
		*/
		Buffer stagingBuffer(vertexSize, m_VertexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

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

		m_VertexBuffer = std::make_shared<Buffer>(vertexSize, m_VertexCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		Buffer::CopyBuffer(stagingBuffer.GetBuffer(), m_VertexBuffer->GetBuffer(), bufferSize, Device::GetGraphicsQueue(), Device::GetCommandPool());
	}

	void Mesh::CreateIndexBuffer(const std::vector<uint32_t>& indices)
	{
		m_IndexCount = static_cast<uint32_t>(indices.size());
		m_HasIndexBuffer = m_IndexCount > 0;
		if (!m_HasIndexBuffer) { return; }

		VkDeviceSize bufferSize = sizeof(indices[0]) * m_IndexCount;
		uint32_t indexSize = sizeof(indices[0]);

		/*
			We need to be able to write our index data to memory.
			This property is indicated with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT property.
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is Used to get memory heap that is host coherent.
			We use this to copy the data into the buffer memory immediately.
		*/
		Buffer stagingBuffer(indexSize, m_IndexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

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
		m_IndexBuffer = std::make_shared<Buffer>(indexSize, m_IndexCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		Buffer::CopyBuffer(stagingBuffer.GetBuffer(), m_IndexBuffer->GetBuffer(), bufferSize, Device::GetGraphicsQueue(), Device::GetCommandPool());
	}

	/**
		@brief Binds vertex and index buffers
	*/
	void Mesh::Bind(VkCommandBuffer commandBuffer)
	{
		VkBuffer buffers[] = { m_VertexBuffer->GetBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

		if (m_HasIndexBuffer) { vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32); }
	}

	void Mesh::Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance)
	{
		if (m_HasIndexBuffer) { vkCmdDrawIndexed(commandBuffer, m_IndexCount, instanceCount, 0, 0, firstInstance); }
		else { vkCmdDraw(commandBuffer, m_VertexCount, instanceCount, 0, firstInstance); }
	}

	/**
	 * @brief Specifies how many vertex buffers we wish to bind to our pipeline. In this case there is only one with all data packed inside it
	*/
	std::vector<VkVertexInputBindingDescription> Mesh::Vertex::GetBindingDescriptions()
	{
		std::vector<VkVertexInputBindingDescription> bindingDescription(1);
		bindingDescription[0].binding = 0;
		bindingDescription[0].stride = sizeof(Vertex);
		bindingDescription[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	/**
	 * @brief Specifies layout of data inside vertex buffer
	*/
	std::vector<VkVertexInputAttributeDescription> Mesh::Vertex::GetAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
		attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) });
		attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord) });

		return attributeDescriptions;
	}

	void Mesh::UpdateVertexBuffer(VkCommandBuffer cmd, int offset, Buffer* buffer, const std::vector<Vertex>& vertices)
	{
		vkCmdUpdateBuffer(cmd, buffer->GetBuffer(), offset, sizeof(vertices[0]) * vertices.size(), vertices.data());
		m_VertexCount += (uint32_t)vertices.size();
	}

	void Mesh::CreateEmptyBuffers(int vertexCount, int indexCount, VkMemoryPropertyFlagBits vertexBufferFlags, VkMemoryPropertyFlagBits indexBufferFlags)
	{
		m_VertexBuffer = std::make_shared<Buffer>(sizeof(Vertex), vertexCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBufferFlags);
		m_IndexBuffer =  std::make_shared<Buffer>(4, indexCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBufferFlags);
	}
}
#include "pch.h"
#include "Mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <glm/gtx/hash.hpp>

template <typename T, typename... Rest> void HashCombine(std::uint32_t& seed, const T& v, const Rest&... rest) 
{
	seed ^= std::hash<T> {}(v)+0x9e3779b9 + (seed << 6) + (seed >> 2);
	(HashCombine(seed, rest), ...);
};

namespace std 
{
	template <> struct hash<Vulture::Mesh::Vertex> 
	{
		uint32_t operator()(Vulture::Mesh::Vertex const& vertex) const 
		{
			uint32_t seed = 0;
			HashCombine(seed, vertex.position);
			return seed;
		}
	};
}

namespace Vulture
{
	void Mesh::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	{
		CreateVertexBuffer(vertices);
		CreateIndexBuffer(indices);
	}

	void Mesh::CreateMesh(const std::string& filepath)
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;
		std::string mtlPath = filepath;
		uint32_t lastSlashPos = (uint32_t)mtlPath.find_last_of('/');
		if (lastSlashPos != std::string::npos)
		{
			// Erase everything after the last '/'
			mtlPath.erase(lastSlashPos);
		}
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str(), mtlPath.c_str()))
		{
			VL_CORE_WARN(warn + err);
		}
		std::cout << warn << std::endl;

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		std::unordered_map<Vertex, uint32_t> uniqueVertices{};
		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				if (index.vertex_index >= 0) {
					vertex.position = {
						attrib.vertices[3 * index.vertex_index + 0],
						attrib.vertices[3 * index.vertex_index + 1],
						attrib.vertices[3 * index.vertex_index + 2],
					};
				}

				if (index.texcoord_index >= 0)
				{
					vertex.texCoord = {
						attrib.texcoords[2 * index.texcoord_index + 0], 1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
					};
				}

				if (index.normal_index >= 0) {
					vertex.normal = {
						attrib.normals[3 * index.normal_index + 0],
						attrib.normals[3 * index.normal_index + 1],
						attrib.normals[3 * index.normal_index + 2],
					};
				}

				// Check if vertex is in hash map already, if yes just add it do indices. We're saving a lot of memory this way
				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
				}
				indices.push_back(uniqueVertices[vertex]);
			}
		}

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

		VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if (Device::IsRayTracingSupported())
			usageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		m_VertexBuffer = std::make_shared<Buffer>(vertexSize, m_VertexCount, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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
		VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (Device::IsRayTracingSupported())
			usageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		m_IndexBuffer = std::make_shared<Buffer>(indexSize, m_IndexCount, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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
		attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) });
		attributeDescriptions.push_back({ 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord) });

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
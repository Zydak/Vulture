#include "pch.h"
#include "Mesh.h"

namespace Vulture
{
	void Mesh::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	{
		CreateVertexBuffer(vertices);
		CreateIndexBuffer(indices);
	}

	void Mesh::CreateMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 mat)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		//aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		//aiColor3D emissiveColor(0.0f, 0.0f, 0.0f); // Default color
		//material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);

		// vertices
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;
			glm::vec3 vector;

			// positions
			vector.x = mesh->mVertices[i].x;
			vector.y = mesh->mVertices[i].y;
			vector.z = mesh->mVertices[i].z;
			vertex.Position = mat * glm::vec4(vector, 1.0f);

			// normals
			if (mesh->HasNormals())
			{
				vector.x = mesh->mNormals[i].x;
				vector.y = mesh->mNormals[i].y;
				vector.z = mesh->mNormals[i].z;
				vertex.Normal = vector;
			}

			// texture coordinates
			if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates
			{
				glm::vec2 vec;
				// a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
				// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
				vec.x = mesh->mTextureCoords[0][i].x;
				vec.y = mesh->mTextureCoords[0][i].y;
				vertex.TexCoord = vec;

				// tangent
				vector.x = mesh->mTangents[i].x;
				vector.y = mesh->mTangents[i].y;
				vector.z = mesh->mTangents[i].z;
				vertex.Tangent = vector;

				// bitangent
				vector.x = mesh->mBitangents[i].x;
				vector.y = mesh->mBitangents[i].y;
				vector.z = mesh->mBitangents[i].z;
				vertex.Bitangent = vector;
			}
			else
				vertex.TexCoord = glm::vec2(0.0f, 0.0f);

			vertices.push_back(vertex);
		}

		// indices
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		CreateVertexBuffer(vertices);
		CreateIndexBuffer(indices);
	}

	void Mesh::CreateVertexBuffer(const std::vector<Vertex>& vertices)
	{
		m_VertexCount = (uint32_t)vertices.size();
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
		m_IndexCount = (uint32_t)indices.size();
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

		if (m_HasIndexBuffer) 
		{ 
			vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32); 
		}
	}

	void Mesh::Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance)
	{
		if (m_HasIndexBuffer) 
		{ 
			vkCmdDrawIndexed(commandBuffer, m_IndexCount, instanceCount, 0, 0, firstInstance); 
		}
		else 
		{ 
			vkCmdDraw(commandBuffer, m_VertexCount, instanceCount, 0, firstInstance); 
		}
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
		attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position) });
		attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Normal) });
		attributeDescriptions.push_back({ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Tangent) });
		attributeDescriptions.push_back({ 3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Bitangent) });
		attributeDescriptions.push_back({ 4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, TexCoord) });

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
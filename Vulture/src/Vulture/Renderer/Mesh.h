#pragma once
#include "pch.h"
#include "Vulkan/Buffer.h"
#include "../Utility/Utility.h"
#include "glm/glm.hpp"

#include "assimp/scene.h"

namespace Vulture
{
	class Mesh
	{
	public:
		struct Vertex
		{
			glm::vec3 Position;
			glm::vec3 Normal;
			glm::vec3 Tangent;
			glm::vec3 Bitangent;
			glm::vec2 TexCoord;

			static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions();
			static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
		};

		void Init(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
		void Init(aiMesh* mesh, const aiScene* scene, glm::mat4 mat = glm::mat4(1.0f));
		void Init(int vertexCount, int indexCount, VkMemoryPropertyFlagBits vertexBufferFlags, VkMemoryPropertyFlagBits indexBufferFlags);
		void Destroy();

		Mesh() = default;
		~Mesh();

		Mesh(const Mesh&) = delete;
		Mesh& operator=(const Mesh&) = delete;

		void Bind(VkCommandBuffer commandBuffer);
		void Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance = 0);

		void UpdateVertexBuffer(VkCommandBuffer cmd, int offset, Buffer* buffer, const std::vector<Vertex>& vertices);


		inline const Buffer* GetVertexBuffer() const { return &m_VertexBuffer; }
		inline Buffer* GetVertexBuffer() { return &m_VertexBuffer; }

		inline const Buffer* GetIndexBuffer() const { return &m_IndexBuffer; }
		inline Buffer* GetIndexBuffer() { return &m_IndexBuffer; }

		inline uint32_t& GetIndexCount() { return m_IndexCount; }
		inline uint32_t& GetVertexCount() { return m_VertexCount; }


		inline bool& HasIndexBuffer() { return m_HasIndexBuffer; }

	private:
		void CreateMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
		void CreateMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 mat = glm::mat4(1.0f));

		void CreateVertexBuffer(const std::vector<Vertex>& vertices);
		void CreateIndexBuffer(const std::vector<uint32_t>& indices);
		void CreateEmptyBuffers(int vertexCount, int indexCount, VkMemoryPropertyFlagBits vertexBufferFlags, VkMemoryPropertyFlagBits indexBufferFlags);

		Buffer m_VertexBuffer;
		uint32_t m_VertexCount = 0;

		bool m_HasIndexBuffer = false;
		Buffer m_IndexBuffer;
		uint32_t m_IndexCount = 0;

		bool m_Initialized = false;
	};
}
#pragma once
#include "pch.h"
#include "Vulkan/Buffer.h"
#include "../Utility/Utility.h"
#include "glm/glm.hpp"

#include "Vulkan/DescriptorSet.h"

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

		struct CreateInfo
		{
			const std::vector<Vertex>* Vertices;
			const std::vector<uint32_t>* Indices;

			VkBufferUsageFlags vertexUsageFlags = 0;
			VkBufferUsageFlags indexUsageFlags = 0;
		};

		void Init(const CreateInfo& createInfo);
		void Init(aiMesh* mesh, const aiScene* scene, glm::mat4 mat = glm::mat4(1.0f), VkBufferUsageFlags customUsageFlags = 0);
		void Destroy();

		Mesh(const CreateInfo& createInfo);
		Mesh() = default;
		~Mesh();

		Mesh(const Mesh&) = delete;
		Mesh& operator=(const Mesh&) = delete;
		Mesh(Mesh&& other) noexcept;
		Mesh& operator=(Mesh&& other) noexcept;

		void Bind(VkCommandBuffer commandBuffer);
		void Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance = 0);

		void UpdateVertexBuffer(const std::vector<Vertex>& vertices, int offset, VkCommandBuffer cmd = 0);
		void UpdateIndexBuffer(const std::vector<uint32_t>& indices, int offset, VkCommandBuffer cmd = 0);

		inline const Buffer* GetVertexBuffer() const { return &m_VertexBuffer; }
		inline Buffer* GetVertexBuffer() { return &m_VertexBuffer; }

		inline const Buffer* GetIndexBuffer() const { return &m_IndexBuffer; }
		inline Buffer* GetIndexBuffer() { return &m_IndexBuffer; }

		inline uint32_t& GetIndexCount() { return m_IndexCount; }
		inline uint32_t& GetVertexCount() { return m_VertexCount; }

		inline bool& HasIndexBuffer() { return m_HasIndexBuffer; }
	private:
		
		void CreateMesh(const CreateInfo& createInfo);
		void CreateMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 mat = glm::mat4(1.0f), VkBufferUsageFlags customUsageFlags = 0);

		void CreateVertexBuffer(const std::vector<Vertex>* const vertices, VkBufferUsageFlags customUsageFlags = 0);
		void CreateIndexBuffer(const std::vector<uint32_t>* const indices, VkBufferUsageFlags customUsageFlags = 0);
		
		Buffer m_VertexBuffer;
		uint32_t m_VertexCount = 0;

		bool m_HasIndexBuffer = false;
		Buffer m_IndexBuffer;
		uint32_t m_IndexCount = 0;

		bool m_Initialized = false;
	};
}
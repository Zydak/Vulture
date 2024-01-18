#pragma once
#include "pch.h"
#include "Mesh.h"
#include "Vulkan/Image.h"

namespace Vulture
{
	struct Material
	{
		glm::vec4 Color;
		glm::vec4 Emissive;
		float Metallic;
		float Roughness;
	};

	class Model
	{
	public:
		Model(const std::string filepath);
		~Model();

		void Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance = 0);

		inline uint32_t GetVertexCount() const { return m_VertexCount; }
		inline uint32_t GetIndexCount() const { return m_IndexCount; }
		inline uint32_t GetMeshCount() const { return (uint32_t)m_Meshes.size(); }
		inline uint32_t GetTextureCount() const { return (uint32_t)m_Textures.size(); }
		inline Ref<Image> GetTexture(int index) const { return m_Textures[index]; }
		inline Mesh& GetMesh(int index) const { return *m_Meshes[index]; }
		inline Material GetMaterial(int index) const { return m_Materials[index]; }
		inline const std::vector<Ref<Mesh>>& GetMeshes() const { return m_Meshes; };
	private:
		void ProcessNode(aiNode* node, const aiScene* scene, int& index);

		std::vector<Ref<Mesh>> m_Meshes;
		std::vector<Material> m_Materials;
		std::vector<Ref<Image>> m_Textures;

		uint32_t m_VertexCount = 0;
		uint32_t m_IndexCount = 0;
	};

}
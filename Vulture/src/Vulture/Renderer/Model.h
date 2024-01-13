#pragma once
#include "pch.h"
#include "Mesh.h"

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

		inline uint32_t GetMeshCount() { return (uint32_t)m_Meshes.size(); }
		inline Mesh& GetMesh(int index) { return *m_Meshes[index]; }
		inline Material GetMaterial(int index) { return m_Materials[index]; }
		inline std::vector<Ref<Mesh>>& GetMeshes() { return m_Meshes; };
	private:
		void ProcessNode(aiNode* node, const aiScene* scene, int& index);

		std::vector<Ref<Mesh>> m_Meshes;
		std::vector<Material> m_Materials;
	};

}
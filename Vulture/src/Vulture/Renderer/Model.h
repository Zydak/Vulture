#pragma once
#include "pch.h"
#include "Mesh.h"

namespace Vulture
{
	class Model
	{
	public:
		Model(const std::string filepath);
		~Model();

		void Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance = 0);

		inline uint32_t GetMeshCount() { return (uint32_t)m_Meshes.size(); }
		inline Mesh& GetMesh(int index) { return *m_Meshes[index]; }
	private:
		void ProcessNode(aiNode* node, const aiScene* scene, int& index);

		std::vector<Ref<Mesh>> m_Meshes;
	};

}
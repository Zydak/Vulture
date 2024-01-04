#include "pch.h"

#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace Vulture
{

	Model::Model(const std::string filepath)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_CalcTangentSpace);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			VL_CORE_ERROR("Failed to load mesh: {0}", importer.GetErrorString());
			return;
		}

		int x = 0;
		ProcessNode(scene->mRootNode, scene, x);
	}

	Model::~Model()
	{

	}

	void Model::Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance)
	{
		for (int i = 0; i < m_Meshes.size(); i++)
		{
			m_Meshes[i]->Bind(commandBuffer);
			m_Meshes[i]->Draw(commandBuffer, instanceCount, firstInstance);
		}
	}

	void Model::ProcessNode(aiNode* node, const aiScene* scene, int& index)
	{
		// process each mesh located at the current node
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			m_Meshes.push_back(std::make_shared<Mesh>());
			m_Meshes[index]->CreateMesh(mesh, scene);
			index++;
		}

		// process each of the children nodes
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene, index);
		}
	}

}

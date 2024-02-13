#include "pch.h"

#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include "AssetManager.h"

namespace Vulture
{

	void Model::Init(const std::string& filepath)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			VL_CORE_ERROR("Failed to load mesh: {0}", importer.GetErrorString());
			return;
		}

		int x = 0;
		ProcessNode(scene->mRootNode, scene, x);

		m_Initialized = true;
	}

	void Model::Destroy()
	{
		m_Meshes.clear();
		m_Materials.clear();
		m_AlbedoTextures.clear();
		m_RoghnessTextures.clear();
		m_MetallnessTextures.clear();
		m_NormalTextures.clear();

		m_VertexCount = 0;
		m_IndexCount = 0;
		m_Initialized = false;
	}

	Model::Model(const std::string& filepath)
	{
		if (m_Initialized)
			Destroy();

		Init(filepath);
	}

	Model::~Model()
	{
		if (m_Initialized)
			Destroy();
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
			glm::mat4 transform = *(glm::mat4*)(&node->mTransformation);
			//transform = glm::transpose(transform);
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			m_Meshes.push_back(std::make_shared<Mesh>());
			m_Meshes[index]->Init(mesh, scene, transform);
			m_VertexCount += m_Meshes[index]->GetVertexCount();
			m_IndexCount += m_Meshes[index]->GetIndexCount();

			VL_CORE_INFO("Loaded mesh: with {0} vertices", m_Meshes[index]->GetVertexCount());

			m_Materials.push_back(Material());
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
			aiColor4D albedoColor(0.0f, 0.0f, 0.0f, 1.0f);
			float roughness = 1.0f;
			float metallic = 0.0f;

			material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);
			material->Get(AI_MATKEY_COLOR_DIFFUSE, albedoColor);
			material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
			material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);

			for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_BASE_COLOR); i++)
			{
				aiString str;
				material->GetTexture(aiTextureType_BASE_COLOR, i, &str);
				m_AlbedoTextures.push_back(AssetManager::LoadTexture(std::string("assets/") + std::string(str.C_Str())));
				VL_CORE_INFO("Loaded texture: {0}", str.C_Str());
			}

			for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_NORMALS); i++)
			{
				aiString str;
				material->GetTexture(aiTextureType_NORMALS, i, &str);
				m_NormalTextures.push_back(AssetManager::LoadTexture(std::string("assets/") + std::string(str.C_Str())));
				VL_CORE_INFO("Loaded texture: {0}", str.C_Str());
			}

			for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS); i++)
			{
				aiString str;
				material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, i, &str);
				m_RoghnessTextures.push_back(AssetManager::LoadTexture(std::string("assets/") + std::string(str.C_Str())));
				VL_CORE_INFO("Loaded texture: {0}", str.C_Str());
			}

			for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_METALNESS); i++)
			{
				aiString str;
				material->GetTexture(aiTextureType_METALNESS, i, &str);
				m_MetallnessTextures.push_back(AssetManager::LoadTexture(std::string("assets/") + std::string(str.C_Str())));
				VL_CORE_INFO("Loaded texture: {0}", str.C_Str());
			}

			Image::CreateInfo info{};
			info.Width = 1;
			info.Height = 1;
			info.Format = VK_FORMAT_R8G8B8A8_UNORM;
			info.Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
			info.Tiling = VK_IMAGE_TILING_OPTIMAL;
			info.SamplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR };

			// Create Empty Texture if none are found
			if (material->GetTextureCount(aiTextureType_BASE_COLOR) == 0)
			{
				m_AlbedoTextures.push_back(AssetManager::CreateTexture(glm::vec4(1.0f), info));
			}
			if (material->GetTextureCount(aiTextureType_NORMALS) == 0)
			{
				m_NormalTextures.push_back(AssetManager::CreateTexture(glm::vec4(0.5f, 0.5f, 1.0f, 1.0f), info));
			}
			if (material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) == 0)
			{
				m_RoghnessTextures.push_back(AssetManager::CreateTexture(glm::vec4(1.0f), info));
			}
			if (material->GetTextureCount(aiTextureType_METALNESS) == 0)
			{
				m_MetallnessTextures.push_back(AssetManager::CreateTexture(glm::vec4(0.0f), info));
			}

			m_Materials[index].Color = glm::vec4(albedoColor.r, albedoColor.g, albedoColor.b, albedoColor.a);
			m_Materials[index].Metallic = metallic;
			m_Materials[index].Roughness = roughness;
			m_Materials[index].Emissive = glm::vec4(emissiveColor.r, emissiveColor.g, emissiveColor.b, 1.0f);

			index++;
		}

		// process each of the children nodes
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene, index);
		}
	}

}

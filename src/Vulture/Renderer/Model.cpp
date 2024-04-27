#include "pch.h"

#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include "Renderer/Renderer.h"

#include "Asset/AssetManager.h"

namespace Vulture
{
	void Model::Init(const std::string& filepath, AssetManager* assetManager)
	{
		Assimp::Importer importer;
		m_AssetManager = assetManager;
		const aiScene* scene = importer.ReadFile(filepath,
			aiProcess_CalcTangentSpace |
			aiProcess_GenSmoothNormals |
			aiProcess_ImproveCacheLocality |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_SplitLargeMeshes |
			aiProcess_Triangulate |
			aiProcess_GenUVCoords |
			aiProcess_SortByPType |
			aiProcess_FindDegenerates |
			aiProcess_FindInvalidData);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			VL_CORE_ERROR("Failed to load model: {0}", importer.GetErrorString());
			return;
		}

		Timer timer;

		int x = 0;
		ProcessNode(scene->mRootNode, scene, x);


		for (int index = 0; index < m_AlbedoTextures.size(); index++)
		{
			CreateTextureSet(index);
		}

		m_Initialized = true;
		VL_CORE_TRACE("Loading model took: {}ms", timer.ElapsedMillis());
	}

	void Model::Destroy()
	{
		m_Meshes.clear();
		m_Materials.clear();
		m_AlbedoTextures.clear();
		m_RoughnessTextures.clear();
		m_MetallnessTextures.clear();
		m_NormalTextures.clear();

		m_VertexCount = 0;
		m_IndexCount = 0;
		m_Initialized = false;
	}

	Model::Model(const std::string& filepath, AssetManager* assetManager)
	{
		if (m_Initialized)
			Destroy();

		Init(filepath, assetManager);
	}

	Model::Model(Model&& other)
	{
		Move(std::move(other));
	}

	Model& Model::operator=(Model&& other)
	{
		Move(std::move(other));
		return *this;
	}

	Model::~Model()
	{
		if (m_Initialized)
			Destroy();
	}

	void Model::Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkPipelineLayout layout)
	{
		for (int i = 0; i < m_Meshes.size(); i++)
		{
			if (layout != 0)
				m_TextureSets[i]->Bind(1, layout, VK_PIPELINE_BIND_POINT_GRAPHICS, commandBuffer);

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
			aiNode* currNode = node;
			while (true)
			{
				if (currNode->mParent)
				{
					currNode = currNode->mParent;
					transform *= *(glm::mat4*)(&currNode->mTransformation);
				}
				else
				{
					break;
				}
			}
			//transform = glm::transpose(transform);
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			m_MeshesNames.push_back(node->mName.C_Str());
			m_Meshes.push_back(std::make_shared<Mesh>());
			m_Meshes[index]->Init(mesh, scene, transform);
			m_VertexCount += m_Meshes[index]->GetVertexCount();
			m_IndexCount += m_Meshes[index]->GetIndexCount();

			m_Materials.push_back(Material());
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 0.0f);
			aiColor4D albedoColor(0.0f, 0.0f, 0.0f, 1.0f);
			float roughness = 1.0f;
			float metallic = 0.0f;

			material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);
			material->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveColor.a);
			material->Get(AI_MATKEY_COLOR_DIFFUSE, albedoColor);
			material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
			material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
			material->Get(AI_MATKEY_REFRACTI, m_Materials[index].Ior);
			m_Materials[index].Ior = glm::max(m_Materials[index].Ior, 1.001f);
			material->Get(AI_MATKEY_CLEARCOAT_FACTOR, m_Materials[index].Clearcoat);
			material->Get(AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR, m_Materials[index].ClearcoatRoughness);

			for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_DIFFUSE); i++)
			{
				aiString str;
				material->GetTexture(aiTextureType_DIFFUSE, i, &str);
				m_AlbedoTextures.push_back(m_AssetManager->LoadAsset(std::string("assets/") + std::string(str.C_Str())));
				VL_CORE_INFO("Loaded texture: {0}", str.C_Str());
			}

			for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_NORMALS); i++)
			{
				aiString str;
				material->GetTexture(aiTextureType_NORMALS, i, &str);
				m_NormalTextures.push_back(m_AssetManager->LoadAsset(std::string("assets/") + std::string(str.C_Str())));
				VL_CORE_INFO("Loaded texture: {0}", str.C_Str());
			}

			for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS); i++)
			{
				aiString str;
				material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, i, &str);
				m_RoughnessTextures.push_back(m_AssetManager->LoadAsset(std::string("assets/") + std::string(str.C_Str())));
				VL_CORE_INFO("Loaded texture: {0}", str.C_Str());
			}

			for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_METALNESS); i++)
			{
				aiString str;
				material->GetTexture(aiTextureType_METALNESS, i, &str);
				m_MetallnessTextures.push_back(m_AssetManager->LoadAsset(std::string("assets/") + std::string(str.C_Str())));
				VL_CORE_INFO("Loaded texture: {0}", str.C_Str());
			}

			// Create Empty Texture if none are found
			if (material->GetTextureCount(aiTextureType_DIFFUSE) == 0)
			{
				m_AlbedoTextures.push_back(m_AssetManager->LoadAsset("assets/white.png"));
			}
			if (material->GetTextureCount(aiTextureType_NORMALS) == 0)
			{
				m_NormalTextures.push_back(m_AssetManager->LoadAsset("assets/empty_normal.png"));
			}
			// TODO: specify format when loading texture
			if (material->GetTextureCount(aiTextureType_METALNESS) == 0)
			{
				m_MetallnessTextures.push_back(m_AssetManager->LoadAsset("assets/white.png"));
			}
			if (material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) == 0)
			{
				m_RoughnessTextures.push_back(m_AssetManager->LoadAsset("assets/white.png"));
			}

			m_Materials[index].Color = glm::vec4(albedoColor.r, albedoColor.g, albedoColor.b, albedoColor.a);
			m_Materials[index].Metallic = metallic;
			m_Materials[index].Roughness = roughness;
			m_Materials[index].Emissive = glm::vec4(emissiveColor.r, emissiveColor.g, emissiveColor.b, emissiveColor.a);

			index++;
		}

		// process each of the children nodes
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene, index);
		}
	}

	void Model::CreateTextureSet(uint32_t index)
	{
		// Textures Set
		Vulture::DescriptorSetLayout::Binding bin1{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
		Vulture::DescriptorSetLayout::Binding bin2{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
		Vulture::DescriptorSetLayout::Binding bin3{ 2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
		Vulture::DescriptorSetLayout::Binding bin4{ 3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };

		m_TextureSets.push_back(std::make_shared<Vulture::DescriptorSet>());
		m_TextureSets[index]->Init(&Vulture::Renderer::GetDescriptorPool(), { bin1, bin2, bin3, bin4 });

		m_AlbedoTextures[index].WaitToLoad();
		m_TextureSets[index]->AddImageSampler(
			0,
			{ Vulture::Renderer::GetSamplerHandle(),
			m_AlbedoTextures[index].GetImage()->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
		);
		m_NormalTextures[index].WaitToLoad();
		m_TextureSets[index]->AddImageSampler(
			1,
			{ Vulture::Renderer::GetSamplerHandle(),
			m_NormalTextures[index].GetImage()->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
		);
		m_RoughnessTextures[index].WaitToLoad();
		m_TextureSets[index]->AddImageSampler(
			2,
			{ Vulture::Renderer::GetSamplerHandle(),
			m_RoughnessTextures[index].GetImage()->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
		);
		m_MetallnessTextures[index].WaitToLoad();
		m_TextureSets[index]->AddImageSampler(
			3,
			{ Vulture::Renderer::GetSamplerHandle(),
			m_MetallnessTextures[index].GetImage()->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
		);

		m_TextureSets[index]->Build();
	}

	void Model::Move(Model&& other)
	{
		if (m_Initialized)
			Destroy();

		m_Initialized = true;
		m_MeshesNames = std::move(other.m_MeshesNames);
		m_Meshes = std::move(other.m_Meshes);
		m_Materials = std::move(other.m_Materials);
		m_AlbedoTextures = std::move(other.m_AlbedoTextures);
		m_NormalTextures = std::move(other.m_NormalTextures);
		m_RoughnessTextures = std::move(other.m_RoughnessTextures);
		m_MetallnessTextures = std::move(other.m_MetallnessTextures);

		m_TextureSets = std::move(other.m_TextureSets);

		m_VertexCount = other.m_VertexCount;
		m_IndexCount = other.m_IndexCount;

		other.m_Initialized = false;
	}

}

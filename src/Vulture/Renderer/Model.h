// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "pch.h"
#include "Mesh.h"
#include "Vulkan/Image.h"
#include "Asset/Asset.h"

namespace Vulture
{
	struct Material
	{
	public:
		glm::vec4 Color;
		glm::vec4 EmissiveColor;
		float Metallic = 0.0f;
		float Roughness = 1.0f;
		float SpecularTint = 0.0f;
		float SpecularStrength = 0.0f;

		float Ior = 1.5f;
		float Transparency = 0.0f;
	private:
		float eta = 0.0f;
	};

	class Model
	{
	public:
		void Init(const std::string& filepath);
		void Destroy();

		Model() = default;
		explicit Model(const std::string& filepath);
		explicit Model(const Model& other) = delete;
		explicit Model(Model&& other) noexcept;

		Model& operator=(const Model& other) = delete;
		Model& operator=(Model&& other) noexcept;

		~Model();

		void Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance = 0, VkPipelineLayout layout = 0);

		inline uint32_t GetAlbedoTextureCount() const { return (uint32_t)m_AlbedoTextures.size(); }
		inline Image* GetAlbedoTexture(int index) const { return m_AlbedoTextures[index].GetImage(); }

		inline uint32_t GetNormalTextureCount() const { return (uint32_t)m_NormalTextures.size(); }
		inline Image* GetNormalTexture(int index) const { return m_NormalTextures[index].GetImage(); }

		inline uint32_t GetRoughnessTextureCount() const { return (uint32_t)m_RoughnessTextures.size(); }
		inline Image* GetRoughnessTexture(int index) const { return m_RoughnessTextures[index].GetImage(); }

		inline uint32_t GetMetallnessTextureCount() const { return (uint32_t)m_MetallnessTextures.size(); }
		inline Image* GetMetallnessTexture(int index) const { return m_MetallnessTextures[index].GetImage(); }

		inline uint32_t GetVertexCount() const { return m_VertexCount; }
		inline uint32_t GetIndexCount() const { return m_IndexCount; }
		inline uint32_t GetMeshCount() const { return (uint32_t)m_Meshes.size(); }

		inline Mesh& GetMesh(int index) const { return *m_Meshes[index]; }
		inline Material GetMaterial(int index) const { return m_Materials[index]; }

		inline const std::vector<Ref<Mesh>>& GetMeshes() const { return m_Meshes; };
		inline const std::vector<Material>& GetMaterials() const { return m_Materials; };
		inline const std::vector<std::string>& GetNames() const { return m_MeshesNames; };
		inline const std::vector<Ref<Vulture::DescriptorSet>>& GetDescriptors() const { return m_TextureSets; };
		inline const glm::mat4& GetMatrix(int index) const { return m_ModelMatrices[index]; }

		inline std::vector<Material>& GetMaterials() { return m_Materials; };
	private:
		void ProcessNode(aiNode* node, const aiScene* scene, int& index);
		void CreateTextureSet(uint32_t index);

		std::vector<std::string> m_MeshesNames;
		std::vector<Ref<Mesh>> m_Meshes;
		std::vector<Material> m_Materials;
		std::vector<glm::mat4> m_ModelMatrices;
		std::vector<AssetHandle> m_AlbedoTextures;
		std::vector<AssetHandle> m_NormalTextures;
		std::vector<AssetHandle> m_RoughnessTextures;
		std::vector<AssetHandle> m_MetallnessTextures;

		std::vector<Ref<Vulture::DescriptorSet>> m_TextureSets;

		uint32_t m_VertexCount = 0;
		uint32_t m_IndexCount = 0;

		bool m_Initialized = false;

		void Reset();
	};
	

	class ModelAsset : public Asset
	{
	public:
		ModelAsset(Model&& model) { Model = std::move(model); };

		explicit ModelAsset(const ModelAsset& other) = delete;
		explicit ModelAsset(ModelAsset&& other) noexcept { Model = std::move(other.Model); };
		ModelAsset& operator=(const ModelAsset& other) = delete;
		ModelAsset& operator=(ModelAsset&& other) noexcept { Model = std::move(other.Model); return *this; };

		virtual AssetType GetAssetType() { return AssetType::Model; }
		Vulture::Model Model;
	};
}
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

		float Ior = 1.5f;
		float SpecTrans = 0.0f;
		float Clearcoat = 0.0f;
		float ClearcoatRoughness = 0.0f;
		float eta = 0.0f;
		float ax = 0.0f;
		float ay = 0.0f;
	};

	class Model
	{
	public:
		void Init(const std::string& filepath);
		void Destroy();

		Model() = default;
		Model(const std::string& filepath);
		~Model();

		void Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance = 0, VkPipelineLayout layout = 0);

		inline uint32_t GetAlbedoTextureCount() const { return (uint32_t)m_AlbedoTextures.size(); }
		inline Ref<Image> GetAlbedoTexture(int index) const { return m_AlbedoTextures[index]; }

		inline uint32_t GetNormalTextureCount() const { return (uint32_t)m_NormalTextures.size(); }
		inline Ref<Image> GetNormalTexture(int index) const { return m_NormalTextures[index]; }

		inline uint32_t GetRoughnessTextureCount() const { return (uint32_t)m_RoughnessTextures.size(); }
		inline Ref<Image> GetRoughnessTexture(int index) const { return m_RoughnessTextures[index]; }

		inline uint32_t GetMetallnessTextureCount() const { return (uint32_t)m_MetallnessTextures.size(); }
		inline Ref<Image> GetMetallnessTexture(int index) const { return m_MetallnessTextures[index]; }

		inline uint32_t GetVertexCount() const { return m_VertexCount; }
		inline uint32_t GetIndexCount() const { return m_IndexCount; }
		inline uint32_t GetMeshCount() const { return (uint32_t)m_Meshes.size(); }

		inline Mesh& GetMesh(int index) const { return *m_Meshes[index]; }
		inline Material GetMaterial(int index) const { return m_Materials[index]; }

		inline const std::vector<Ref<Mesh>>& GetMeshes() const { return m_Meshes; };
		inline const std::vector<Material>& GetMaterials() const { return m_Materials; };
		inline const std::vector<std::string>& GetNames() const { return m_MeshesNames; };
		inline const std::vector<Ref<Vulture::DescriptorSet>>& GetDescriptors() const { return m_TextureSets; };

		inline std::vector<Material>& GetMaterials() { return m_Materials; };
	private:
		void ProcessNode(aiNode* node, const aiScene* scene, int& index);
		void CreateTextureSet(uint32_t index);

		std::vector<std::string> m_MeshesNames;
		std::vector<Ref<Mesh>> m_Meshes;
		std::vector<Material> m_Materials;
		std::vector<Ref<Image>> m_AlbedoTextures;
		std::vector<Ref<Image>> m_NormalTextures;
		std::vector<Ref<Image>> m_RoughnessTextures;
		std::vector<Ref<Image>> m_MetallnessTextures;

		std::vector<Ref<Vulture::DescriptorSet>> m_TextureSets;

		uint32_t m_VertexCount = 0;
		uint32_t m_IndexCount = 0;

		bool m_Initialized = false;
	};

}
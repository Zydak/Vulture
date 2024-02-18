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

		float Sheen = 0.0f;
		float SheenTint = 0.0f;
		float SpecularTint = 0.0f;
		float Ior = 1.01f;
		float Anisotropic = 0.0f;
		float SpecTrans = 0.0f;
		float Clearcoat = 0.0f;
		float eta = 0.0f;
		float Subsurface = 0.0f;
		float ax = 0.0f;
		float ay = 0.0f;
		float ClearcoatRoughness = 0.0f;

		uint32_t mediumType;
		glm::vec4 mediumColor;
		float mediumDensity;
	};

	class Model
	{
	public:
		void Init(const std::string& filepath);
		void Destroy();

		Model() = default;
		Model(const std::string& filepath);
		~Model();

		void Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance = 0);

		inline uint32_t GetAlbedoTextureCount() const { return (uint32_t)m_AlbedoTextures.size(); }
		inline Ref<Image> GetAlbedoTexture(int index) const { return m_AlbedoTextures[index]; }

		inline uint32_t GetNormalTextureCount() const { return (uint32_t)m_NormalTextures.size(); }
		inline Ref<Image> GetNormalTexture(int index) const { return m_NormalTextures[index]; }

		inline uint32_t GetRoughnessTextureCount() const { return (uint32_t)m_RoghnessTextures.size(); }
		inline Ref<Image> GetRoughnessTexture(int index) const { return m_RoghnessTextures[index]; }

		inline uint32_t GetMetallnessTextureCount() const { return (uint32_t)m_MetallnessTextures.size(); }
		inline Ref<Image> GetMetallnessTexture(int index) const { return m_MetallnessTextures[index]; }

		inline uint32_t GetVertexCount() const { return m_VertexCount; }
		inline uint32_t GetIndexCount() const { return m_IndexCount; }
		inline uint32_t GetMeshCount() const { return (uint32_t)m_Meshes.size(); }
		inline Mesh& GetMesh(int index) const { return *m_Meshes[index]; }
		inline Material GetMaterial(int index) const { return m_Materials[index]; }
		inline const std::vector<Ref<Mesh>>& GetMeshes() const { return m_Meshes; };
	private:
		void ProcessNode(aiNode* node, const aiScene* scene, int& index);

		std::vector<Ref<Mesh>> m_Meshes;
		std::vector<Material> m_Materials;
		std::vector<Ref<Image>> m_AlbedoTextures;
		std::vector<Ref<Image>> m_NormalTextures;
		std::vector<Ref<Image>> m_RoghnessTextures;
		std::vector<Ref<Image>> m_MetallnessTextures;

		uint32_t m_VertexCount = 0;
		uint32_t m_IndexCount = 0;

		bool m_Initialized = false;
	};

}
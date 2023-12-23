#pragma once
#include "pch.h"
#include "FontAtlas.h"
#include "Mesh.h"
#include "../Vulkan/Swapchain.h"

namespace Vulture
{
	class Text
	{
	public:
		Text(const std::string& text, Ref<FontAtlas> fontAtlas, const glm::vec4& color, float kerningOffset = 0.0f, int maxLettersCount = 0, bool resizable = false);
		void ChangeText(const std::string& text, float kerningOffset = 0.0f);

		std::string GetTextString() const { return m_Text; }
		inline bool IsResizable() const { return m_Resizable; }
		inline float GetMaxHeight() const { return m_Height; }
		inline float GetMaxWidth() const { return m_Width; }
		Mesh& GetTextMesh(int frameIndex)
		{
			if (m_Resizable)
			{
				return *m_TextMeshes[frameIndex];
			}
			else
			{
				return *m_TextMeshes[0];
			}
		}
		glm::vec4 GetTextColor() const { return m_Color; }
		Ref<FontAtlas> GetFontAtlas() const { return m_FontAtlas; }

		bool UploadToBuffer(int frameIndex, VkCommandBuffer cmdBuffer);
	private:
		void GetTextVertices(std::vector<Mesh::Vertex>& vertices, std::vector<uint32_t>& indices);

		float m_Width = 0;
		float m_Height = 0;
		Ref<FontAtlas> m_FontAtlas;
		float m_KerningOffset = 0.0f;
		std::array<bool, Swapchain::MAX_FRAMES_IN_FLIGHT> m_Changed;
		std::string m_Text;
		std::vector<std::shared_ptr<Mesh>> m_TextMeshes;
		glm::vec4 m_Color;
		bool m_Resizable;
	};
}
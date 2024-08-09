// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include "Text.h"

namespace Vulture
{

	void Text::Init(const CreateInfo& createInfo)
	{
		if (m_Initialized)
			Destroy();

		VL_CORE_ASSERT(createInfo, "Incorectly initialized Text::CreateInfo!");
		m_Text = createInfo.Text;
		m_Color = createInfo.Color;
		m_Resizable = createInfo.Resizable;
		m_KerningOffset = createInfo.KerningOffset;
		m_FontAtlas = createInfo.FontAtlas;
		if (createInfo.Resizable)
		{
			int vertexCount = createInfo.MaxLettersCount * 4;
			int indexCount = createInfo.MaxLettersCount * 6;
			std::vector<Mesh::Vertex> vert(vertexCount);
			std::vector<uint32_t> ind(indexCount);
			m_TextMesh.Init({&vert, &ind, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});
			ChangeText(createInfo.Text);
		}
		else
		{
			std::vector<Mesh::Vertex> vertices;
			std::vector<uint32_t> indices;

			GetTextVertices(vertices, indices);

			m_TextMesh.Init({ &vertices, &indices });
		}

		m_Initialized = true;
	}

	void Text::Destroy()
	{
		if (!m_Initialized)
			return;

		m_TextMesh.Destroy();
		Reset();
	}

	Text::Text(const CreateInfo& createInfo)
	{
		Init(createInfo);
	}

	Text::Text(Text&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Width = std::move(other.m_Width);
		m_Height = std::move(other.m_Height);
		m_FontAtlas = std::move(other.m_FontAtlas);
		m_KerningOffset = std::move(other.m_KerningOffset);
		m_Text = std::move(other.m_Text);
		m_TextMesh = std::move(other.m_TextMesh);
		m_Color = std::move(other.m_Color);
		m_Resizable = std::move(other.m_Resizable);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();
	}

	Text& Text::operator=(Text&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Width = std::move(other.m_Width);
		m_Height = std::move(other.m_Height);
		m_FontAtlas = std::move(other.m_FontAtlas);
		m_KerningOffset = std::move(other.m_KerningOffset);
		m_Text = std::move(other.m_Text);
		m_TextMesh = std::move(other.m_TextMesh);
		m_Color = std::move(other.m_Color);
		m_Resizable = std::move(other.m_Resizable);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	Text::~Text()
	{
		Destroy();
	}

	void Text::ChangeText(const std::string& text, float kerningOffset, VkCommandBuffer cmdBuffer)
	{
		VL_CORE_ASSERT(m_Resizable, "You have to set resizable flag in constructor!");
		m_Text = text;
		m_KerningOffset = kerningOffset;

		std::vector<Mesh::Vertex> vertices;
		std::vector<uint32_t> indices;

		GetTextVertices(vertices, indices);

		m_TextMesh.HasIndexBuffer() = true;
		if (!vertices.empty())
		{
			// Update Device local memory using vkCmdUpdateBuffer
			m_TextMesh.GetVertexBuffer()->WriteToBuffer(vertices.data(), vertices.size() * sizeof(Mesh::Vertex), 0, cmdBuffer);
			m_TextMesh.GetIndexBuffer()->WriteToBuffer(indices.data(), indices.size() * sizeof(uint32_t), 0, cmdBuffer);
		}
		m_TextMesh.GetVertexCount() = (uint32_t)vertices.size();
		m_TextMesh.GetIndexCount() = (uint32_t)indices.size();
	}

	void Text::GetTextVertices(std::vector<Mesh::Vertex>& vertices, std::vector<uint32_t>& indices)
	{
		const auto& fontGeometry = m_FontAtlas->GetGeometry();
		const auto& metrics = fontGeometry.getMetrics();
		std::shared_ptr<Image> fontAtlas = m_FontAtlas->GetAtlasTexture();

		m_Width = 0;
		m_Height = (float)metrics.ascenderY;

		float x = 0.0f;                                              // font advance
		float y = 0.0f;                                              // font advance
		float fontScale = 1.0f / ((float)metrics.ascenderY - (float)metrics.descenderY);    // ascender = from baseline to the highest point. descender = to the lowest point (all glyphs)

		int glyphCount = -1;

		float spaceAdvance = (float)fontGeometry.getGlyph(' ')->getAdvance();
		for (int i = 0; i < m_Text.size(); i++)
		{
			char character = m_Text[i];

			if (character == '\r')
				continue;
			if (character == '\t')
			{
				x += 6.0f * spaceAdvance * fontScale;
				continue;
			}
			if (character == ' ')
			{
				x += spaceAdvance * fontScale;
				continue;
			}
			if (character == '\n')
			{
				x = 0;
				y -= fontScale * (float)metrics.lineHeight;
				continue;
			}
			glyphCount++;
			auto glyph = fontGeometry.getGlyph(character);
			if (!glyph)
				glyph = fontGeometry.getGlyph('?');

			double pl, pb, pr, pt;    // Quad size
			glyph->getQuadPlaneBounds(pl, pb, pr, pt);

			double al, ab, ar, at;    // Tex coords
			glyph->getQuadAtlasBounds(al, ab, ar, at);

			pl *= fontScale;
			pb *= fontScale;
			pr *= fontScale;
			pt *= fontScale;

			pl += x;
			pb += y;
			pr += x;
			pt += y;

			float texelWidth = 1.0f / fontAtlas->GetImageSize().width;
			float texelHeight = 1.0f / fontAtlas->GetImageSize().height;

			al *= texelWidth;
			ab *= texelHeight;
			ar *= texelWidth;
			at *= texelHeight;

			{
				Mesh::Vertex vertex{};
				vertex.Position = glm::vec3(pl, pb, 0.0f);
				vertex.TexCoord = glm::vec2(al, ab);
				vertices.push_back(vertex);
			}

			{
				Mesh::Vertex vertex{};
				vertex.Position = glm::vec3(pl, pt, 0.0f);
				vertex.TexCoord = glm::vec2(al, at);
				vertices.push_back(vertex);
			}

			{
				Mesh::Vertex vertex{};
				vertex.Position = glm::vec3(pr, pt, 0.0f);
				vertex.TexCoord = glm::vec2(ar, at);
				vertices.push_back(vertex);
			}

			{
				Mesh::Vertex vertex{};
				vertex.Position = glm::vec3(pr, pb, 0.0f);
				vertex.TexCoord = glm::vec2(ar, ab);
				vertices.push_back(vertex);
			}

			indices.push_back(0 + (4 * glyphCount));
			indices.push_back(1 + (4 * glyphCount));
			indices.push_back(2 + (4 * glyphCount));
			indices.push_back(0 + (4 * glyphCount));
			indices.push_back(2 + (4 * glyphCount));
			indices.push_back(3 + (4 * glyphCount));

			double advance = glyph->getAdvance();
			x += fontScale * (float)advance + m_KerningOffset;
			if (x + spaceAdvance > m_Width)
				m_Width = x + spaceAdvance; // add spaceAdvance for better look
		}
	}

	void Text::Reset()
	{
		m_Width = 0;
		m_Height = 0;
		m_FontAtlas = nullptr;
		m_KerningOffset = 0.0f;
		m_Text.clear();
		m_Color = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_Resizable = false;
		m_Initialized = false;
	}

}
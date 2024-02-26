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
		for (int i = 0; i < (int)m_Changed.size(); i++)
		{
			m_Changed[i] = false;
		}
		if (createInfo.Resizable)
		{
			int vertexCount = createInfo.MaxLettersCount * 4;
			int indexCount = createInfo.MaxLettersCount * 6;
			ChangeText(createInfo.Text);
			for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				m_TextMeshes.emplace_back(std::make_shared<Mesh>());
				m_TextMeshes[i]->Init(vertexCount, indexCount, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			}
		}
		else
		{
			m_TextMeshes.emplace_back(std::make_shared<Mesh>());
			m_Text = createInfo.Text;

			std::vector<Mesh::Vertex> vertices;
			std::vector<uint32_t> indices;

			GetTextVertices(vertices, indices);

			m_TextMeshes[0]->Init(vertices, indices);
		}

		m_Initialized = true;
	}

	void Text::Destroy()
	{
		m_FontAtlas.reset();
		m_TextMeshes.clear();
		m_Initialized = false;
	}

	Text::Text(const CreateInfo& createInfo)
	{
		Init(createInfo);
	}

	Text::~Text()
	{
		if (m_Initialized)
			Destroy();
	}

	void Text::ChangeText(const std::string& text, float kerningOffset)
	{
		VL_CORE_ASSERT(m_Resizable, "You have to set resizable flag in constructor!");
		m_Text = text;
		m_KerningOffset = kerningOffset;
		for (int i = 0; i < (int)m_Changed.size(); i++)
		{
			m_Changed[i] = true;
		}
	}

	/*
		@return false when buffer was not updated.
		@return true when buffer was updated.
	*/
	bool Text::UploadToBuffer(int frameIndex, VkCommandBuffer cmdBuffer)
	{
		VL_CORE_ASSERT(m_Resizable, "Can't upload to buffer that is not resizable! Set the resizable flag on creation");

		if (m_Changed[frameIndex] == false)
		{
			return false;
		}
		m_Changed[frameIndex] = false;
		
		std::vector<Mesh::Vertex> vertices;
		std::vector<uint32_t> indices;

		GetTextVertices(vertices, indices);

		m_TextMeshes[frameIndex]->HasIndexBuffer() = true;
		if (!vertices.empty())
		{
			// Not sure which way is faster :/

			// Update Device local memory using vkCmdUpdateBuffer
			m_TextMeshes[frameIndex]->GetVertexBuffer()->WriteToBuffer(vertices.data(), vertices.size() * sizeof(Mesh::Vertex), 0, cmdBuffer);
			m_TextMeshes[frameIndex]->GetIndexBuffer()->WriteToBuffer(indices.data(), indices.size() * sizeof(uint32_t), 0, cmdBuffer);

			// Map and memcopy data to host visible memory
			// m_TextMeshes[frameIndex]->GetVertexBuffer()->Map();
			// m_TextMeshes[frameIndex]->GetVertexBuffer()->WriteToBuffer(vertices.data(), vertices.size() * sizeof(Mesh::Vertex), 0);
			// m_TextMeshes[frameIndex]->GetVertexBuffer()->Flush();
			// m_TextMeshes[frameIndex]->GetVertexBuffer()->Unmap();

			// m_TextMeshes[frameIndex]->GetIndexBuffer()->Map();
			// m_TextMeshes[frameIndex]->GetIndexBuffer()->WriteToBuffer(indices.data(), indices.size() * sizeof(uint32_t), 0);
			// m_TextMeshes[frameIndex]->GetIndexBuffer()->Flush();
			// m_TextMeshes[frameIndex]->GetIndexBuffer()->Unmap();
		}
		m_TextMeshes[frameIndex]->GetVertexCount() = (uint32_t)vertices.size();
		m_TextMeshes[frameIndex]->GetIndexCount() = (uint32_t)indices.size();

		return true;
	}

	void Text::GetTextVertices(std::vector<Mesh::Vertex>& vertices, std::vector<uint32_t>& indices)
	{
		m_Width = 0;
		m_Height = 0;

		const auto& fontGeometry = m_FontAtlas->GetGeometry();
		const auto& metrics = fontGeometry.getMetrics();
		std::shared_ptr<Image> fontAtlas = m_FontAtlas->GetAtlasTexture();

		float x = 0.0f;                                              // font advance
		float y = 0.0f;                                              // font advance
		float fontScale = 1.0f / ((float)metrics.ascenderY - (float)metrics.descenderY);    // ascender = from baseline to the highest point. descender = to the lowest point (all glyphs)

		m_Height = (float)metrics.ascenderY;

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

}
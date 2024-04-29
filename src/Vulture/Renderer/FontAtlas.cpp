#include "pch.h"
#include "FontAtlas.h"

#include "Renderer.h"

#include "msdf-atlas-gen/msdf-atlas-gen.h"

namespace Vulture
{
	// T and S are types, N number of channels
	template<typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
	static std::shared_ptr<Image> CreateAtlas(const std::vector<msdf_atlas::GlyphGeometry>& glyphs, const float& width, const float& height)
	{
		// Configuration of signed distance field generator
		msdf_atlas::GeneratorAttributes attributes;
		attributes.config.overlapSupport = true;
		attributes.scanlinePass = true;

		// Generator for glyphs
		msdf_atlas::ImmediateAtlasGenerator<S, N, GenFunc, msdf_atlas::BitmapAtlasStorage<T, N>> generator((int)width, (int)height);
		generator.setAttributes(attributes);
		generator.setThreadCount(4);
		generator.generate(glyphs.data(), (int)glyphs.size());

		msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>)generator.atlasStorage();

		Image::CreateInfo info;
		info.Width = bitmap.width;
		info.Height = bitmap.height;
		info.Format = VK_FORMAT_R8G8B8A8_UNORM;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		Ref<Image> tex = std::make_shared<Image>(info);
		
		Buffer::CreateInfo BufferInfo{};
		BufferInfo.InstanceSize = bitmap.width * bitmap.height * 4;
		BufferInfo.InstanceCount = 1;
		BufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		BufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		Buffer pixelBuffer;
		pixelBuffer.Init(BufferInfo);
		pixelBuffer.Map(bitmap.width * bitmap.height * 4);
		pixelBuffer.WriteToBuffer((void*)bitmap.pixels, bitmap.width * bitmap.height * 4);
		pixelBuffer.Unmap();
		tex->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		tex->CopyBufferToImage(pixelBuffer.GetBuffer(), bitmap.width, bitmap.height);
		tex->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		return tex;
	}

	void FontAtlas::Init(const std::string& filepath, const std::string& fontName, float fontSize, glm::vec2 atlasSize)
	{
		if (m_Initialized)
			Destroy();

		static bool initialized = false;
		static msdfgen::FreetypeHandle* ft;
		if (!initialized)
		{
			ft = msdfgen::initializeFreetype();
			VL_CORE_ASSERT(ft, "Failed to initialize freetype");
			initialized = true;
		}
		msdfgen::FontHandle* font = msdfgen::loadFont(ft, filepath.c_str());
		VL_CORE_ASSERT(font, "Failed to load font: " + filepath);

		static const uint32_t charsetRanges[] =
		{
			0x0020, 0x00FF,
			0,
		};

		msdf_atlas::Charset charset;
		for (int i = 0; i < 2; i += 2)
		{
			for (uint32_t c = charsetRanges[i]; c < charsetRanges[i + 1]; c++)
			{
				charset.add(c);
			}
		}

		m_FontGeometry = msdf_atlas::FontGeometry(&m_Glyphs);
		m_FontGeometry.loadCharset(font, 1.0, charset);

		// This class computes the layout of a static atlas
		msdf_atlas::TightAtlasPacker atlasPacker;
		atlasPacker.setPixelRange(2.0f);
		atlasPacker.setMiterLimit(1.0f);
		atlasPacker.setPadding(0);
		atlasPacker.setScale(fontSize);
		atlasPacker.setDimensions(atlasSize.x, atlasSize.y);
		uint32_t remaining = atlasPacker.pack(m_Glyphs.data(), (int)m_Glyphs.size());
		VL_CORE_ASSERT(remaining == 0, "Failed to pack atlas");

		int width, height;
		atlasPacker.getDimensions(width, height);

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull
#define THREAD_COUNT 4

		uint64_t coloringSeed = 0;
		msdf_atlas::Workload([&glyphs = m_Glyphs, &coloringSeed](int i, int threadNo) -> bool
			{
				uint64_t glyphSeed = (LCG_MULTIPLIER * (coloringSeed ^ i) + LCG_INCREMENT) * !!coloringSeed;
				glyphs[i].edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
				return true;
			}, (int)m_Glyphs.size()).finish(THREAD_COUNT);

		m_AtlasTexture = CreateAtlas<uint8_t, float, 4, msdf_atlas::mtsdfGenerator>(m_Glyphs, (const float)width, (const float)height);

		msdfgen::destroyFont(font);
		msdfgen::deinitializeFreetype(ft);

		DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
		m_DescriptorSet.Init(&Vulture::Renderer::GetDescriptorPool(), { bin });
		m_DescriptorSet.AddImageSampler(0, { Vulture::Renderer::GetSamplerHandle(), m_AtlasTexture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
		m_DescriptorSet.Build();

		m_Initialized = true;
	}

	void FontAtlas::Destroy()
	{
		m_AtlasTexture.reset();
		m_DescriptorSet.Destroy();
		m_Initialized = false;
	}

	FontAtlas::FontAtlas(const std::string& filepath, const std::string& fontName, float fontSize, glm::vec2 atlasSize)
		: m_FontName(fontName), m_Sampler({})
	{
		Init(filepath, fontName, fontSize, atlasSize);
	}

	FontAtlas::~FontAtlas()
	{
		if (m_Initialized)
			Destroy();
	}

}
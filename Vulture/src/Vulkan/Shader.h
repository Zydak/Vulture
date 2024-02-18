#pragma once

#include "pch.h"
#include "Device.h"

#include <shaderc/shaderc.hpp>

namespace Vulture
{
	class Shader
	{
	public:
		struct CreateInfo
		{
			std::string Filepath;
			VkShaderStageFlagBits Type;
		};

		void Init(const CreateInfo& info);
		void Destroy();

		Shader() = default;
		Shader(const CreateInfo& info);
		~Shader();

		std::string ReadFile(const std::string& filepath);
		std::vector<uint32_t> ReadFileVec(const std::string& filepath);
		void WriteFile(const std::string& filepath, const std::string& data);
		void WriteFile(const std::string& filepath, const std::vector<uint32_t>& data);
		std::vector<uint32_t> CompileSource(const std::string& filepath);

		void CreateCacheDir();

		VkPipelineShaderStageCreateInfo GetStageCreateInfo();
		inline VkShaderModule GetModuleHandle() { return m_ModuleHandle; }
		inline VkShaderStageFlagBits GetType() { return m_Type; }

	private:

		shaderc_shader_kind VkStageToScStage(VkShaderStageFlagBits stage);

		VkShaderModule m_ModuleHandle;
		VkShaderStageFlagBits m_Type;
		bool m_Initialzed = false;
	};
}
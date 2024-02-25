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

			std::vector<std::string> Macros;
		};

		void Init(const CreateInfo& info);
		void Destroy();

		Shader() = default;
		Shader(const CreateInfo& info);
		~Shader();

		std::vector<uint32_t> CompileSource(const std::string& filepath, std::vector<std::string> macros);

		void CreateCacheDir();

		VkPipelineShaderStageCreateInfo GetStageCreateInfo();
		inline VkShaderModule GetModuleHandle() { return m_ModuleHandle; }
		inline VkShaderStageFlagBits GetType() { return m_Type; }

		std::string ReadShaderFile(const std::string& filepath);

	private:

		shaderc_shader_kind VkStageToScStage(VkShaderStageFlagBits stage);

		VkShaderModule m_ModuleHandle;
		VkShaderStageFlagBits m_Type;
		bool m_Initialzed = false;
	};
}
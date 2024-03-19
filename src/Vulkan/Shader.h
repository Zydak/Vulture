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

			std::vector<std::string> Defines;
		};

		void Init(const CreateInfo& info);
		void Destroy();

		Shader() = default;
		Shader(const CreateInfo& info);
		~Shader();

		VkPipelineShaderStageCreateInfo GetStageCreateInfo();
		inline VkShaderModule GetModuleHandle() { return m_ModuleHandle; }
		inline VkShaderStageFlagBits GetType() { return m_Type; }
	private:

		std::string ReadShaderFile(const std::string& filepath);
		void CreateCacheDir();
		std::vector<uint32_t> CompileSource(const std::string& filepath, std::vector<std::string> defines);
		shaderc_shader_kind VkStageToScStage(VkShaderStageFlagBits stage);

		VkShaderModule m_ModuleHandle;
		VkShaderStageFlagBits m_Type;
		bool m_Initialzed = false;
	};
}
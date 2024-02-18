#include "pch.h"

#include "Shader.h"
#include "Utility/Utility.h"

#include <shaderc/libshaderc_util/file_finder.h>
#include <shaderc/glslc/file_includer.h>

namespace Vulture
{
	void Shader::Init(const CreateInfo& info)
	{
		if (m_Initialzed)
			Destroy();

		m_Type = info.Type;

		std::vector<uint32_t> data = CompileSource(info.Filepath);

		VkShaderModuleCreateInfo createInfo{};

		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = data.size() * 4.0f;
		createInfo.pCode = data.data();

		VL_CORE_RETURN_ASSERT(vkCreateShaderModule(Device::GetDevice(), &createInfo, nullptr, &m_ModuleHandle),
			VK_SUCCESS,
			"failed to create shader Module"
		);

		Device::SetObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)m_ModuleHandle, info.Filepath.c_str());

		m_Initialzed = true;
	}

	void Shader::Destroy()
	{
		vkDestroyShaderModule(Device::GetDevice(), m_ModuleHandle, nullptr);
		m_Initialzed = false;
	}

	Shader::Shader(const CreateInfo& info)
	{
		Init(info);
	}

	Shader::~Shader()
	{
		if (m_Initialzed)
			Destroy();
	}

	/*
	 * @brief Reads the contents of a file into a vector of characters.
	 *
	 * @param filepath - The path to the file to be read.
	 * @return A vector of characters containing the file contents.
	 */
	std::string Shader::ReadFile(const std::string& filepath)
	{
		std::ifstream file(filepath, std::ios::ate | std::ios::binary);    // ate goes to the end of the file so reading filesize is easier and binary avoids text transformation
		VL_CORE_ASSERT(file.is_open(), "failed to open file: " + filepath);

		uint32_t fileSize = (uint32_t)(file.tellg());    // tellg gets current position in file
		file.seekg(0);    // return to the beginning of the file
		
		std::stringstream ss;

		ss << file.rdbuf();

		file.close();

		std::string str = ss.str();
		return str;
	}

	std::vector<uint32_t> Shader::ReadFileVec(const std::string& filepath)
	{
		std::ifstream file(filepath, std::ios::ate | std::ios::binary);    // ate goes to the end of the file so reading filesize is easier and binary avoids text transformation
		VL_CORE_ASSERT(file.is_open(), "failed to open file: " + filepath);

		uint32_t fileSize = (uint32_t)(file.tellg());    // tellg gets current position in file
		file.seekg(0);    // return to the beginning of the file

		std::vector<uint32_t> vec;
		vec.resize(fileSize / 4.0f);

		file.read(reinterpret_cast<char*>(vec.data()), fileSize);

		return vec;
	}

	void Shader::WriteFile(const std::string& filepath, const std::string& data)
	{
		std::ofstream outputFile(filepath, std::ios::binary); // Open the file for writing

		VL_CORE_ASSERT(outputFile.is_open(), "Failed to open file! {}", filepath);
		outputFile << data;
		outputFile.close();
	}

	void Shader::WriteFile(const std::string& filepath, const std::vector<uint32_t>& data)
	{
		std::ofstream outputFile(filepath, std::ios::binary); // Open the file for writing

		VL_CORE_ASSERT(outputFile.is_open(), "Failed to open file! {}", filepath);

		outputFile.write(reinterpret_cast<const char*>(data.data()), sizeof(uint32_t) * data.size());

		outputFile.close();
	}

	static std::string GetLastPartAfterLastSlash(const std::string& str)
	{
		size_t lastSlashPos = str.find_last_of('/');
		if (lastSlashPos != std::string::npos && lastSlashPos != str.length() - 1) 
		{
			return str.substr(lastSlashPos + 1); // Extract the substring after the last slash
		}
		return str; // Return the original string if no slash is found or if the last character is a slash
	}

	static std::string vectorToString(const std::vector<uint32_t>& vec) 
	{
		std::string result;
		for (uint32_t codePoint : vec) 
		{
			result += static_cast<char>(codePoint);
		}
		return result;
	}

	static std::vector<uint32_t> stringToVector(const std::string& str) 
	{
		std::vector<uint32_t> result;
		for (char c : str) 
		{
			result.push_back(static_cast<uint32_t>(c));
		}
		return result;
	}

	std::vector<uint32_t> Shader::CompileSource(const std::string& filepath)
	{
		CreateCacheDir();
		std::string source = ReadFile(filepath);
		std::string shaderName = GetLastPartAfterLastSlash(filepath);
		if (std::filesystem::exists("CachedShaders/" + shaderName + ".src"))
		{
			std::string cashedSrc = ReadFile("CachedShaders/" + shaderName + ".src");
			if (source == cashedSrc)
			{
				std::vector<uint32_t> data = ReadFileVec("CachedShaders/" + shaderName + ".data");
				return data;
			}
			else
			{
				VL_CORE_INFO("Compiling shader {}", filepath);

				WriteFile("CachedShaders/" + shaderName + ".src", source);

				shaderc::Compiler compiler;
				shaderc::CompileOptions options;
				options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
				options.SetOptimizationLevel(shaderc_optimization_level_performance);
				shaderc_util::FileFinder fileFinder;
				options.SetIncluder(std::make_unique<glslc::FileIncluder>(&fileFinder));

				shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, VkStageToScStage(m_Type), filepath.c_str(), options);

				VL_CORE_RETURN_ASSERT(module.GetCompilationStatus(), 0, "Failed to compile shader! {}", filepath);

				std::vector<uint32_t> data(module.cbegin(), module.cend());

				WriteFile("CachedShaders/" + shaderName + ".data", data);

				return data;
			}
		}
		else
		{
			VL_CORE_INFO("Compiling shader {}", filepath);

			WriteFile("CachedShaders/" + shaderName + ".src", source);

			shaderc::Compiler compiler;
			shaderc::CompileOptions options;
			options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
			shaderc_util::FileFinder fileFinder;
			options.SetIncluder(std::make_unique<glslc::FileIncluder>(&fileFinder));

			shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, VkStageToScStage(m_Type), filepath.c_str(), options);

			if (module.GetCompilationStatus() != 0)
				VL_CORE_ERROR("{}", module.GetErrorMessage());

			std::vector<uint32_t> data(module.cbegin(), module.cend());

			WriteFile("CachedShaders/" + shaderName + ".data", data);

			return data;
		}
	}

	void Shader::CreateCacheDir()
	{
		if (!std::filesystem::exists("CachedShaders"))
		{
			std::filesystem::create_directory("CachedShaders");
		}
	}

	VkPipelineShaderStageCreateInfo Shader::GetStageCreateInfo()
	{
		VkPipelineShaderStageCreateInfo stage;
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.stage = m_Type;
		stage.module = m_ModuleHandle;
		stage.pName = "main";
		stage.flags = 0;
		stage.pNext = nullptr;
		stage.pSpecializationInfo = nullptr;

		return stage;
	}

	shaderc_shader_kind Shader::VkStageToScStage(VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:
			return shaderc_vertex_shader;
			break;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return shaderc_tess_control_shader;
			break;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return shaderc_tess_evaluation_shader;
			break;
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return shaderc_geometry_shader;
			break;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return shaderc_fragment_shader;
			break;
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return shaderc_compute_shader;
			break;
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
			return shaderc_raygen_shader;
			break;
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
			return shaderc_anyhit_shader;
			break;
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			return shaderc_closesthit_shader;
			break;
		case VK_SHADER_STAGE_MISS_BIT_KHR:
			return shaderc_miss_shader;
			break;
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			return shaderc_intersection_shader;
			break;
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			return shaderc_callable_shader;
			break;
		case VK_SHADER_STAGE_TASK_BIT_EXT:
			return shaderc_task_shader;
			break;
		case VK_SHADER_STAGE_MESH_BIT_EXT:
			return shaderc_mesh_shader;
			break;
		default:
			VL_CORE_ASSERT(false, "Incorrect shader type");
			break;
		}
	}
}
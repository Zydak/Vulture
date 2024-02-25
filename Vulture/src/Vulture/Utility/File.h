#pragma once

#include "pch.h"

namespace Vulture
{
	class File
	{
	public:

		template<typename T>
		static void WriteToFile(T* data, uint32_t byteSize, const std::string& filepath)
		{
			std::ofstream outputFile(filepath, std::ios::binary); // Open the file for writing

			VL_CORE_ASSERT(outputFile.is_open(), "Failed to open file! {}", filepath);

			outputFile.write(reinterpret_cast<const char*>(data), byteSize);

			outputFile.close();
		}

		template<typename T>
		static void ReadFromFile(T* data, uint32_t byteSize, const std::string& filepath, uint32_t offset = 0)
		{
			std::ifstream file(filepath, std::ios::binary);
			VL_CORE_ASSERT(file.is_open(), "failed to open file: " + filepath);

			file.seekg(offset);

			file.read(reinterpret_cast<char*>(data), byteSize);
		}

		template<typename T>
		static void ReadFromFileVec(std::vector<T>& data, const std::string& filepath)
		{
			std::ifstream file(filepath, std::ios::ate | std::ios::binary);
			VL_CORE_ASSERT(file.is_open(), "failed to open file: " + filepath);

			uint32_t fileSize = (uint32_t)(file.tellg());
			file.seekg(0);

			uint32_t TSize = sizeof(T);
			data.resize(fileSize / TSize);

			file.read(reinterpret_cast<char*>(data.data()), fileSize);
		}

		static std::string ReadFromFile(const std::string filepath)
		{
			std::ifstream file(filepath, std::ios::ate | std::ios::binary);
			VL_CORE_ASSERT(file.is_open(), "failed to open file: " + filepath);

			uint32_t fileSize = (uint32_t)(file.tellg());
			file.seekg(0);

			std::stringstream ss;

			ss << file.rdbuf();

			file.close();

			std::string str = ss.str();
			return str;
		}
	};
}
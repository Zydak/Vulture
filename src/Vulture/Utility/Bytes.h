#pragma once
#include "pch.h"

namespace Vulture
{
	namespace Bytes
	{
		static std::vector<char> ToBytes(void* data, uint32_t size)
		{
			std::vector<char> bytes(size);
			memcpy(bytes.data(), data, size);

			return bytes;
		}

		template<typename T>
		static T FromBytes(const std::vector<char>& bytes)
		{
			T object;
			memcpy(&object, bytes.data(), bytes.size());

			return object;
		}
	}
}
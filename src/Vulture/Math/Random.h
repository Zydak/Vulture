// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once

#include "glm/glm.hpp"

namespace Vulture
{
	static uint32_t PCG(uint32_t& seed)
	{
		uint32_t state = seed * 747796405u + 2891336453u;
		uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
		seed = (word >> 22u) ^ word;
		return seed;
	}

	static float Random(uint32_t& seed)
	{
		seed = PCG(seed);
		return float(seed) / float(0xffffffff);
	}
}
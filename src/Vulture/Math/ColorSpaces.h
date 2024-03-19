#pragma once
#include "glm/glm.hpp"

namespace Vulture
{
	static const glm::mat3 lab2cone = glm::mat3(
		+4.0767416621, -1.2684380046, -0.0041960863,
		-3.3077115913, +2.6097574011, -0.7034186147,
		+0.2309699292, -0.3413193965, +1.7076147010);

	static const glm::mat3 cone2lrgb = glm::mat3(
		1, 1, 1,
		+0.3963377774f, -0.1055613458f, -0.0894841775f,
		+0.2158037573f, -0.0638541728f, -1.2914855480f);


	static glm::vec3 OKLABtoRGB(glm::vec3 col) {
		col = cone2lrgb * col;
		col = col * col * col;
		col = lab2cone * col;
		return col;
	}

	static glm::vec3 OKLCHtoRGB(glm::vec3 col)
	{
		glm::vec3 oklab = glm::vec3(0.0f);
		oklab.r = col.r;
		oklab.g = col.g * cos(col.b);
		oklab.b = col.g * sin(col.b);

		return OKLABtoRGB(oklab);
	}
}
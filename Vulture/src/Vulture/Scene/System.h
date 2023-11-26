#pragma once
#include "pch.h"

namespace Vulture
{
	class SystemInterface
	{
	public:
		virtual void OnCreate() = 0;
		virtual void OnUpdate(double deltaTime) = 0;
		virtual void OnDestroy() = 0;
	};
}
// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

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
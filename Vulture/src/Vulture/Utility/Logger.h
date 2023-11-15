#pragma once

#include "pch.h"

#include "spdlog/spdlog.h"

namespace Vulture
{
	class Logger
	{
	public:
		static void Init();
		~Logger();

		inline static std::shared_ptr<spdlog::logger> GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger> GetClientLogger() { return s_CoreLogger; }
	private:
		Logger() {};

		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

// Core log macros
#ifdef DIST
#define VL_CORE_ERROR(...)
#define VL_CORE_WARN(...)
#define VL_CORE_INFO(...)
#define VL_CORE_TRACE(...)
#else
#define VL_CORE_ERROR(...)	::Vulture::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define VL_CORE_WARN(...)	::Vulture::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define VL_CORE_INFO(...)	::Vulture::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define VL_CORE_TRACE(...)	::Vulture::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#endif

// Client log macros
#ifdef DIST
#define VL_ERROR(...)
#define VL_WARN(...)
#define VL_INFO(...)
#define VL_TRACE(...)
#else
#define VL_ERROR(...)	::Vulture::Logger::GetClientLogger()->error(__VA_ARGS__)
#define VL_WARN(...)	::Vulture::Logger::GetClientLogger()->warn(__VA_ARGS__)
#define VL_INFO(...)	::Vulture::Logger::GetClientLogger()->info(__VA_ARGS__)
#define VL_TRACE(...)	::Vulture::Logger::GetClientLogger()->trace(__VA_ARGS__)
#endif
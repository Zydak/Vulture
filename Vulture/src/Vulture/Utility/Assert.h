#include "pch.h"
#pragma warning(push, 0)
//
// Core assert
//

#ifndef DIST

#if defined(WIN)
#define VL_CORE_ASSERT(condition, ...)\
		if(!(condition)) {\
			VL_CORE_ERROR(__VA_ARGS__);\
			__debugbreak();\
		}
#else
#define VL_CORE_ASSERT(condition, ...)\
		if(!(condition)) {\
			VL_CORE_ERROR(__VA_ARGS__);\
			__builtin_trap();\
		}
#endif 

#else
#define VL_CORE_ASSERT(condition, ...)
#endif

// return value assert
#ifndef DIST

#if defined(WIN)
#define VL_CORE_RETURN_ASSERT(function, value, ...)\
{\
		auto val = function;\
		if(val != value) {\
			VL_CORE_ERROR("Expected: {}, Actual: {}", value, val);\
			VL_CORE_ERROR(__VA_ARGS__);\
			__debugbreak();\
		}\
}
#else
#define VL_CORE_RETURN_ASSERT(function, value, ...)\
		if(function != value) {\
			VL_CORE_ERROR(__VA_ARGS__);\
			__builtin_trap();\
		}
#endif 

#else
#define VL_CORE_RETURN_ASSERT(function, value, ...) function
#endif

//
// Client assert
//

#ifndef DIST

#if defined(WIN)
#define VL_ASSERT(condition, ...)\
		if(!(condition)) {\
			VL_ERROR(__VA_ARGS__);\
			__debugbreak();\
		}
#else
#define VL_ASSERT(condition, ...)\
		if(!(condition)) {\
			VL_ERROR(__VA_ARGS__);\
			__builtin_trap();\
		}
#endif 

#else
#define VL_ASSERT(condition, ...)
#endif

// return value assert
#ifndef DIST

#if defined(WIN)
#define VL_RETURN_ASSERT(function, value, ...)\
		if(function != value) {\
			VL_ERROR(__VA_ARGS__);\
			__debugbreak();\
		}
#else
#define VL_RETURN_ASSERT(function, value, ...)\
		if(function != value) {\
			VL_ERROR(__VA_ARGS__);\
			__builtin_trap();\
		}
#endif 

#else
#define VL_RETURN_ASSERT(function, value, ...) function
#endif
#pragma warning(pop)
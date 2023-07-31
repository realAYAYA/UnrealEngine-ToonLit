// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !defined(TRACE_UE_COMPAT_LAYER)
#	if defined(__UNREAL__) && __UNREAL__
#		define TRACE_UE_COMPAT_LAYER	0
#	else
#		define TRACE_UE_COMPAT_LAYER	1
#	endif
#endif

#if TRACE_UE_COMPAT_LAYER

// platform defines
#define PLATFORM_ANDROID	0
#define PLATFORM_APPLE		0
#define PLATFORM_HOLOLENS	0
#define PLATFORM_MAC		0
#define PLATFORM_UNIX		0
#define PLATFORM_WINDOWS	0

#ifdef _WIN32
#	undef PLATFORM_WINDOWS
#	define PLATFORM_WINDOWS			1
#elif defined(__linux__)
#	undef  PLATFORM_UNIX
#	define PLATFORM_UNIX			1
#elif defined(__APPLE__)
#	undef  PLATFORM_MAC
#	define PLATFORM_MAC				1
#	undef  PLATFORM_APPLE
#	define PLATFORM_APPLE			1
#endif

#if PLATFORM_MAC
#endif

// arch defines
#if defined(__amd64__) || defined(_M_X64)
#	define PLATFORM_CPU_X86_FAMILY	1
#	define PLATFORM_CPU_ARM_FAMILY	0
#	define PLATFORM_64BITS			1
#	define PLATFORM_CACHE_LINE_SIZE	64
#elif defined(__arm64__) || defined(_M_ARM64)
#	define PLATFORM_CPU_X86_FAMILY	0
#	define PLATFORM_CPU_ARM_FAMILY	1
#	define PLATFORM_64BITS			1
#	define PLATFORM_CACHE_LINE_SIZE	64
#else
#	error Unknown architecture
#endif

// external includes
#include <cstddef>
#include <cstdint>
#include <utility>

#if PLATFORM_WINDOWS
#	if !defined(WIN32_LEAN_AND_MEAN)
#		define WIN32_LEAN_AND_MEAN
#	endif
#	if !defined(NOGDI)
#		define NOGDI
#	endif
#	if !defined(NOMINMAX)
#		define NOMINMAX
#	endif
#	include <Windows.h>
#endif

// types
using uint8  = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using int8	= int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using UPTRINT = uintptr_t;
using PTRINT  = intptr_t;

using SIZE_T = size_t;

#if PLATFORM_WINDOWS
#	undef TEXT
#endif
#define TEXT(x)	x
#define TCHAR	ANSICHAR
using ANSICHAR	= char;
using WIDECHAR	= wchar_t;

// keywords
#if defined(_MSC_VER)
#	define FORCENOINLINE	__declspec(noinline)
#	define FORCEINLINE		__forceinline
#else
#	define FORCENOINLINE	inline __attribute__((noinline))
#	define FORCEINLINE		inline __attribute__((always_inline))
#endif

#if defined(_MSC_VER)
#	define LIKELY(x)		x
#	define UNLIKELY(x)		x
#else
#	define LIKELY(x)		__builtin_expect(!!(x), 1)
#	define UNLIKELY(x)		__builtin_expect(!!(x), 0)
#endif

#define UE_ARRAY_COUNT(x)	(sizeof(x) / sizeof(x[0]))

// so/dll
#if defined(TRACE_DLL_EXPORT)
#	if PLATFORM_WINDOWS && defined(TRACE_DLL_EXPORT)
#		if TRACE_IMPLEMENT
#			define TRACELOG_API __declspec(dllexport)
#		else
#			define TRACELOG_API __declspec(dllimport)
#		endif
#	else
#		define TRACELOG_API		__attribute__ ((visibility ("default")))
#	endif
#else
#	define TRACELOG_API
#endif

#if !defined(IS_MONOLITHIC)
#	define IS_MONOLITHIC				0
#endif

// misc defines
#define THIRD_PARTY_INCLUDES_END
#define THIRD_PARTY_INCLUDES_START
#define TRACE_ENABLED					1
#define TRACE_PRIVATE_CONTROL_ENABLED	0
#define TRACE_PRIVATE_EXTERNAL_LZ4		1
#define UE_TRACE_ENABLED				TRACE_ENABLED
#define UE_BUILD_SHIPPING				1

// api
template <typename T>
inline auto Forward(T t)
{
	return std::forward<T>(t);
}

#endif // TRACE_UE_COMPAT_LAYER

#include <cstring>
#include "lz4.h"

#if PLATFORM_WINDOWS
#	pragma warning(push)
#	pragma warning(disable : 4200) // zero-sized arrays
#	pragma warning(disable : 4201) // anonymous structs
#	pragma warning(disable : 4127) // conditional expr. is constant
#endif

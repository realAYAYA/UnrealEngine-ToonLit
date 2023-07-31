// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreTypes.h"
#include <string.h>
#include <type_traits>

/**
 * Used to declare an untyped array of data with compile-time alignment.
 * It needs to use template specialization as the MS_ALIGN and GCC_ALIGN macros require literal parameters.
 */
template<int32 Size,uint32 Alignment>
struct TAlignedBytes; // this intentionally won't compile, we don't support the requested alignment

/** Unaligned storage. */
template<int32 Size>
struct TAlignedBytes<Size,1>
{
	uint8 Pad[Size];
};


// C++/CLI doesn't support alignment of native types in managed code, so we enforce that the element
// size is a multiple of the desired alignment
#ifdef __cplusplus_cli
	#define IMPLEMENT_ALIGNED_STORAGE(Align) \
		template<int32 Size>        \
		struct TAlignedBytes<Size,Align> \
		{ \
			uint8 Pad[Size]; \
			static_assert(Size % Align == 0, "CLR interop types must not be aligned."); \
		};
#else
/** A macro that implements TAlignedBytes for a specific alignment. */
#define IMPLEMENT_ALIGNED_STORAGE(Align) \
	template<int32 Size>        \
	struct TAlignedBytes<Size,Align> \
	{ \
		struct MS_ALIGN(Align) TPadding \
		{ \
			uint8 Pad[Size]; \
		} GCC_ALIGN(Align); \
		TPadding Padding; \
	};
#endif

// Implement TAlignedBytes for these alignments.
IMPLEMENT_ALIGNED_STORAGE(64);
IMPLEMENT_ALIGNED_STORAGE(32);
IMPLEMENT_ALIGNED_STORAGE(16);
IMPLEMENT_ALIGNED_STORAGE(8);
IMPLEMENT_ALIGNED_STORAGE(4);
IMPLEMENT_ALIGNED_STORAGE(2);

#undef IMPLEMENT_ALIGNED_STORAGE

/** An untyped array of data with compile-time alignment and size derived from another type. */
template<typename ElementType>
struct TTypeCompatibleBytes :
	public TAlignedBytes<
		sizeof(ElementType),
		alignof(ElementType)
		>
{
	using ElementTypeAlias_NatVisHelper = ElementType;
	ElementType*		GetTypedPtr()		{ return (ElementType*)this;  }
	const ElementType*	GetTypedPtr() const	{ return (const ElementType*)this; }
};

template <
	typename ToType,
	typename FromType,
	std::enable_if_t<sizeof(ToType) == sizeof(FromType) && std::is_trivially_copyable_v<ToType> && std::is_trivially_copyable_v<FromType>>* = nullptr
>
inline ToType BitCast(const FromType& From)
{
// Ensure we can use this builtin - seems to be present on Clang 9, GCC 11 and MSVC 19.26,
// but gives spurious "non-void function 'BitCast' should return a value" errors on some
// Mac and Android toolchains when building PCHs, so avoid those.
#if (PLATFORM_APPLE && __clang_major__ >= 13) || (PLATFORM_ANDROID && __clang_major__ >= 13) || (PLATFORM_SWITCH && __clang_major__ >= 13) || (!PLATFORM_APPLE && !PLATFORM_ANDROID && !PLATFORM_SWITCH && defined(__clang__) && __clang_major__ >= 9) || (defined(__GNUC__) && __GNUC__ >= 11) || (defined(_MSC_VER) && _MSC_VER >= 1926)
	return __builtin_bit_cast(ToType, From);
#else
	TTypeCompatibleBytes<ToType> Result;
	memcpy(&Result, &From, sizeof(ToType));
	return *Result.GetTypedPtr();
#endif
}

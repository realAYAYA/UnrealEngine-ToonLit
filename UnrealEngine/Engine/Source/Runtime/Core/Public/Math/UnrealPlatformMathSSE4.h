// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_SKIP - Included through other header

#include "HAL/Platform.h"
#include "Math/UnrealPlatformMathSSE.h"

// UE5.2+ requires SSE4.2

// We have to retain this #if because it's pulled in via the linux header chain
// for all platforms at the moment and we rely on the parent class to implement
// the functions
#if PLATFORM_MAYBE_HAS_SSE4_1
#include <smmintrin.h>

namespace UE4
{
namespace SSE4
{
	FORCEINLINE float TruncToFloat(float F)
	{
		return _mm_cvtss_f32(_mm_round_ps(_mm_set_ss(F), 3));
	}

	FORCEINLINE double TruncToDouble(double F)
	{
		return _mm_cvtsd_f64(_mm_round_pd(_mm_set_sd(F), 3));
	}

	FORCEINLINE float FloorToFloat(float F)
	{
		return _mm_cvtss_f32(_mm_floor_ps(_mm_set_ss(F)));
	}

	FORCEINLINE double FloorToDouble(double F)
	{
		return _mm_cvtsd_f64(_mm_floor_pd(_mm_set_sd(F)));
	}

	FORCEINLINE float RoundToFloat(float F)
	{
		return FloorToFloat(F + 0.5f);
	}

	FORCEINLINE double RoundToDouble(double F)
	{
		return FloorToDouble(F + 0.5);
	}

	FORCEINLINE float CeilToFloat(float F)
	{
		return _mm_cvtss_f32(_mm_ceil_ps(_mm_set_ss(F)));
	}

	FORCEINLINE double CeilToDouble(double F)
	{
		return _mm_cvtsd_f64(_mm_ceil_pd(_mm_set_sd(F)));
	}
}
}

#endif // PLATFORM_MAYBE_HAS_SSE4_1

#define UNREALPLATFORMMATH_SSE4_1_ENABLED PLATFORM_ALWAYS_HAS_SSE4_1

template<class Base>
struct TUnrealPlatformMathSSE4Base : public TUnrealPlatformMathSSEBase<Base>
{
#if UNREALPLATFORMMATH_SSE4_1_ENABLED

	// Truncate

	static FORCEINLINE float TruncToFloat(float F)
	{
		return UE4::SSE4::TruncToFloat(F);
	}

	static FORCEINLINE double TruncToDouble(double F)
	{
		return UE4::SSE4::TruncToDouble(F);
	}

	// Round

	static FORCEINLINE float RoundToFloat(float F)
	{
		return UE4::SSE4::RoundToFloat(F);
	}

	static FORCEINLINE double RoundToDouble(double F)
	{
		return UE4::SSE4::RoundToDouble(F);
	}

	// Floor

	static FORCEINLINE float FloorToFloat(float F)
	{
		return UE4::SSE4::FloorToFloat(F);
	}

	static FORCEINLINE double FloorToDouble(double F)
	{
		return UE4::SSE4::FloorToDouble(F);
	}

	// Ceil

	static FORCEINLINE float CeilToFloat(float F)
	{
		return UE4::SSE4::CeilToFloat(F);
	}

	static FORCEINLINE double CeilToDouble(double F)
	{
		return UE4::SSE4::CeilToDouble(F);
	}


	//
	// Wrappers for overloads in the base, required since calls declared in base struct won't redirect back to this class
	//

	static FORCEINLINE double TruncToFloat(double F) { return TruncToDouble(F); }
	static FORCEINLINE double RoundToFloat(double F) { return RoundToDouble(F); }
	static FORCEINLINE double FloorToFloat(double F) { return FloorToDouble(F); }
	static FORCEINLINE double CeilToFloat(double F) { return CeilToDouble(F); }

#endif // UNREALPLATFORMMATH_SSE4_ENABLED
};

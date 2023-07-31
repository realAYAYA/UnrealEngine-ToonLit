// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "Math/UnrealPlatformMathSSE4.h"

/**
* Microsoft base implementation of the Math OS functions
**/
struct FMicrosoftPlatformMathBase : public TUnrealPlatformMathSSE4Base<FGenericPlatformMath>
{
#if PLATFORM_ENABLE_VECTORINTRINSICS
	static FORCEINLINE bool IsNaN( float A ) { return _isnan(A) != 0; }
	static FORCEINLINE bool IsNaN(double A) { return _isnan(A) != 0; }
	static FORCEINLINE bool IsFinite( float A ) { return _finite(A) != 0; }
	static FORCEINLINE bool IsFinite(double A) { return _finite(A) != 0; }

	#pragma intrinsic(_BitScanReverse)

	static FORCEINLINE uint32 FloorLog2(uint32 Value)
	{
		// Use BSR to return the log2 of the integer
		// return 0 if value is 0
		unsigned long BitIndex;
		return _BitScanReverse(&BitIndex, Value) ? BitIndex : 0;
	}
	static FORCEINLINE uint8 CountLeadingZeros8(uint8 Value)
	{
		unsigned long BitIndex;
		_BitScanReverse(&BitIndex, uint32(Value)*2 + 1);
		return uint8(8 - BitIndex);
	}

	static FORCEINLINE uint32 CountTrailingZeros(uint32 Value)
	{
		// return 32 if value was 0
		unsigned long BitIndex;	// 0-based, where the LSB is 0 and MSB is 31
		return _BitScanForward( &BitIndex, Value ) ? BitIndex : 32;
	}

	static FORCEINLINE uint32 CeilLogTwo( uint32 Arg )
	{
		// if Arg is 0, change it to 1 so that we return 0
		Arg = Arg ? Arg : 1;
		return 32 - CountLeadingZeros(Arg - 1);
	}

	static FORCEINLINE uint32 RoundUpToPowerOfTwo(uint32 Arg)
	{
		return 1 << CeilLogTwo(Arg);
	}

	static FORCEINLINE uint64 RoundUpToPowerOfTwo64(uint64 Arg)
	{
		return uint64(1) << CeilLogTwo64(Arg);
	}

#if PLATFORM_64BITS

	#pragma intrinsic(_BitScanReverse64)

	static FORCEINLINE uint64 FloorLog2_64(uint64 Value)
	{
		unsigned long BitIndex;
		return _BitScanReverse64(&BitIndex, Value) ? BitIndex : 0;
	}

	static FORCEINLINE uint64 CeilLogTwo64(uint64 Arg)
	{
		// if Arg is 0, change it to 1 so that we return 0
		Arg = Arg ? Arg : 1;
		return 64 - CountLeadingZeros64(Arg - 1);
	}

	static FORCEINLINE uint64 CountLeadingZeros64(uint64 Value)
	{
		//https://godbolt.org/z/Ejh5G4vPK	
		// return 64 if value if was 0
		unsigned long BitIndex;
		if ( ! _BitScanReverse64(&BitIndex, Value) ) BitIndex = -1;
        return 63 - BitIndex;
	}

	static FORCEINLINE uint64 CountTrailingZeros64(uint64 Value)
	{
		// return 64 if Value is 0
		unsigned long BitIndex;	// 0-based, where the LSB is 0 and MSB is 63
		return _BitScanForward64( &BitIndex, Value ) ? BitIndex : 64;
	}

	static FORCEINLINE uint32 CountLeadingZeros(uint32 Value)
	{
		// return 32 if value is zero
		unsigned long BitIndex;
		_BitScanReverse64(&BitIndex, uint64(Value)*2 + 1);
		return 32 - BitIndex;
	}

#else // 32-bit

	static FORCEINLINE uint32 CountLeadingZeros(uint32 Value)
	{
		// return 32 if value is zero
		unsigned long BitIndex;
		if ( ! _BitScanReverse(&BitIndex, Value) ) BitIndex = -1;
        return 31 - BitIndex;
	}

#endif

#if PLATFORM_ENABLE_POPCNT_INTRINSIC
	static FORCEINLINE int32 CountBits(uint64 Bits)
	{
		return _mm_popcnt_u64(Bits);
	}
#endif

#endif
};

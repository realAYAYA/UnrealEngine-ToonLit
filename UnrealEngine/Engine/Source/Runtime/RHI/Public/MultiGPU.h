// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MultiGPU.h: Multi-GPU support
=============================================================================*/

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"

#if PLATFORM_DESKTOP
#define WITH_MGPU 1	// Explicit MGPU
#else
#define WITH_MGPU 0
#endif

#if WITH_MGPU
#define MAX_NUM_GPUS 8
extern RHI_API uint32 GNumExplicitGPUsForRendering;
extern RHI_API uint32 GVirtualMGPU;
#else
#define MAX_NUM_GPUS 1
#define GNumExplicitGPUsForRendering 1
#define GVirtualMGPU 0
#endif

/** A mask where each bit is a GPU index. Can not be empty so that non SLI platforms can optimize it to be always 1.  */
struct FRHIGPUMask
{
private:

	uint32 GPUMask;

	FORCEINLINE explicit FRHIGPUMask(uint32 InGPUMask) : GPUMask(InGPUMask)
	{
#if WITH_MGPU
		check(InGPUMask != 0);
#else
		check(InGPUMask == 1);
#endif
	}

public:

	FORCEINLINE FRHIGPUMask() : FRHIGPUMask(FRHIGPUMask::GPU0())
	{
	}

	FORCEINLINE static FRHIGPUMask FromIndex(uint32 GPUIndex) { return FRHIGPUMask(1 << GPUIndex); }

	FORCEINLINE uint32 ToIndex() const
	{
#if WITH_MGPU
		check(HasSingleIndex());
		return FMath::CountTrailingZeros(GPUMask);
#else
		return 0;
#endif
	}

	FORCEINLINE bool HasSingleIndex() const
	{
#if WITH_MGPU
		return FMath::IsPowerOfTwo(GPUMask);
#else
		return true;
#endif
	}

	FORCEINLINE uint32 GetNumActive() const
	{
#if WITH_MGPU
		return FPlatformMath::CountBits(GPUMask);
#else
		return 1;
#endif
	}

	FORCEINLINE uint32 GetLastIndex() const
	{
#if WITH_MGPU
		return FPlatformMath::FloorLog2(GPUMask);
#else
		return 0;
#endif
	}

	FORCEINLINE uint32 GetFirstIndex() const
	{
#if WITH_MGPU
		return FPlatformMath::CountTrailingZeros(GPUMask);
#else
		return 0;
#endif
	}

	FORCEINLINE bool Contains(uint32 GPUIndex) const { return (GPUMask & (1 << GPUIndex)) != 0; }
	FORCEINLINE bool ContainsAll(const FRHIGPUMask& Rhs) const { return (GPUMask & Rhs.GPUMask) == Rhs.GPUMask; }
	FORCEINLINE bool Intersects(const FRHIGPUMask& Rhs) const { return (GPUMask & Rhs.GPUMask) != 0; }

	FORCEINLINE bool operator ==(const FRHIGPUMask& Rhs) const { return GPUMask == Rhs.GPUMask; }
	FORCEINLINE bool operator !=(const FRHIGPUMask& Rhs) const { return GPUMask != Rhs.GPUMask; }

	void operator |=(const FRHIGPUMask& Rhs) { GPUMask |= Rhs.GPUMask; }
	void operator &=(const FRHIGPUMask& Rhs) { GPUMask &= Rhs.GPUMask; }

	FORCEINLINE uint32 GetNative() const { return GVirtualMGPU ? 1 : GPUMask; }

	FORCEINLINE FRHIGPUMask operator &(const FRHIGPUMask& Rhs) const
	{
		return FRHIGPUMask(GPUMask & Rhs.GPUMask);
	}

	FORCEINLINE FRHIGPUMask operator |(const FRHIGPUMask& Rhs) const
	{
		return FRHIGPUMask(GPUMask | Rhs.GPUMask);
	}

	FORCEINLINE static const FRHIGPUMask GPU0() { return FRHIGPUMask(1); }
	FORCEINLINE static const FRHIGPUMask All() { return FRHIGPUMask((1 << GNumExplicitGPUsForRendering) - 1); }
	FORCEINLINE static const FRHIGPUMask FilterGPUsBefore(uint32 GPUIndex) { return FRHIGPUMask(~((1u << GPUIndex) - 1)) & All(); }

	struct FIterator
	{
		FORCEINLINE explicit FIterator(const uint32 InGPUMask) : GPUMask(InGPUMask), FirstGPUIndexInMask(0)
		{
#if WITH_MGPU
			FirstGPUIndexInMask = FPlatformMath::CountTrailingZeros(InGPUMask);
#endif
		}

		FORCEINLINE explicit FIterator(const FRHIGPUMask& InGPUMask) : FIterator(InGPUMask.GPUMask)
		{
		}

		FORCEINLINE FIterator& operator++()
		{
#if WITH_MGPU
			GPUMask &= ~(1 << FirstGPUIndexInMask);
			FirstGPUIndexInMask = FPlatformMath::CountTrailingZeros(GPUMask);
#else
			GPUMask = 0;
#endif
			return *this;
		}

		FORCEINLINE FIterator operator++(int)
		{
			FIterator Copy(*this);
			++*this;
			return Copy;
		}

		FORCEINLINE uint32 operator*() const { return FirstGPUIndexInMask; }
		FORCEINLINE bool operator !=(const FIterator& Rhs) const { return GPUMask != Rhs.GPUMask; }
		FORCEINLINE explicit operator bool() const { return GPUMask != 0; }
		FORCEINLINE bool operator !() const { return !(bool)*this; }

	private:
		uint32 GPUMask;
		unsigned long FirstGPUIndexInMask;
	};

	FORCEINLINE friend FRHIGPUMask::FIterator begin(const FRHIGPUMask& NodeMask) { return FRHIGPUMask::FIterator(NodeMask.GPUMask); }
	FORCEINLINE friend FRHIGPUMask::FIterator end(const FRHIGPUMask& NodeMask) { return FRHIGPUMask::FIterator(0); }
};

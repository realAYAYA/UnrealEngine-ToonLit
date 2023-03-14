// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MultiGPU.h: Multi-GPU support
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

/** When greater than one, indicates that SLI rendering is enabled */
#if PLATFORM_DESKTOP
#define WITH_SLI 1	// Implicit SLI
#define WITH_MGPU 1	// Explicit MGPU
#define MAX_NUM_GPUS 8
extern RHI_API uint32 GNumExplicitGPUsForRendering;
extern RHI_API uint32 GNumAlternateFrameRenderingGroups;
extern RHI_API uint32 GVirtualMGPU;
#else
#define WITH_SLI 0
#define WITH_MGPU 0
#define MAX_NUM_GPUS 1
#define GNumExplicitGPUsForRendering 1
#define GNumAlternateFrameRenderingGroups 1
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

/**
 * GPU mask utilities to get information about AFR groups and siblings.
 * - An AFR group is the set of GPUs that are working on the same frame together.
 * - AFR siblings are the GPUs in other groups do the same kind of work on subsequent
 *   frames. For example, two GPUs that render the same view on different frames are
 *   AFR siblings.
 *
 * For an 4 GPU setup with 2 AFR groups:
 * - There are 2 GPUs per AFR group. 0b1010 and 0b0101 are the two groups.
 * - Each GPU has 1 sibling. 0b1100 and 0b0011 are siblings.
 */
struct AFRUtils
{
	/**
	 * Gets the number of GPUs per AFR group.
	 */
	static inline uint32 GetNumGPUsPerGroup()
	{
		checkSlow(GNumExplicitGPUsForRendering % GNumAlternateFrameRenderingGroups == 0);
		return GNumExplicitGPUsForRendering / GNumAlternateFrameRenderingGroups;
	}

	/**
	 * Gets the AFR group index for a GPU index.
	 */
	static inline uint32 GetGroupIndex(uint32 GPUIndex)
	{
		return GPUIndex % GNumAlternateFrameRenderingGroups;
	}

	/**
	 * Gets the index of a GPU relative its AFR group.
	 */
	static inline uint32 GetIndexWithinGroup(uint32 GPUIndex)
	{
		return GPUIndex / GNumAlternateFrameRenderingGroups;
	}

	/**
	 * Gets the next AFR sibling for a GPU.
	 */
	static inline uint32 GetNextSiblingGPUIndex(uint32 GPUIndex)
	{
#if WITH_MGPU
		return GetIndexWithinGroup(GPUIndex) * GNumAlternateFrameRenderingGroups + GetGroupIndex(GPUIndex + 1);
#else
		return GPUIndex;
#endif
	}

	/**
	 * Gets a mask containing the next AFR siblings for a GPU mask.
	 */
	static inline FRHIGPUMask GetNextSiblingGPUMask(FRHIGPUMask InGPUMask)
	{
		FRHIGPUMask::FIterator It(InGPUMask);
		FRHIGPUMask GPUMask = FRHIGPUMask::FromIndex(GetNextSiblingGPUIndex(*It));
		while (++It)
		{
			GPUMask |= FRHIGPUMask::FromIndex(GetNextSiblingGPUIndex(*It));
		}
		return GPUMask;
	}

	/**
	 * Gets the previous AFR sibling for a GPU.
	 */
	static inline uint32 GetPrevSiblingGPUIndex(uint32 GPUIndex)
	{
#if WITH_MGPU
		return GetIndexWithinGroup(GPUIndex) * GNumAlternateFrameRenderingGroups + GetGroupIndex(GPUIndex + GNumAlternateFrameRenderingGroups - 1);
#else
		return GPUIndex;
#endif
	}

	/**
	 * Gets a mask containing the previous AFR siblings for a GPU mask.
	 */
	static inline FRHIGPUMask GetPrevSiblingGPUMask(FRHIGPUMask InGPUMask)
	{
		FRHIGPUMask::FIterator It(InGPUMask);
		FRHIGPUMask GPUMask = FRHIGPUMask::FromIndex(GetPrevSiblingGPUIndex(*It));
		while (++It)
		{
			GPUMask |= FRHIGPUMask::FromIndex(GetPrevSiblingGPUIndex(*It));
		}
		return GPUMask;
	}

	/**
	 * Gets the GPU mask including all GPUs within the same AFR group.
	 */
	static inline FRHIGPUMask GetGPUMaskForGroup(uint32 GPUIndex)
	{
#if WITH_MGPU
		return GroupMasks[GetGroupIndex(GPUIndex)];
#else
		return FRHIGPUMask::FromIndex(GPUIndex);
#endif
	}

	/**
	 * Gets the GPU mask including all GPUs within the same AFR group(s).
	 */
	static inline FRHIGPUMask GetGPUMaskForGroup(FRHIGPUMask InGPUMask)
	{
		FRHIGPUMask::FIterator It(InGPUMask);
		FRHIGPUMask GPUMask = GetGPUMaskForGroup(*It);
		while (++It)
		{
			GPUMask |= GetGPUMaskForGroup(*It);
		}
		return GPUMask;
	}

	/**
	 * Gets the GPU mask including all siblings across all AFR groups.
	 */
	static inline FRHIGPUMask GetGPUMaskWithSiblings(uint32 GPUIndex)
	{
#if WITH_MGPU
		return SiblingMasks[GetIndexWithinGroup(GPUIndex)];
#else
		return FRHIGPUMask::FromIndex(GPUIndex);
#endif
	}

	/**
	 * Gets the GPU mask including all siblings across all AFR groups.
	 */
	static inline FRHIGPUMask GetGPUMaskWithSiblings(FRHIGPUMask InGPUMask)
	{
		FRHIGPUMask::FIterator It(InGPUMask);
		FRHIGPUMask GPUMask = GetGPUMaskWithSiblings(*It);
		while (++It)
		{
			GPUMask |= GetGPUMaskWithSiblings(*It);
		}
		return GPUMask;
	}

#if WITH_MGPU
	static void StaticInitialize();

	static inline const TArray<FRHIGPUMask, TFixedAllocator<MAX_NUM_GPUS>>& GetGroupMasks() { return GroupMasks; }
	static inline const TArray<FRHIGPUMask, TFixedAllocator<MAX_NUM_GPUS>>& GetSiblingMasks() { return SiblingMasks; }

private:
	static RHI_API TArray<FRHIGPUMask, TFixedAllocator<MAX_NUM_GPUS>> GroupMasks;
	static RHI_API TArray<FRHIGPUMask, TFixedAllocator<MAX_NUM_GPUS>> SiblingMasks;
#endif // WITH_MGPU
};

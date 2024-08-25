// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/Allocators/CachedOSPageAllocator.h"
#include "HAL/Allocators/CachedOSVeryLargePageAllocator.h"
#include "HAL/Allocators/PooledVirtualMemoryAllocator.h"
#include "HAL/CriticalSection.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/MallocBinnedCommon.h"
#include "HAL/UnrealMemory.h"
#include "Math/NumericLimits.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Fork.h"
#include "Templates/Atomic.h"

struct FGenericMemoryStats;

#define BINNED2_MAX_CACHED_OS_FREES (64)
#if PLATFORM_64BITS
	#define BINNED2_MAX_CACHED_OS_FREES_BYTE_LIMIT (64*1024*1024)
#else
	#define BINNED2_MAX_CACHED_OS_FREES_BYTE_LIMIT (16*1024*1024)
#endif

#define BINNED2_LARGE_ALLOC					65536		// Alignment of OS-allocated pointer - pool-allocated pointers will have a non-aligned pointer
#define BINNED2_MINIMUM_ALIGNMENT_SHIFT		4			// Alignment of blocks, expressed as a shift
#define BINNED2_MINIMUM_ALIGNMENT			16			// Alignment of blocks
#define BINNED2_MAXIMUM_ALIGNMENT			128
#define BINNED2_MAX_SMALL_POOL_SIZE			(32768-16)	// Maximum block size in GMallocBinned2SmallBlockSizes
#define BINNED2_SMALL_POOL_COUNT			45


#define DEFAULT_GMallocBinned2PerThreadCaches 1
#define DEFAULT_GMallocBinned2LockFreeCaches 0
#define DEFAULT_GMallocBinned2AllocExtra 32
#define BINNED2_MAX_GMallocBinned2MaxBundlesBeforeRecycle 8
#define DEFAULT_GMallocBinned2MoveOSFreesOffTimeCriticalThreads 1

// When book keeping is at the end of FFreeBlock, MallocBinned2 cannot tell if the allocation comes from a large allocation (higher than 64KB, also named as "OSAllocation") 
// or from VeryLargePageAllocator that fell back to FCachedOSPageAllocator. In both cases the allocation (large or small) might be aligned to 64KB.
// bookKeeping at the end needs to be disabled if we want VeryLargePageAllocator to properly fallback to regular TCachedOSPageAllocator if needed
#ifndef BINNED2_BOOKKEEPING_AT_THE_END_OF_LARGEBLOCK
#define BINNED2_BOOKKEEPING_AT_THE_END_OF_LARGEBLOCK 0
#endif

// If we are emulating forking on a windows server or are a linux server, enable support for avoiding dirtying pages owned by the parent. 
#ifndef BINNED2_FORK_SUPPORT
	#define BINNED2_FORK_SUPPORT (UE_SERVER && (PLATFORM_UNIX || DEFAULT_SERVER_FAKE_FORKS))
#endif


#define BINNED2_ALLOW_RUNTIME_TWEAKING UE_BINNEDCOMMON_ALLOW_RUNTIME_TWEAKING
#if BINNED2_ALLOW_RUNTIME_TWEAKING
	extern CORE_API int32 GMallocBinned2PerThreadCaches;
	extern CORE_API int32 GMallocBinned2MaxBundlesBeforeRecycle;
	extern CORE_API int32 GMallocBinned2AllocExtra;
	extern CORE_API int32 GMallocBinned2MoveOSFreesOffTimeCriticalThreads;

#else
	#define GMallocBinned2PerThreadCaches DEFAULT_GMallocBinned2PerThreadCaches
	#define GMallocBinned2MaxBundlesBeforeRecycle BINNED2_MAX_GMallocBinned2MaxBundlesBeforeRecycle
	#define GMallocBinned2AllocExtra DEFAULT_GMallocBinned2AllocExtra
	#define GMallocBinned2MoveOSFreesOffTimeCriticalThreads DEFAULT_GMallocBinned2MoveOSFreesOffTimeCriticalThreads
#endif

#define BINNED2_ALLOCATOR_STATS UE_BINNEDCOMMON_ALLOCATOR_STATS
#define BINNED2_ALLOCATOR_STATS_VALIDATION (BINNED2_ALLOCATOR_STATS && 0)


#if BINNED2_ALLOCATOR_STATS
//////////////////////////////////////////////////////////////////////////
// the following don't need a critical section because they are covered by the critical section called Mutex
extern TAtomic<int64> AllocatedSmallPoolMemory; // memory that's requested to be allocated by the game
extern TAtomic<int64> AllocatedOSSmallPoolMemory;
extern TAtomic<int64> AllocatedLargePoolMemory; // memory requests to the OS which don't fit in the small pool
extern TAtomic<int64> AllocatedLargePoolMemoryWAlignment; // when we allocate at OS level we need to align to a size
#endif
#if BINNED2_ALLOCATOR_STATS_VALIDATION
#include "Misc/ScopeLock.h"

extern int64 AllocatedSmallPoolMemoryValidation;
extern FCriticalSection ValidationCriticalSection;
extern int32 RecursionCounter;
#endif

// Canary value used in FFreeBlock
// A constant value unless we're compiled with fork support in which case there are two values identifying whether the page
// was allocated pre- or post-fork
enum class EBlockCanary : uint8
{
	Zero = 0x0, // Not clear why this is needed by FreeBundles
#if BINNED2_FORK_SUPPORT
	PreFork = 0xb7,
	PostFork = 0xca,
#else
	Value = 0xe3 
#endif
};


//
// Optimized virtual memory allocator.
//
class FMallocBinned2 : public TMallocBinnedCommon<FMallocBinned2, BINNED2_MINIMUM_ALIGNMENT, BINNED2_MAXIMUM_ALIGNMENT, BINNED2_MINIMUM_ALIGNMENT_SHIFT, BINNED2_SMALL_POOL_COUNT, BINNED2_MAX_SMALL_POOL_SIZE>
{
	// Forward declares.
	struct FPoolInfo;
	using PoolHashBucket = TPoolHashBucket<FPoolInfo>;
	struct Private;

	/** Information about a piece of free memory. */
	struct FFreeBlock
	{
		FORCEINLINE FFreeBlock(uint32 InPageSize, uint16 InBlockSize, uint8 InPoolIndex, EBlockCanary InCanary)
			: BlockSize(InBlockSize)
			, PoolIndex(InPoolIndex)
			, CanaryAndForkState(InCanary)
			, NextFreeBlock(nullptr)
		{
			check(InPoolIndex < MAX_uint8 && InBlockSize <= MAX_uint16);
			NumFreeBlocks = InPageSize / InBlockSize;
			if (NumFreeBlocks * InBlockSize + sizeof(FFreeBlock) > InPageSize)
			{
				NumFreeBlocks--;
			}
			check(NumFreeBlocks * InBlockSize + sizeof(FFreeBlock) <= InPageSize);
		}

		FORCEINLINE uint32 GetNumFreeRegularBlocks() const
		{
			return NumFreeBlocks;
		}


		FORCEINLINE void* AllocateRegularBlock()
		{
			--NumFreeBlocks;
#if !UE_USE_VERYLARGEPAGEALLOCATOR || !BINNED2_BOOKKEEPING_AT_THE_END_OF_LARGEBLOCK
			if (IsAligned(this, BINNED2_LARGE_ALLOC))
			{
				return (uint8*)this + BINNED2_LARGE_ALLOC - (NumFreeBlocks + 1) * BlockSize;
			}
#else
			if (IsAligned(((uintptr_t)this)+sizeof(FFreeBlock), BINNED2_LARGE_ALLOC))
			{
				// The book keeping FreeBlock is at the end of the "page" so we align down to get to the beginning of the page
				uintptr_t ptr = AlignDown((uintptr_t)this, BINNED2_LARGE_ALLOC);
				// And we offset the returned pointer based on how many free blocks are left.
				return (uint8*) ptr + (NumFreeBlocks * BlockSize);
			}
#endif
			return (uint8*)this + (NumFreeBlocks)* BlockSize;
		}

		uint16 BlockSize;				// Size of the blocks that this list points to
		uint8 PoolIndex;				// Index of this pool

		// Normally this value just functions as a canary to detect invalid memory state.
		// When process forking is supported, it's still a canary but it has two valid values.
		// One value is used pre-fork and one post-fork and the value is used to avoid freeing memory in pages shared with the parent process.
		EBlockCanary CanaryAndForkState; 

		uint32 NumFreeBlocks;          // Number of consecutive free blocks here, at least 1.
		void*  NextFreeBlock;          // Next free block in another pool
	};

	struct FPoolList
	{
		FPoolList();

		void Clear();
		bool IsEmpty() const;

		      FPoolInfo& GetFrontPool();
		const FPoolInfo& GetFrontPool() const;

		void LinkToFront(FPoolInfo* Pool);

		FPoolInfo& PushNewPoolToFront(FMallocBinned2& Allocator, uint32 InBytes, uint32 InPoolIndex);

		void ValidateActivePools();
		void ValidateExhaustedPools();

	private:
		FPoolInfo* Front;
	};

	/** Pool table. */
	struct FPoolTable
	{
		FPoolList ActivePools;
		FPoolList ExhaustedPools;
		uint32    BlockSize;

		FPoolTable();
	};

	// Pool tables for different pool sizes
	FPoolTable SmallPoolTables[BINNED2_SMALL_POOL_COUNT];

	PoolHashBucket* HashBuckets;
	PoolHashBucket* HashBucketFreeList;
	uint64 NumPoolsPerPage;
#if BINNED2_FORK_SUPPORT
	EBlockCanary CurrentCanary = EBlockCanary::PreFork; // The value of the canary for pages we have allocated this side of the fork 
	EBlockCanary OldCanary = EBlockCanary::PreFork;		// If we have forked, the value canary of old pages we should avoid touching 
#else 
	static constexpr EBlockCanary CurrentCanary = EBlockCanary::Value;
#endif

#if !PLATFORM_UNIX && !PLATFORM_ANDROID
#if UE_USE_VERYLARGEPAGEALLOCATOR
	FCachedOSVeryLargePageAllocator CachedOSPageAllocator;
#else
	TCachedOSPageAllocator<BINNED2_MAX_CACHED_OS_FREES, BINNED2_MAX_CACHED_OS_FREES_BYTE_LIMIT> CachedOSPageAllocator;
#endif
#else
	FPooledVirtualMemoryAllocator CachedOSPageAllocator;
#endif

	FCriticalSection Mutex;

	FORCEINLINE bool IsOSAllocation(const void* Ptr)
	{
#if UE_USE_VERYLARGEPAGEALLOCATOR && !PLATFORM_UNIX && !PLATFORM_ANDROID
		return !CachedOSPageAllocator.IsPartOf(Ptr) && IsAligned(Ptr, BINNED2_LARGE_ALLOC);
#else
		return IsAligned(Ptr, BINNED2_LARGE_ALLOC);
#endif
	}

	static FORCEINLINE FFreeBlock* GetPoolHeaderFromPointer(void* Ptr)
	{
#if !UE_USE_VERYLARGEPAGEALLOCATOR || !BINNED2_BOOKKEEPING_AT_THE_END_OF_LARGEBLOCK
		return (FFreeBlock*)AlignDown(Ptr, BINNED2_LARGE_ALLOC);
#else
		return (FFreeBlock*) (AlignDown((uintptr_t) Ptr, BINNED2_LARGE_ALLOC) + BINNED2_LARGE_ALLOC - sizeof(FFreeBlock));
#endif
	}

public:


	CORE_API FMallocBinned2();

	CORE_API virtual ~FMallocBinned2();

	// FMalloc interface.
	CORE_API virtual bool IsInternallyThreadSafe() const override;
	FORCEINLINE virtual void* Malloc(SIZE_T Size, uint32 Alignment) override
	{
#if BINNED2_ALLOCATOR_STATS_VALIDATION
		FScopeLock Lock(&ValidationCriticalSection);
		++RecursionCounter;
		void *Result = MallocInline(Size, Alignment);
		if ( !IsOSAllocation(Result))
		{
			SIZE_T OutSize;
			ensure( GetAllocationSize(Result, OutSize) );
			AllocatedSmallPoolMemoryValidation += OutSize;
			if (RecursionCounter==1)
			{
				check(GetTotalAllocatedSmallPoolMemory() == AllocatedSmallPoolMemoryValidation);
				if (GetTotalAllocatedSmallPoolMemory() != AllocatedSmallPoolMemoryValidation)
				{
					UE_DEBUG_BREAK();
				}
			}
		}
		--RecursionCounter;
		return Result;
#else
		return MallocInline(Size, Alignment);
#endif
	}
	FORCEINLINE void* MallocInline(SIZE_T Size, uint32 Alignment)
	{
		// Only allocate from the small pools if the size is small enough and the alignment isn't crazy large.
		// With large alignments, we'll waste a lot of memory allocating an entire page, but such alignments are highly unlikely in practice.
		const bool bUseSmallPool = UseSmallAlloc(Size, Alignment);
		if (bUseSmallPool)
		{
			FPerThreadFreeBlockLists* Lists = GMallocBinned2PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
			if (Lists)
			{
				const uint32 PoolIndex = BoundSizeToPoolIndex(Size, MemSizeToIndex);
				if (void* Result = Lists->Malloc(PoolIndex))
				{
#if BINNED2_ALLOCATOR_STATS
					const uint32 BlockSize = PoolIndexToBlockSize(PoolIndex);
					Lists->AllocatedMemory += BlockSize;
#endif
					return Result;
				}
			}
		}

		return MallocSelect(Size, Alignment, bUseSmallPool);
	}
	FORCEINLINE static bool UseSmallAlloc(SIZE_T Size, uint32 Alignment)
	{
#if UE_USE_VERYLARGEPAGEALLOCATOR && BINNED2_BOOKKEEPING_AT_THE_END_OF_LARGEBLOCK
		if (Alignment > BINNED2_MINIMUM_ALIGNMENT)
		{
			Size = Align(Size, Alignment);
		}
		bool bResult = (Size <= BINNED2_MAX_SMALL_POOL_SIZE);
#else
		bool bResult = ((Size <= BINNED2_MAX_SMALL_POOL_SIZE) & (Alignment <= BINNED2_MINIMUM_ALIGNMENT)); // one branch, not two
#endif
		return bResult;
	}
	CORE_API void* MallocSelect(SIZE_T Size, uint32 Alignment, bool bUseSmallPool);
	FORCEINLINE void* MallocSelect(SIZE_T Size, uint32 Alignment)
	{
		return MallocSelect(Size, Alignment, UseSmallAlloc(Size, Alignment));
	}
	FORCEINLINE virtual void* Realloc(void* Ptr, SIZE_T NewSize, uint32 Alignment) override
	{
#if BINNED2_ALLOCATOR_STATS_VALIDATION
		bool bOldIsOsAllocation = IsOSAllocation(Ptr);
		SIZE_T OldSize;
		if (!bOldIsOsAllocation)
		{
			ensure(GetAllocationSize(Ptr, OldSize));
		}
		FScopeLock Lock(&ValidationCriticalSection);
		++RecursionCounter;
		void *Result = ReallocInline(Ptr, NewSize, Alignment);
		if ( !bOldIsOsAllocation )
		{
			AllocatedSmallPoolMemoryValidation -= OldSize;
		}
		if (!IsOSAllocation(Result))
		{
			SIZE_T OutSize;
			ensure(GetAllocationSize(Result, OutSize));
			AllocatedSmallPoolMemoryValidation += OutSize;
		}
		if (RecursionCounter == 1)
		{
			check(GetTotalAllocatedSmallPoolMemory() == AllocatedSmallPoolMemoryValidation);
			if (GetTotalAllocatedSmallPoolMemory() != AllocatedSmallPoolMemoryValidation)
			{
				UE_DEBUG_BREAK();
			}
		}
		--RecursionCounter;
		return Result;
#else
		return ReallocInline(Ptr, NewSize, Alignment);
#endif
	}
	FORCEINLINE void* ReallocInline(void* Ptr, SIZE_T NewSize, uint32 Alignment) 
	{
#if UE_USE_VERYLARGEPAGEALLOCATOR && BINNED2_BOOKKEEPING_AT_THE_END_OF_LARGEBLOCK
		if (Alignment > BINNED2_MINIMUM_ALIGNMENT && (NewSize <= BINNED2_MAX_SMALL_POOL_SIZE))
		{
			NewSize = Align(NewSize, Alignment);
		}
		if (NewSize <= BINNED2_MAX_SMALL_POOL_SIZE)
#else
		if (NewSize <= BINNED2_MAX_SMALL_POOL_SIZE && Alignment <= BINNED2_MINIMUM_ALIGNMENT) // one branch, not two
#endif
		{
			FPerThreadFreeBlockLists* Lists = GMallocBinned2PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
			if (Lists && (!Ptr || !IsOSAllocation(Ptr)))
			{
				uint32 BlockSize = 0;
				uint32 PoolIndex = 0;

				bool bCanFree = true; // the nullptr is always "freeable"
				if (Ptr)
				{
					// Reallocate to a smaller/bigger pool if necessary
					FFreeBlock* Free = GetPoolHeaderFromPointer(Ptr);
					BlockSize = Free->BlockSize;
					PoolIndex = Free->PoolIndex;
					// If canary is invalid we will assert in ReallocExternal. Otherwise it's the pre-fork canary and we will allocate new memory without touching this allocation.
					bCanFree = Free->CanaryAndForkState == CurrentCanary;
					if (NewSize && bCanFree && NewSize <= BlockSize && (PoolIndex == 0 || NewSize > PoolIndexToBlockSize(PoolIndex - 1)))
					{
						return Ptr;
					}
					bCanFree = bCanFree && Lists->CanFree(PoolIndex, BlockSize);
				}
				if (bCanFree)
				{
					uint32 NewPoolIndex = BoundSizeToPoolIndex(NewSize, MemSizeToIndex);
					uint32 NewBlockSize = PoolIndexToBlockSize(NewPoolIndex);
					void* Result = NewSize ? Lists->Malloc(NewPoolIndex) : nullptr;
#if BINNED2_ALLOCATOR_STATS
					if (Result)
					{
						Lists->AllocatedMemory += NewBlockSize;
					}
#endif
					if (Result || !NewSize)
					{
						if (Result && Ptr)
						{
							FMemory::Memcpy(Result, Ptr, FPlatformMath::Min<SIZE_T>(NewSize, BlockSize));
						}
						if (Ptr)
						{
							bool bDidPush = Lists->Free(Ptr, PoolIndex, BlockSize);
							checkSlow(bDidPush);
#if BINNED2_ALLOCATOR_STATS
							Lists->AllocatedMemory -= BlockSize;
#endif
						}

						return Result;
					}
				}
			}
		}
		void* Result = ReallocExternal(Ptr, NewSize, Alignment);
		return Result;
	}

	FORCEINLINE virtual void Free(void* Ptr) override
	{
#if BINNED2_ALLOCATOR_STATS_VALIDATION
		FScopeLock Lock(&ValidationCriticalSection);
		++RecursionCounter;
		if (!IsOSAllocation(Ptr))
		{
			SIZE_T OutSize;
			ensure(GetAllocationSize(Ptr, OutSize));
			AllocatedSmallPoolMemoryValidation -= OutSize;
		}
		FreeInline(Ptr);
		if (RecursionCounter == 1)
		{
			check(GetTotalAllocatedSmallPoolMemory() == AllocatedSmallPoolMemoryValidation);
			if (GetTotalAllocatedSmallPoolMemory() != AllocatedSmallPoolMemoryValidation)
			{
				UE_DEBUG_BREAK();
			}
		}
		--RecursionCounter;
#else
		FreeInline(Ptr);
#endif
	}

	FORCEINLINE void FreeInline(void* Ptr)
	{
		if (!IsOSAllocation(Ptr))
		{
			FPerThreadFreeBlockLists* Lists = GMallocBinned2PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
			if (Lists)
			{
				FFreeBlock* BasePtr = GetPoolHeaderFromPointer(Ptr);
				int32 BlockSize = BasePtr->BlockSize;
				// If canary is invalid we will assert in FreeExternal. Otherwise it's the pre-fork canary and we will turn this free into a no-op.
				if (BasePtr->CanaryAndForkState == CurrentCanary && Lists->Free(Ptr, BasePtr->PoolIndex, BasePtr->BlockSize))
				{
#if BINNED2_ALLOCATOR_STATS
					Lists->AllocatedMemory -= BasePtr->BlockSize;
#endif
					return;
				}
			}
		}
		FreeExternal(Ptr);
	}
	FORCEINLINE virtual bool GetAllocationSize(void *Ptr, SIZE_T &SizeOut) override
	{
		if (!IsOSAllocation(Ptr))
		{
			const FFreeBlock* Free = GetPoolHeaderFromPointer(Ptr);
#if BINNED2_FORK_SUPPORT
			if (Free->CanaryAndForkState == CurrentCanary || Free->CanaryAndForkState == OldCanary)
#else
			if (Free->CanaryAndForkState == CurrentCanary)
#endif
			{
				SizeOut = Free->BlockSize;
				return true;
			}
		}
		return GetAllocationSizeExternal(Ptr, SizeOut);
	}

	FORCEINLINE virtual SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment) override
	{
		return QuantizeSizeCommon(Count, Alignment, *this);
	}

	CORE_API virtual bool ValidateHeap() override;
	CORE_API virtual void Trim(bool bTrimThreadCaches) override;
	CORE_API virtual void SetupTLSCachesOnCurrentThread() override;
	CORE_API virtual void ClearAndDisableTLSCachesOnCurrentThread() override;
	CORE_API virtual const TCHAR* GetDescriptiveName() override;
	CORE_API virtual void UpdateStats() override;
	CORE_API virtual void OnMallocInitialized() override;
	CORE_API virtual void OnPreFork() override;
	CORE_API virtual void OnPostFork() override;
	// End FMalloc interface.

	CORE_API void FlushCurrentThreadCache();
	CORE_API void* MallocExternalSmall(SIZE_T Size, uint32 Alignment);
	CORE_API void* MallocExternalLarge(SIZE_T Size, uint32 Alignment);
	CORE_API void* ReallocExternal(void* Ptr, SIZE_T NewSize, uint32 Alignment);
	CORE_API void FreeExternal(void *Ptr);
	CORE_API bool GetAllocationSizeExternal(void* Ptr, SIZE_T& SizeOut);

	CORE_API void CanaryTest(const FFreeBlock* Block) const;
	CORE_API void CanaryFail(const FFreeBlock* Block) const;

#if BINNED2_ALLOCATOR_STATS
	CORE_API int64 GetTotalAllocatedSmallPoolMemory() const;
#endif
	CORE_API virtual void GetAllocatorStats( FGenericMemoryStats& out_Stats ) override;
	/** Dumps current allocator stats to the log. */
	CORE_API virtual void DumpAllocatorStats(class FOutputDevice& Ar) override;
	
	static CORE_API uint16 SmallBlockSizesReversed[BINNED2_SMALL_POOL_COUNT]; // this is reversed to get the smallest elements on our main cache line
	static CORE_API FMallocBinned2* MallocBinned2;
	static CORE_API uint32 PageSize;
	static CORE_API uint32 OsAllocationGranularity;
	// Mapping of sizes to small table indices
	static CORE_API uint8 MemSizeToIndex[1 + (BINNED2_MAX_SMALL_POOL_SIZE >> BINNED2_MINIMUM_ALIGNMENT_SHIFT)];

	static void RegisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists);
	static void UnregisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists);

	static void* AllocateMetaDataMemory(SIZE_T Size);
	static void FreeMetaDataMemory(void* Ptr, SIZE_T Size);

	FORCEINLINE uint32 PoolIndexToBlockSize(uint32 PoolIndex) const
	{
		return SmallBlockSizesReversed[BINNED2_SMALL_POOL_COUNT - PoolIndex - 1];
	}
};

#define BINNED2_INLINE (1)
#if BINNED2_INLINE // during development, it helps with iteration time to not include these here, but rather in the .cpp
	#if PLATFORM_USES_FIXED_GMalloc_CLASS && !FORCE_ANSI_ALLOCATOR && USE_MALLOC_BINNED2
		#define FMEMORY_INLINE_FUNCTION_DECORATOR  FORCEINLINE
		#define FMEMORY_INLINE_GMalloc (FMallocBinned2::MallocBinned2)
		#include "FMemory.inl" // IWYU pragma: export
	#endif
#endif


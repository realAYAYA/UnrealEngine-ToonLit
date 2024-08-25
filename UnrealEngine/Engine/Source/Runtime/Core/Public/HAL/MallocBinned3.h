// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if PLATFORM_64BITS && PLATFORM_HAS_FPlatformVirtualMemoryBlock
#include "HAL/Allocators/CachedOSPageAllocator.h"
#include "HAL/Allocators/PooledVirtualMemoryAllocator.h"
#include "HAL/CriticalSection.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/MallocBinnedCommon.h"
#include "HAL/PlatformMemory.h"
#include "HAL/UnrealMemory.h"
#include "Math/NumericLimits.h"
#include "Misc/AssertionMacros.h"
#include "Templates/AlignmentTemplates.h"
#include "Templates/Atomic.h"

struct FGenericMemoryStats;

#define USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS (0)

#ifndef USE_512MB_MAX_MEMORY_PER_BLOCK_SIZE 
#define USE_512MB_MAX_MEMORY_PER_BLOCK_SIZE 0
#endif

#define BINNED3_BASE_PAGE_SIZE				4096			// Minimum "page size" for binned3
#define BINNED3_MINIMUM_ALIGNMENT_SHIFT		4				// Alignment of blocks, expressed as a shift
#define BINNED3_MINIMUM_ALIGNMENT			16				// Alignment of blocks

#ifndef BINNED3_MAX_SMALL_POOL_ALIGNMENT
#define BINNED3_MAX_SMALL_POOL_ALIGNMENT	128
#endif


#ifndef BINNED3_MAX_SMALL_POOL_SIZE
#if USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
#define BINNED3_MAX_SMALL_POOL_SIZE			(BINNEDCOMMON_MAX_LISTED_SMALL_POOL_SIZE)	// Maximum medium block size
#else
#define BINNED3_MAX_SMALL_POOL_SIZE			(128 * 1024)	// Maximum medium block size
#endif
#endif
#define BINNED3_SMALL_POOL_COUNT			(BINNEDCOMMON_NUM_LISTED_SMALL_POOLS + (BINNED3_MAX_SMALL_POOL_SIZE - BINNEDCOMMON_MAX_LISTED_SMALL_POOL_SIZE) / BINNED3_BASE_PAGE_SIZE)

#if USE_512MB_MAX_MEMORY_PER_BLOCK_SIZE
#define MAX_MEMORY_PER_BLOCK_SIZE_SHIFT (29) // maximum of 512MB per block size
#else
#define MAX_MEMORY_PER_BLOCK_SIZE_SHIFT (30) // maximum of 1GB per block size
#endif

#define MAX_MEMORY_PER_BLOCK_SIZE (1ull << MAX_MEMORY_PER_BLOCK_SIZE_SHIFT) 

// This choice depends on how efficient the OS is with sparse commits in large VM blocks
#if !defined(BINNED3_USE_SEPARATE_VM_PER_POOL)
	#if PLATFORM_WINDOWS
		#define BINNED3_USE_SEPARATE_VM_PER_POOL (1)
	#else
		#define BINNED3_USE_SEPARATE_VM_PER_POOL (0)
	#endif
#endif

#define DEFAULT_GMallocBinned3PerThreadCaches 1
#define DEFAULT_GMallocBinned3AllocExtra 32
#define BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle 8

#define BINNED3_ALLOW_RUNTIME_TWEAKING UE_BINNEDCOMMON_ALLOW_RUNTIME_TWEAKING
#if BINNED3_ALLOW_RUNTIME_TWEAKING
	extern CORE_API int32 GMallocBinned3PerThreadCaches;
	extern CORE_API int32 GMallocBinned3MaxBundlesBeforeRecycle;
	extern CORE_API int32 GMallocBinned3AllocExtra;
#else
	#define GMallocBinned3PerThreadCaches DEFAULT_GMallocBinned3PerThreadCaches
	#define GMallocBinned3MaxBundlesBeforeRecycle BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle
	#define GMallocBinned3AllocExtra DEFAULT_GMallocBinned3AllocExtra
#endif

#define BINNED3_ALLOCATOR_STATS UE_BINNEDCOMMON_ALLOCATOR_STATS

#if BINNED3_ALLOCATOR_STATS
	#define BINNED3_ALLOCATOR_PER_BIN_STATS !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#else
	#define BINNED3_ALLOCATOR_PER_BIN_STATS 0
#endif


PRAGMA_DISABLE_UNSAFE_TYPECAST_WARNINGS

//
// Optimized virtual memory allocator.
//
class CORE_API FMallocBinned3 : public TMallocBinnedCommon<FMallocBinned3, BINNED3_MINIMUM_ALIGNMENT, BINNED3_MAX_SMALL_POOL_ALIGNMENT, BINNED3_MINIMUM_ALIGNMENT_SHIFT, BINNED3_SMALL_POOL_COUNT, BINNED3_MAX_SMALL_POOL_SIZE>
{	
	// Forward declares.
	struct FPoolInfoLarge;
	struct FPoolInfoSmall;
	struct FPoolTable;
	using PoolHashBucket = TPoolHashBucket<FPoolInfoLarge>;
	struct Private;


	/** Information about a piece of free memory. */
	struct FFreeBlock
	{
		enum
		{
			CANARY_VALUE = 0xe7
		};

		FORCEINLINE FFreeBlock(uint32 InPageSize, uint32 InBlockSize, uint8 InPoolIndex)
			: BlockSizeShifted(InBlockSize >> BINNED3_MINIMUM_ALIGNMENT_SHIFT)
			, PoolIndex(InPoolIndex)
			, Canary(CANARY_VALUE)
			, NextFreeIndex(MAX_uint32)
		{
			check(InPoolIndex < MAX_uint8 && (InBlockSize >> BINNED3_MINIMUM_ALIGNMENT_SHIFT) <= MAX_uint16);
			NumFreeBlocks = InPageSize / InBlockSize;
		}

		FORCEINLINE uint32 GetNumFreeRegularBlocks() const
		{
			return NumFreeBlocks;
		}
		FORCEINLINE bool IsCanaryOk() const
		{
			return Canary == FFreeBlock::CANARY_VALUE;
		}

		FORCEINLINE void CanaryTest() const
		{
			if (!IsCanaryOk())
			{
				CanaryFail();
			}
			//checkSlow(PoolIndex == BoundSizeToPoolIndex(BlockSize));
		}
		void CanaryFail() const;

		FORCEINLINE void* AllocateRegularBlock()
		{
			--NumFreeBlocks;
			return (uint8*)this + NumFreeBlocks* (uint32(BlockSizeShifted) << BINNED3_MINIMUM_ALIGNMENT_SHIFT);
		}

		uint16 BlockSizeShifted;		// Size of the blocks that this list points to >> BINNED3_MINIMUM_ALIGNMENT_SHIFT
		uint8 PoolIndex;				// Index of this pool
		uint8 Canary;					// Constant value of 0xe3
		uint32 NumFreeBlocks;          // Number of consecutive free blocks here, at least 1.
		uint32 NextFreeIndex;          // Next free block or MAX_uint32
	};

	/** Pool table. */
	struct FPoolTable
	{
		uint32 BlockSize;
		uint16 BlocksPerBlockOfBlocks;
		uint8 PagesPlatformForBlockOfBlocks;

		FBitTree BlockOfBlockAllocationBits; // one bits in here mean the virtual memory is committed
		FBitTree BlockOfBlockIsExhausted;    // one bit in here means the pool is completely full

		uint32 NumEverUsedBlockOfBlocks;
		FPoolInfoSmall** PoolInfos;

		uint64 UnusedAreaOffsetLow;

#if BINNED3_ALLOCATOR_PER_BIN_STATS
		// these are "head end" stats, above the TLS cache
		TAtomic<int64> TotalRequestedAllocSize;
		TAtomic<int64> TotalAllocCount;
		TAtomic<int64> TotalFreeCount;

		FORCEINLINE void HeadEndAlloc(SIZE_T Size)
		{
			check(Size >= 0 && Size <= BlockSize);
			TotalRequestedAllocSize += Size;
			TotalAllocCount++;
		}
		FORCEINLINE void HeadEndFree()
		{
			TotalFreeCount++;
		}
#else
		FORCEINLINE void HeadEndAlloc(SIZE_T Size)
		{
		}
		FORCEINLINE void HeadEndFree()
		{
		}
#endif
	};

	// Pool tables for different pool sizes
	FPoolTable SmallPoolTables[BINNED3_SMALL_POOL_COUNT];

	uint32 SmallPoolInfosPerPlatformPage;

	PoolHashBucket* HashBuckets;
	PoolHashBucket* HashBucketFreeList;
	uint64 NumLargePoolsPerPage;

	FCriticalSection Mutex;

#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	FORCEINLINE uint64 PoolIndexFromPtr(const void* Ptr) // returns a uint64 for it can also be used to check if it is an OS allocation
	{
		return (UPTRINT(Ptr) - UPTRINT(Binned3BaseVMPtr)) >> MAX_MEMORY_PER_BLOCK_SIZE_SHIFT;
	}
	FORCEINLINE uint8* PoolBasePtr(uint32 InPoolIndex)
	{
		return Binned3BaseVMPtr + InPoolIndex * MAX_MEMORY_PER_BLOCK_SIZE;
	}
#else
#if BINNED3_ALLOCATOR_STATS
	void RecordPoolSearch(uint32 Tests);
#else
	FORCEINLINE void RecordPoolSearch(uint32 Tests)
	{

	}
#endif
	FORCEINLINE uint64 PoolIndexFromPtr(const void* Ptr) // returns a uint64 for it can also be used to check if it is an OS allocation
	{
		if (PoolSearchDiv == 0)
		{
			return (UPTRINT(Ptr) - UPTRINT(PoolBaseVMPtr[0])) >> MAX_MEMORY_PER_BLOCK_SIZE_SHIFT;
		}
		uint64 PoolIndex = BINNED3_SMALL_POOL_COUNT;
		if (((uint8*)Ptr >= PoolBaseVMPtr[0]) & ((uint8*)Ptr < HighestPoolBaseVMPtr + MAX_MEMORY_PER_BLOCK_SIZE))
		{
			PoolIndex = uint64((uint8*)Ptr - PoolBaseVMPtr[0]) / PoolSearchDiv;
			if (PoolIndex >= BINNED3_SMALL_POOL_COUNT)
			{
				PoolIndex = BINNED3_SMALL_POOL_COUNT - 1;
			}
			uint32 Tests = 1; // we are counting potential cache misses here, not actual comparisons
			if ((uint8*)Ptr < PoolBaseVMPtr[PoolIndex])
			{
				do
				{
					Tests++;
					PoolIndex--;
					check(PoolIndex < BINNED3_SMALL_POOL_COUNT);
				} while ((uint8*)Ptr < PoolBaseVMPtr[PoolIndex]);
				if ((uint8*)Ptr >= PoolBaseVMPtr[PoolIndex] + MAX_MEMORY_PER_BLOCK_SIZE)
				{
					PoolIndex = BINNED3_SMALL_POOL_COUNT; // was in the gap
				}
			}
			else if ((uint8*)Ptr >= PoolBaseVMPtr[PoolIndex] + MAX_MEMORY_PER_BLOCK_SIZE)
			{
				do
				{
					Tests++;
					PoolIndex++;
					check(PoolIndex < BINNED3_SMALL_POOL_COUNT);
				} while ((uint8*)Ptr >= PoolBaseVMPtr[PoolIndex] + MAX_MEMORY_PER_BLOCK_SIZE);
				if ((uint8*)Ptr < PoolBaseVMPtr[PoolIndex])
				{
					PoolIndex = BINNED3_SMALL_POOL_COUNT; // was in the gap
				}
			}
			RecordPoolSearch(Tests);
		}
		return PoolIndex;
	}

	FORCEINLINE uint8* PoolBasePtr(uint32 InPoolIndex)
	{
		return PoolBaseVMPtr[InPoolIndex];
	}
#endif
	FORCEINLINE uint32 PoolIndexFromPtrChecked(const void* Ptr)
	{
		uint64 Result = PoolIndexFromPtr(Ptr);
		check(Result < BINNED3_SMALL_POOL_COUNT);
		return (uint32)Result;
	}

	FORCEINLINE bool IsOSAllocation(const void* Ptr)
	{
		return PoolIndexFromPtr(Ptr) >= BINNED3_SMALL_POOL_COUNT;
	}


	FORCEINLINE void* BlockOfBlocksPointerFromContainedPtr(const void* Ptr, uint8 PagesPlatformForBlockOfBlocks, uint32& OutBlockOfBlocksIndex)
	{
		uint32 PoolIndex = PoolIndexFromPtrChecked(Ptr);
		uint8* PoolStart = PoolBasePtr(PoolIndex);
		uint64 BlockOfBlocksIndex = (UPTRINT(Ptr) - UPTRINT(PoolStart)) / (UPTRINT(PagesPlatformForBlockOfBlocks) * UPTRINT(OsAllocationGranularity));
		OutBlockOfBlocksIndex = BlockOfBlocksIndex;

		uint8* Result = PoolStart + BlockOfBlocksIndex * UPTRINT(PagesPlatformForBlockOfBlocks) * UPTRINT(OsAllocationGranularity);

		check(Result < PoolStart + MAX_MEMORY_PER_BLOCK_SIZE);
		return Result;
	}
	FORCEINLINE uint8* BlockPointerFromIndecies(uint32 InPoolIndex, uint32 BlockOfBlocksIndex, uint32 BlockOfBlocksSize)
	{
		uint8* PoolStart = PoolBasePtr(InPoolIndex);
		uint8* Ptr = PoolStart + BlockOfBlocksIndex * uint64(BlockOfBlocksSize);
		check(Ptr + BlockOfBlocksSize <= PoolStart + MAX_MEMORY_PER_BLOCK_SIZE);
		return Ptr;
	}
	FPoolInfoSmall* PushNewPoolToFront(FPoolTable& Table, uint32 InBlockSize, uint32 InPoolIndex, uint32& OutBlockOfBlocksIndex);
	FPoolInfoSmall* GetFrontPool(FPoolTable& Table, uint32 InPoolIndex, uint32& OutBlockOfBlocksIndex);

public:

	FMallocBinned3();

	virtual ~FMallocBinned3();

	// FMalloc interface.
	virtual bool IsInternallyThreadSafe() const override;
	FORCEINLINE virtual void* Malloc(SIZE_T Size, uint32 Alignment) override
	{
		void* Result = nullptr;
	
		// Only allocate from the small pools if the size is small enough and the alignment isn't crazy large.
		// With large alignments, we'll waste a lot of memory allocating an entire page, but such alignments are highly unlikely in practice.
		if ((Size <= BINNED3_MAX_SMALL_POOL_SIZE) & (Alignment <= BINNED3_MINIMUM_ALIGNMENT)) // one branch, not two
		{
			FPerThreadFreeBlockLists* Lists = GMallocBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
			if (Lists)
			{
				uint32 PoolIndex = BoundSizeToPoolIndex(Size, MemSizeToIndex);
				uint32 BlockSize = PoolIndexToBlockSize(PoolIndex);
				Result = Lists->Malloc(PoolIndex);
#if BINNED3_ALLOCATOR_STATS
				if (Result)
				{
					SmallPoolTables[PoolIndex].HeadEndAlloc(Size);
					Lists->AllocatedMemory += BlockSize;
				}
#endif
			}
		}
		if (Result == nullptr)
		{
			Result = MallocExternal(Size, Alignment);
		}

		return Result;
	}
	FORCEINLINE virtual void* Realloc(void* Ptr, SIZE_T NewSize, uint32 Alignment) override
	{
		if (NewSize <= BINNED3_MAX_SMALL_POOL_SIZE && Alignment <= BINNED3_MINIMUM_ALIGNMENT) // one branch, not two
		{
			FPerThreadFreeBlockLists* Lists = GMallocBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;

			uint64 PoolIndex = PoolIndexFromPtr(Ptr);
			if ((!!Lists) & ((!Ptr) | (PoolIndex < BINNED3_SMALL_POOL_COUNT)))
			{
				uint32 BlockSize = 0;

				bool bCanFree = true; // the nullptr is always "freeable"
				if (Ptr)
				{
					// Reallocate to a smaller/bigger pool if necessary
					BlockSize = PoolIndexToBlockSize(PoolIndex);
					if ((!!NewSize) & (NewSize <= BlockSize) & ((!PoolIndex) | (NewSize > PoolIndexToBlockSize(static_cast<uint32>(PoolIndex - 1)))))
					{
#if BINNED3_ALLOCATOR_STATS
						SmallPoolTables[PoolIndex].HeadEndAlloc(NewSize);
						SmallPoolTables[PoolIndex].HeadEndFree();
#endif
						return Ptr;
					}
					bCanFree = Lists->CanFree(PoolIndex, BlockSize);
				}
				if (bCanFree)
				{
					uint32 NewPoolIndex = BoundSizeToPoolIndex(NewSize, MemSizeToIndex);
					uint32 NewBlockSize = PoolIndexToBlockSize(NewPoolIndex);
					void* Result = NewSize ? Lists->Malloc(NewPoolIndex) : nullptr;
#if BINNED3_ALLOCATOR_STATS
					if (Result)
					{
						SmallPoolTables[NewPoolIndex].HeadEndAlloc(NewSize);
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
#if BINNED3_ALLOCATOR_STATS
							SmallPoolTables[PoolIndex].HeadEndFree();
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
		uint64 PoolIndex = PoolIndexFromPtr(Ptr);
		if (PoolIndex < BINNED3_SMALL_POOL_COUNT)
		{
			FPerThreadFreeBlockLists* Lists = GMallocBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
			if (Lists)
			{
				int32 BlockSize = PoolIndexToBlockSize(PoolIndex);
				if (Lists->Free(Ptr, PoolIndex, BlockSize))
				{
#if BINNED3_ALLOCATOR_STATS
					SmallPoolTables[PoolIndex].HeadEndFree();
					Lists->AllocatedMemory -= BlockSize;
#endif
					return;
				}
			}
		}
		FreeExternal(Ptr);
	}
	FORCEINLINE virtual bool GetAllocationSize(void *Ptr, SIZE_T &SizeOut) override
	{
		uint64 PoolIndex = PoolIndexFromPtr(Ptr);
		if (PoolIndex < BINNED3_SMALL_POOL_COUNT)
		{
			SizeOut = PoolIndexToBlockSize(PoolIndex);
			return true;
		}
		return GetAllocationSizeExternal(Ptr, SizeOut);
	}

	FORCEINLINE virtual SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment) override
	{
		return QuantizeSizeCommon(Count, Alignment, *this);
	}

	virtual bool ValidateHeap() override;
	virtual void Trim(bool bTrimThreadCaches) override;
	virtual void SetupTLSCachesOnCurrentThread() override;
	virtual void ClearAndDisableTLSCachesOnCurrentThread() override;
	virtual const TCHAR* GetDescriptiveName() override;
	// End FMalloc interface.

	void FlushCurrentThreadCache();
	void* MallocExternal(SIZE_T Size, uint32 Alignment);
	void* ReallocExternal(void* Ptr, SIZE_T NewSize, uint32 Alignment);
	void FreeExternal(void *Ptr);
	bool GetAllocationSizeExternal(void* Ptr, SIZE_T& SizeOut);

#if BINNED3_ALLOCATOR_STATS
	int64 GetTotalAllocatedSmallPoolMemory() const;
#endif
	virtual void GetAllocatorStats( FGenericMemoryStats& out_Stats ) override;
	/** Dumps current allocator stats to the log. */
	virtual void DumpAllocatorStats(class FOutputDevice& Ar) override;
	
	// +1 enables PoolIndexToBlockSize(~0u / -1) dummy access that helps avoid PoolIndex == 0 branching in Realloc(),
	// see ((!PoolIndex) | (NewSize > PoolIndexToBlockSize(static_cast<uint32>(PoolIndex - 1)))
	static uint16 SmallBlockSizesReversedShifted[BINNED3_SMALL_POOL_COUNT + 1]; // this is reversed to get the smallest elements on our main cache line
	static FMallocBinned3* MallocBinned3;
	static uint32 OsAllocationGranularity;

	static void RegisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists);
	static void UnregisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists);

#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	static uint8* Binned3BaseVMPtr;
	FPlatformMemory::FPlatformVirtualMemoryBlock Binned3BaseVMBlock;
#else
	static uint64 PoolSearchDiv; // if this is zero, the VM turned out to be contiguous anyway so we use a simple subtract and shift
	static uint8* HighestPoolBaseVMPtr; // this is a duplicate of PoolBaseVMPtr[BINNED3_SMALL_POOL_COUNT - 1]
	static uint8* PoolBaseVMPtr[BINNED3_SMALL_POOL_COUNT];
	FPlatformMemory::FPlatformVirtualMemoryBlock PoolBaseVMBlock[BINNED3_SMALL_POOL_COUNT];
#endif
	// Mapping of sizes to small table indices
	static uint8 MemSizeToIndex[1 + (BINNED3_MAX_SMALL_POOL_SIZE >> BINNED3_MINIMUM_ALIGNMENT_SHIFT)];

	FORCEINLINE uint32 PoolIndexToBlockSize(uint32 PoolIndex) const
	{
		return uint32(SmallBlockSizesReversedShifted[BINNED3_SMALL_POOL_COUNT - PoolIndex - 1]) << BINNED3_MINIMUM_ALIGNMENT_SHIFT;
	}

	void Commit(uint32 InPoolIndex, void *Ptr, SIZE_T Size);
	void Decommit(uint32 InPoolIndex, void *Ptr, SIZE_T Size);

	static void* AllocateMetaDataMemory(SIZE_T Size);
	static void FreeMetaDataMemory(void *Ptr, SIZE_T Size);
};

PRAGMA_RESTORE_UNSAFE_TYPECAST_WARNINGS

#define BINNED3_INLINE (1)
#if BINNED3_INLINE // during development, it helps with iteration time to not include these here, but rather in the .cpp
	#if PLATFORM_USES_FIXED_GMalloc_CLASS && !FORCE_ANSI_ALLOCATOR && 0/*USE_MALLOC_BINNED3*/
		#define FMEMORY_INLINE_FUNCTION_DECORATOR  FORCEINLINE
		#define FMEMORY_INLINE_GMalloc (FMallocBinned3::MallocBinned3)
		#include "FMemory.inl" // IWYU pragma: export
	#endif
#endif
#endif


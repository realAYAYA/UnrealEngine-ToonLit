// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/MallocBinned3.h"

PRAGMA_DISABLE_UNSAFE_TYPECAST_WARNINGS

#if PLATFORM_64BITS && PLATFORM_HAS_FPlatformVirtualMemoryBlock
#include "Logging/LogMacros.h"
#include "Misc/ScopeLock.h"
#include "Templates/Function.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "HAL/MemoryMisc.h"
#include "HAL/PlatformMisc.h"

#if USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
#include "HAL/Allocators/CachedOSPageAllocator.h"
#define BINNED3_MAX_CACHED_OS_FREES (64)
#define BINNED3_MAX_CACHED_OS_FREES_BYTE_LIMIT (64*1024*1024)

typedef TCachedOSPageAllocator<BINNED3_MAX_CACHED_OS_FREES, BINNED3_MAX_CACHED_OS_FREES_BYTE_LIMIT> TBinned3CachedOSPageAllocator;

TBinned3CachedOSPageAllocator& GetCachedOSPageAllocator()
{
	static TBinned3CachedOSPageAllocator Singleton;
	return Singleton;
}

#endif

#if BINNED3_ALLOW_RUNTIME_TWEAKING

int32 GMallocBinned3PerThreadCaches = DEFAULT_GMallocBinned3PerThreadCaches;
static FAutoConsoleVariableRef GMallocBinned3PerThreadCachesCVar(
	TEXT("MallocBinned3.PerThreadCaches"),
	GMallocBinned3PerThreadCaches,
	TEXT("Enables per-thread caches of small (<= 32768 byte) allocations from FMallocBinned3")
	);

int32 GMallocBinned3MaxBundlesBeforeRecycle = BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle;
static FAutoConsoleVariableRef GMallocBinned3MaxBundlesBeforeRecycleCVar(
	TEXT("MallocBinned3.BundleRecycleCount"),
	GMallocBinned3MaxBundlesBeforeRecycle,
	TEXT("Number of freed bundles in the global recycler before it returns them to the system, per-block size. Limited by BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle (currently 4)")
	);

int32 GMallocBinned3AllocExtra = DEFAULT_GMallocBinned3AllocExtra;
static FAutoConsoleVariableRef GMallocBinned3AllocExtraCVar(
	TEXT("MallocBinned3.AllocExtra"),
	GMallocBinned3AllocExtra,
	TEXT("When we do acquire the lock, how many blocks cached in TLS caches. In no case will we grab more than a page.")
	);

#endif

float GMallocBinned3FlushThreadCacheMaxWaitTime = 0.02f;
static FAutoConsoleVariableRef GMallocBinned3FlushThreadCacheMaxWaitTimeCVar(
	TEXT("MallocBinned3.FlushThreadCacheMaxWaitTime"),
	GMallocBinned3FlushThreadCacheMaxWaitTime,
	TEXT("The threshold of time before warning about FlushCurrentThreadCache taking too long (seconds)."),
	ECVF_ReadOnly
);

#if BINNED3_ALLOCATOR_STATS
int64 Binned3AllocatedSmallPoolMemory = 0; // memory that's requested to be allocated by the game
int64 Binned3AllocatedOSSmallPoolMemory = 0;

int64 Binned3AllocatedLargePoolMemory = 0; // memory requests to the OS which don't fit in the small pool
int64 Binned3AllocatedLargePoolMemoryWAlignment = 0; // when we allocate at OS level we need to align to a size

TAtomic<int64> Binned3Commits;
TAtomic<int64> Binned3Decommits;
int64 Binned3PoolInfoMemory = 0;
int64 Binned3HashMemory = 0;
int64 Binned3FreeBitsMemory = 0;
TAtomic<int64> Binned3TotalPoolSearches;
TAtomic<int64> Binned3TotalPointerTests;


#endif

#define BINNED3_TIME_LARGE_BLOCKS (0)

#if BINNED3_TIME_LARGE_BLOCKS
TAtomic<double> MemoryRangeReserveTotalTime(0.0);
TAtomic<int32> MemoryRangeReserveTotalCount(0);

TAtomic<double> MemoryRangeFreeTotalTime(0.0);
TAtomic<int32> MemoryRangeFreeTotalCount(0);
#endif

#define BINNED3_LARGE_POOL_CANARIES 1 // need to repad the data structure so that the pagesize divides by this to disable

MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) static uint8 Binned3UnusedAlignPadding[PLATFORM_CACHE_LINE_SIZE] GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE) = { 0 };
uint16 FMallocBinned3::SmallBlockSizesReversedShifted[BINNED3_SMALL_POOL_COUNT + 1] = { 0 };
uint32 FMallocBinned3::OsAllocationGranularity = 0;

#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	uint8* FMallocBinned3::Binned3BaseVMPtr = nullptr;
#else
	uint64 FMallocBinned3::PoolSearchDiv = 0;
	uint8* FMallocBinned3::HighestPoolBaseVMPtr = nullptr;
	uint8* FMallocBinned3::PoolBaseVMPtr[BINNED3_SMALL_POOL_COUNT] = { nullptr };
#endif

FMallocBinned3* FMallocBinned3::MallocBinned3 = nullptr;
// Mapping of sizes to small table indices
uint8 FMallocBinned3::MemSizeToIndex[1 + (BINNED3_MAX_SMALL_POOL_SIZE >> BINNED3_MINIMUM_ALIGNMENT_SHIFT)] = { 0 };

struct FMallocBinned3::FPoolInfoSmall
{
	enum ECanary
	{
		SmallUnassigned = 0x3,
		SmallAssigned = 0x1
	};

	uint32 Canary : 2;
	uint32 Taken : 15;
	uint32 NoFirstFreeIndex : 1;
	uint32 FirstFreeIndex : 14;

	FPoolInfoSmall()
		: Canary(ECanary::SmallUnassigned)
		, Taken(0)
		, NoFirstFreeIndex(1)
		, FirstFreeIndex(0)
	{
		static_assert(sizeof(FPoolInfoSmall) == 4, "Padding fail");
	}
	void CheckCanary(ECanary ShouldBe) const
	{
		if (Canary != ShouldBe)
		{
			UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x"), int32(Canary), int32(ShouldBe));
		}
	}
	void SetCanary(ECanary ShouldBe, bool bPreexisting, bool bGuarnteedToBeNew)
	{
		if (bPreexisting)
		{
			if (bGuarnteedToBeNew)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x. This block is both preexisting and guaranteed to be new; which makes no sense."), int32(Canary), int32(ShouldBe));
			}
			if (ShouldBe == ECanary::SmallUnassigned)
			{
				if (Canary != ECanary::SmallAssigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x because this block should be preexisting and in use."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x because this block should be preexisting."), int32(Canary), int32(ShouldBe));
			}
		}
		else
		{
			if (bGuarnteedToBeNew)
			{
				if (Canary != ECanary::SmallUnassigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x. This block is guaranteed to be new yet is it already assigned."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe && Canary != ECanary::SmallUnassigned)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x does not have an expected value."), int32(Canary), int32(ShouldBe));
			}
		}
		Canary = ShouldBe;
	}
	bool HasFreeRegularBlock() const
	{
		CheckCanary(ECanary::SmallAssigned);
		return !NoFirstFreeIndex;
	}

	void* AllocateRegularBlock(uint8* BlockOfBlocksPtr, uint32 BlockSize)
	{
		check(HasFreeRegularBlock());
		++Taken;
		check(Taken != 0);
		FFreeBlock* Free = (FFreeBlock*)(BlockOfBlocksPtr + BlockSize * FirstFreeIndex);
		void* Result = Free->AllocateRegularBlock();
		if (Free->GetNumFreeRegularBlocks() == 0)
		{
			if (Free->NextFreeIndex == MAX_uint32)
			{
				FirstFreeIndex = 0;
				NoFirstFreeIndex = 1;
			}
			else
			{
				FirstFreeIndex = Free->NextFreeIndex;
				check(uint32(FirstFreeIndex) == Free->NextFreeIndex);
				check(((FFreeBlock*)(BlockOfBlocksPtr + BlockSize * FirstFreeIndex))->GetNumFreeRegularBlocks());
			}
		}

		return Result;
	}
};

struct FMallocBinned3::FPoolInfoLarge
{
	enum class ECanary : uint32
	{
		LargeUnassigned = 0x39431234,
		LargeAssigned = 0x17ea5678,
	};

public:
	ECanary	Canary;	// See ECanary
private:
	uint32 AllocSize;      // Number of bytes allocated
	uint32 VMSizeDivVirtualSizeAlignment;    // Number of VM bytes allocated aligned for OS
	uint32 CommitSize;    // Number of bytes committed by the OS

public:
	FPoolInfoLarge() :
		Canary(ECanary::LargeUnassigned),
		AllocSize(0),
		VMSizeDivVirtualSizeAlignment(0),
		CommitSize(0)
	{
	}
	void CheckCanary(ECanary ShouldBe) const
	{
		if (Canary != ShouldBe)
		{
			UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x"), int32(Canary), int32(ShouldBe));
		}
	}
	void SetCanary(ECanary ShouldBe, bool bPreexisting, bool bGuarnteedToBeNew)
	{
		if (bPreexisting)
		{
			if (bGuarnteedToBeNew)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x. This block is both preexisting and guaranteed to be new; which makes no sense."), int32(Canary), int32(ShouldBe));
			}
			if (ShouldBe == ECanary::LargeUnassigned)
			{
				if (Canary != ECanary::LargeAssigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x because this block should be preexisting and in use."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x because this block should be preexisting."), int32(Canary), int32(ShouldBe));
			}
		}
		else
		{
			if (bGuarnteedToBeNew)
			{
				if (Canary != ECanary::LargeUnassigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x. This block is guaranteed to be new yet is it already assigned."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe && Canary != ECanary::LargeUnassigned)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x does not have an expected value."), int32(Canary), int32(ShouldBe));
			}
		}
		Canary = ShouldBe;
	}
	uint32 GetOSRequestedBytes() const
	{
		return AllocSize;
	}

	UPTRINT GetOsCommittedBytes() const
	{
		return (UPTRINT)CommitSize;
	}

	uint32 GetOsVMPages() const
	{
		CheckCanary(ECanary::LargeAssigned);
		return VMSizeDivVirtualSizeAlignment;
	}

	void SetOSAllocationSizes(uint32 InRequestedBytes)
	{
		CheckCanary(ECanary::LargeAssigned);
		AllocSize = InRequestedBytes;
		check(AllocSize > 0 && CommitSize >= AllocSize && VMSizeDivVirtualSizeAlignment * FPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment() >= CommitSize);
	}

	void SetOSAllocationSizes(uint32 InRequestedBytes, UPTRINT InCommittedBytes, uint32 InVMSizeDivVirtualSizeAlignment)
	{
		CheckCanary(ECanary::LargeAssigned);
		AllocSize = InRequestedBytes;
		CommitSize = InCommittedBytes;
		VMSizeDivVirtualSizeAlignment = InVMSizeDivVirtualSizeAlignment;
		check(AllocSize > 0 && CommitSize >= AllocSize && VMSizeDivVirtualSizeAlignment * FPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment() >= CommitSize);
	}
};

struct FMallocBinned3::Private
{
	// Implementation. 
	static CA_NO_RETURN void OutOfMemory(uint64 Size, uint32 Alignment=0)
	{
		// this is expected not to return
		FPlatformMemory::OnOutOfMemory(Size, Alignment);
	}

	/**
	* Gets the FPoolInfoSmall for a small block memory address. If no valid info exists one is created.
	*/
	static FPoolInfoSmall* GetOrCreatePoolInfoSmall(FMallocBinned3& Allocator, uint32 InPoolIndex, uint32 BlockOfBlocksIndex)
	{
		FPoolInfoSmall*& InfoBlock = Allocator.SmallPoolTables[InPoolIndex].PoolInfos[BlockOfBlocksIndex / Allocator.SmallPoolInfosPerPlatformPage];
		if (!InfoBlock)
		{
			InfoBlock = (FPoolInfoSmall*)Allocator.AllocateMetaDataMemory(Allocator.OsAllocationGranularity);
#if BINNED3_ALLOCATOR_STATS
			Binned3PoolInfoMemory += Allocator.OsAllocationGranularity;
#endif
			DefaultConstructItems<FPoolInfoSmall>((void*)InfoBlock, Allocator.SmallPoolInfosPerPlatformPage);
		}

		FPoolInfoSmall* Result = &InfoBlock[BlockOfBlocksIndex % Allocator.SmallPoolInfosPerPlatformPage];

		bool bGuaranteedToBeNew = false;
		if (BlockOfBlocksIndex >= Allocator.SmallPoolTables[InPoolIndex].NumEverUsedBlockOfBlocks)
		{
			bGuaranteedToBeNew = true;
			Allocator.SmallPoolTables[InPoolIndex].NumEverUsedBlockOfBlocks = BlockOfBlocksIndex + 1;
		}
		Result->SetCanary(FPoolInfoSmall::ECanary::SmallAssigned, false, bGuaranteedToBeNew);
		return Result;
	}
	/**
	 * Gets the FPoolInfoLarge for a large block memory address. If no valid info exists one is created.
	 */
	static FPoolInfoLarge* GetOrCreatePoolInfoLarge(FMallocBinned3& Allocator, void* InPtr)
	{
		/** 
		 * Creates an array of FPoolInfo structures for tracking allocations.
		 */
		auto CreatePoolArray = [&Allocator](uint64 NumPools)
		{
			uint64 PoolArraySize = NumPools * sizeof(FPoolInfoLarge);

			void* Result = Allocator.AllocateMetaDataMemory(PoolArraySize);
#if BINNED3_ALLOCATOR_STATS
			Binned3PoolInfoMemory += PoolArraySize;
#endif
			if (!Result)
			{
				OutOfMemory(PoolArraySize);
			}

			DefaultConstructItems<FPoolInfoLarge>(Result, NumPools);
			return (FPoolInfoLarge*)Result;
		};

		uint32 BucketIndex;
		UPTRINT BucketIndexCollision;
		uint32  PoolIndex;
		Allocator.PtrToPoolMapping.GetHashBucketAndPoolIndices(InPtr, BucketIndex, BucketIndexCollision, PoolIndex);

		PoolHashBucket* FirstBucket = &Allocator.HashBuckets[BucketIndex];
		PoolHashBucket* Collision   = FirstBucket;
		do
		{
			if (!Collision->FirstPool)
			{
				Collision->BucketIndex = BucketIndexCollision;
				Collision->FirstPool = CreatePoolArray(Allocator.NumLargePoolsPerPage);
				Collision->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, true);
				return &Collision->FirstPool[PoolIndex];
			}

			if (Collision->BucketIndex == BucketIndexCollision)
			{
				Collision->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, false);
				return &Collision->FirstPool[PoolIndex];
			}

			Collision = Collision->Next;
		}
		while (Collision != FirstBucket);

		// Create a new hash bucket entry
		if (!Allocator.HashBucketFreeList)
		{
			{
				Allocator.HashBucketFreeList = (PoolHashBucket*)Allocator.AllocateMetaDataMemory(FMallocBinned3::OsAllocationGranularity);
#if BINNED3_ALLOCATOR_STATS
				Binned3HashMemory += FMallocBinned3::OsAllocationGranularity;
#endif
			}

			for (UPTRINT i = 0, n = FMallocBinned3::OsAllocationGranularity / sizeof(PoolHashBucket); i < n; ++i)
			{
				Allocator.HashBucketFreeList->Link(new (Allocator.HashBucketFreeList + i) PoolHashBucket());
			}
		}

		PoolHashBucket* NextFree  = Allocator.HashBucketFreeList->Next;
		PoolHashBucket* NewBucket = Allocator.HashBucketFreeList;

		NewBucket->Unlink();

		if (NextFree == NewBucket)
		{
			NextFree = nullptr;
		}
		Allocator.HashBucketFreeList = NextFree;

		if (!NewBucket->FirstPool)
		{
			NewBucket->FirstPool = CreatePoolArray(Allocator.NumLargePoolsPerPage);
			NewBucket->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, true);
		}
		else
		{
			NewBucket->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, false);
		}

		NewBucket->BucketIndex = BucketIndexCollision;

		FirstBucket->Link(NewBucket);

		return &NewBucket->FirstPool[PoolIndex];
	}

	static FPoolInfoLarge* FindPoolInfo(FMallocBinned3& Allocator, void* InPtr)
	{
		uint32 BucketIndex;
		UPTRINT BucketIndexCollision;
		uint32  PoolIndex;
		Allocator.PtrToPoolMapping.GetHashBucketAndPoolIndices(InPtr, BucketIndex, BucketIndexCollision, PoolIndex);


		PoolHashBucket* FirstBucket = &Allocator.HashBuckets[BucketIndex];
		PoolHashBucket* Collision   = FirstBucket;
		do
		{
			if (Collision->BucketIndex == BucketIndexCollision)
			{
				return &Collision->FirstPool[PoolIndex];
			}

			Collision = Collision->Next;
		}
		while (Collision != FirstBucket);

		return nullptr;
	}

	struct FGlobalRecycler
	{
		bool PushBundle(uint32 InPoolIndex, FBundleNode* InBundle)
		{
			uint32 NumCachedBundles = FMath::Min<uint32>(GMallocBinned3MaxBundlesBeforeRecycle, BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle);
			for (uint32 Slot = 0; Slot < NumCachedBundles; Slot++)
			{
				if (!Bundles[InPoolIndex].FreeBundles[Slot])
				{
					if (!FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Bundles[InPoolIndex].FreeBundles[Slot], InBundle, nullptr))
					{
						return true;
					}
				}
			}
			return false;
		}

		FBundleNode* PopBundle(uint32 InPoolIndex)
		{
			uint32 NumCachedBundles = FMath::Min<uint32>(GMallocBinned3MaxBundlesBeforeRecycle, BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle);
			for (uint32 Slot = 0; Slot < NumCachedBundles; Slot++)
			{
				FBundleNode* Result = Bundles[InPoolIndex].FreeBundles[Slot];
				if (Result)
				{
					if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Bundles[InPoolIndex].FreeBundles[Slot], nullptr, Result) == Result)
					{
						return Result;
					}
				}
			}
			return nullptr;
		}

	private:
		struct FPaddedBundlePointer
		{
			FBundleNode* FreeBundles[BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle];
#define BINNED3_BUNDLE_PADDING (PLATFORM_CACHE_LINE_SIZE - sizeof(FBundleNode*) * BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle)
#if (4 + (4 * PLATFORM_64BITS)) * BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle < PLATFORM_CACHE_LINE_SIZE
			uint8 Padding[BINNED3_BUNDLE_PADDING];
#endif
			FPaddedBundlePointer()
			{
				DefaultConstructItems<FBundleNode*>(FreeBundles, BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle);
			}
		};
		static_assert(sizeof(FPaddedBundlePointer) == PLATFORM_CACHE_LINE_SIZE, "FPaddedBundlePointer should be the same size as a cache line");
		MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) FPaddedBundlePointer Bundles[BINNED3_SMALL_POOL_COUNT] GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE);
	};

	static FGlobalRecycler GGlobalRecycler;

	static void FreeBundles(FMallocBinned3& Allocator, FBundleNode* BundlesToRecycle, uint32 InBlockSize, uint32 InPoolIndex)
	{
		FPoolTable& Table = Allocator.SmallPoolTables[InPoolIndex];

		FBundleNode* Bundle = BundlesToRecycle;
		while (Bundle)
		{
			FBundleNode* NextBundle = Bundle->NextBundle;

			FBundleNode* Node = Bundle;
			do
			{
				FBundleNode* NextNode = Node->NextNodeInCurrentBundle;

				uint32 OutBlockOfBlocksIndex;
				void* BasePtrOfNode = Allocator.BlockOfBlocksPointerFromContainedPtr(Node, Allocator.SmallPoolTables[InPoolIndex].PagesPlatformForBlockOfBlocks, OutBlockOfBlocksIndex);
				uint32 BlockWithinIndex = (((uint8*)Node) - ((uint8*)BasePtrOfNode)) / Allocator.SmallPoolTables[InPoolIndex].BlockSize;

				FPoolInfoSmall* NodePoolBlock = Allocator.SmallPoolTables[InPoolIndex].PoolInfos[OutBlockOfBlocksIndex / Allocator.SmallPoolInfosPerPlatformPage];
				if (!NodePoolBlock)
				{
					UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to free an unrecognized small block %p"), Node);
				}
				FPoolInfoSmall* NodePool = &NodePoolBlock[OutBlockOfBlocksIndex % Allocator.SmallPoolInfosPerPlatformPage];

				NodePool->CheckCanary(FPoolInfoSmall::ECanary::SmallAssigned);

				bool bWasExhaused = NodePool->NoFirstFreeIndex;

				// Free a pooled allocation.
				FFreeBlock* Free = (FFreeBlock*)Node;
				Free->NumFreeBlocks = 1;
				Free->NextFreeIndex = NodePool->NoFirstFreeIndex ? MAX_uint32 : NodePool->FirstFreeIndex;
				Free->BlockSizeShifted = (InBlockSize >> BINNED3_MINIMUM_ALIGNMENT_SHIFT);
				Free->Canary = FFreeBlock::CANARY_VALUE;
				Free->PoolIndex = InPoolIndex;
				NodePool->FirstFreeIndex = BlockWithinIndex;
				NodePool->NoFirstFreeIndex = 0;
				check(uint32(NodePool->FirstFreeIndex) == BlockWithinIndex);

				// Free this pool.
				check(NodePool->Taken >= 1);
				if (--NodePool->Taken == 0)
				{
					NodePool->SetCanary(FPoolInfoSmall::ECanary::SmallUnassigned, true, false);
					Table.BlockOfBlockAllocationBits.FreeBit(OutBlockOfBlocksIndex);

					uint64 AllocSize = Allocator.SmallPoolTables[InPoolIndex].PagesPlatformForBlockOfBlocks * Allocator.OsAllocationGranularity;

					if (!bWasExhaused)
					{
						Table.BlockOfBlockIsExhausted.AllocBit(OutBlockOfBlocksIndex);
					}

					Allocator.Decommit(InPoolIndex, BasePtrOfNode, AllocSize);
#if BINNED3_ALLOCATOR_STATS
					Binned3AllocatedOSSmallPoolMemory -= AllocSize;
#endif
				}
				else if (bWasExhaused)
				{
					Table.BlockOfBlockIsExhausted.FreeBit(OutBlockOfBlocksIndex);
				}

				Node = NextNode;
			} while (Node);

			Bundle = NextBundle;
		}
	}


	static FCriticalSection& GetFreeBlockListsRegistrationMutex()
	{
		static FCriticalSection FreeBlockListsRegistrationMutex;
		return FreeBlockListsRegistrationMutex;
	}
	static TArray<FPerThreadFreeBlockLists*>& GetRegisteredFreeBlockLists()
	{
		static TArray<FPerThreadFreeBlockLists*> RegisteredFreeBlockLists;
		return RegisteredFreeBlockLists;
	}
	static void RegisterThreadFreeBlockLists( FPerThreadFreeBlockLists* FreeBlockLists )
	{
		FScopeLock Lock(&GetFreeBlockListsRegistrationMutex());
		GetRegisteredFreeBlockLists().Add(FreeBlockLists);
	}
	static void UnregisterThreadFreeBlockLists( FPerThreadFreeBlockLists* FreeBlockLists )
	{
		FScopeLock Lock(&GetFreeBlockListsRegistrationMutex());
		GetRegisteredFreeBlockLists().Remove(FreeBlockLists);
#if BINNED3_ALLOCATOR_STATS
		ConsolidatedMemory.fetch_add(FreeBlockLists->AllocatedMemory, std::memory_order_relaxed);
#endif
	}
};

FMallocBinned3::Private::FGlobalRecycler FMallocBinned3::Private::GGlobalRecycler;

void FMallocBinned3::RegisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists)
{
	Private::RegisterThreadFreeBlockLists(FreeBlockLists);
}

void FMallocBinned3::UnregisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists)
{
	Private::UnregisterThreadFreeBlockLists(FreeBlockLists);
}

FMallocBinned3::FPoolInfoSmall* FMallocBinned3::PushNewPoolToFront(FMallocBinned3::FPoolTable& Table, uint32 InBlockSize, uint32 InPoolIndex, uint32& OutBlockOfBlocksIndex)
{
	const uint32 BlockOfBlocksSize = OsAllocationGranularity * Table.PagesPlatformForBlockOfBlocks;

	// Allocate memory.

	uint32 BlockOfBlocksIndex = Table.BlockOfBlockAllocationBits.AllocBit();
	if (BlockOfBlocksIndex == MAX_uint32)
	{
		return nullptr;
	}
	uint8* FreePtr = BlockPointerFromIndecies(InPoolIndex, BlockOfBlocksIndex, BlockOfBlocksSize);

	LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
	Commit(InPoolIndex, FreePtr, BlockOfBlocksSize);
	uint64 EndOffset = UPTRINT(FreePtr + BlockOfBlocksSize) - UPTRINT(PoolBasePtr(InPoolIndex));
	if (EndOffset > Table.UnusedAreaOffsetLow)
	{
		Table.UnusedAreaOffsetLow = EndOffset;
	}
	FFreeBlock* Free = new ((void*)FreePtr) FFreeBlock(BlockOfBlocksSize, InBlockSize, InPoolIndex);
#if BINNED3_ALLOCATOR_STATS
	Binned3AllocatedOSSmallPoolMemory += (int64)BlockOfBlocksSize;
#endif
	check(IsAligned(Free, OsAllocationGranularity));
	// Create pool
	FPoolInfoSmall* Result = Private::GetOrCreatePoolInfoSmall(*this, InPoolIndex, BlockOfBlocksIndex);
	Result->CheckCanary(FPoolInfoSmall::ECanary::SmallAssigned);
	Result->Taken = 0;
	Result->FirstFreeIndex = 0;
	Result->NoFirstFreeIndex = 0;
	Table.BlockOfBlockIsExhausted.FreeBit(BlockOfBlocksIndex);

	OutBlockOfBlocksIndex = BlockOfBlocksIndex;

	return Result;
}

FMallocBinned3::FPoolInfoSmall* FMallocBinned3::GetFrontPool(FPoolTable& Table, uint32 InPoolIndex, uint32& OutBlockOfBlocksIndex)
{
	OutBlockOfBlocksIndex = Table.BlockOfBlockIsExhausted.NextAllocBit();
	if (OutBlockOfBlocksIndex == MAX_uint32)
	{
		return nullptr;
	}
	return Private::GetOrCreatePoolInfoSmall(*this, InPoolIndex, OutBlockOfBlocksIndex);
}


FMallocBinned3::FMallocBinned3()
	: HashBucketFreeList(nullptr)
{
	static bool bOnce = false;
	check(!bOnce); // this is now a singleton-like thing and you cannot make multiple copies
	bOnce = true;

	check(!PLATFORM_32BITS);

	OsAllocationGranularity = FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment();
	checkf(FMath::IsPowerOfTwo(OsAllocationGranularity), TEXT("OS page size must be a power of two"));

	// First thing we try to allocate address space for bins as it might help us to move forward Constants.AddressStart and reduce the amount of available address space for the Large OS Allocs
	// Available address space is used to reserve hash map that can address all of that range, so less addressable space means less memory is allocated for book keeping
#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	Binned3BaseVMBlock = FPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(BINNED3_SMALL_POOL_COUNT * MAX_MEMORY_PER_BLOCK_SIZE, OsAllocationGranularity);
	Binned3BaseVMPtr = (uint8*)Binned3BaseVMBlock.GetVirtualPointer();
	check(IsAligned(Binned3BaseVMPtr, OsAllocationGranularity));
	verify(Binned3BaseVMPtr);
#else

	for (uint32 Index = 0; Index < BINNED3_SMALL_POOL_COUNT; ++Index)
	{
		FPlatformMemory::FPlatformVirtualMemoryBlock NewBLock = FPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(MAX_MEMORY_PER_BLOCK_SIZE, OsAllocationGranularity);

		uint8* NewVM = (uint8*)NewBLock.GetVirtualPointer();
		check(IsAligned(NewVM, OsAllocationGranularity));
		// insertion sort
		if (Index && NewVM < PoolBaseVMPtr[Index - 1])
		{
			uint32 InsertIndex = 0;
			for (; InsertIndex < Index; ++InsertIndex)
			{
				if (NewVM < PoolBaseVMPtr[InsertIndex])
				{
					break;
				}
			}
			check(InsertIndex < Index);
			for (uint32 MoveIndex = Index; MoveIndex > InsertIndex; --MoveIndex)
			{
				PoolBaseVMPtr[MoveIndex] = PoolBaseVMPtr[MoveIndex - 1];
				PoolBaseVMBlock[MoveIndex] = PoolBaseVMBlock[MoveIndex - 1];
			}
			PoolBaseVMPtr[InsertIndex] = NewVM;
			PoolBaseVMBlock[InsertIndex] = NewBLock;
		}
		else
		{
			PoolBaseVMPtr[Index] = NewVM;
			PoolBaseVMBlock[Index] = NewBLock;
		}
	}
	HighestPoolBaseVMPtr = PoolBaseVMPtr[BINNED3_SMALL_POOL_COUNT - 1];
	uint64 TotalGaps = 0;
	for (uint32 Index = 0; Index < BINNED3_SMALL_POOL_COUNT - 1; ++Index)
	{
		check(PoolBaseVMPtr[Index + 1] > PoolBaseVMPtr[Index]); // we sorted it
		check(PoolBaseVMPtr[Index + 1] >= PoolBaseVMPtr[Index] + MAX_MEMORY_PER_BLOCK_SIZE); // and blocks are non-overlapping
		TotalGaps += PoolBaseVMPtr[Index + 1] - (PoolBaseVMPtr[Index] + MAX_MEMORY_PER_BLOCK_SIZE);
	}
	if (TotalGaps == 0)
	{
		PoolSearchDiv = 0;
	}
	else if (TotalGaps < MAX_MEMORY_PER_BLOCK_SIZE)
	{
		PoolSearchDiv = MAX_MEMORY_PER_BLOCK_SIZE; // the gaps are not significant, ignoring them should give accurate searches
	}
	else
	{
		PoolSearchDiv = MAX_MEMORY_PER_BLOCK_SIZE + ((TotalGaps + BINNED3_SMALL_POOL_COUNT - 2) / (BINNED3_SMALL_POOL_COUNT - 1));
	}
#endif

	FGenericPlatformMemoryConstants Constants = FPlatformMemory::GetConstants();
#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	if (Constants.AddressStart == (uint64)Binned3BaseVMPtr)
	{
		Constants.AddressStart += Align(BINNED3_SMALL_POOL_COUNT * MAX_MEMORY_PER_BLOCK_SIZE, OsAllocationGranularity);
	}
#else
	if (!TotalGaps && Constants.AddressStart == (uint64)PoolBaseVMPtr[0])
	{
		Constants.AddressStart += Align(BINNED3_SMALL_POOL_COUNT * MAX_MEMORY_PER_BLOCK_SIZE, OsAllocationGranularity);
	}
#endif

	// large slab sizes are possible OsAllocationGranularity = 65536;
	NumLargePoolsPerPage = OsAllocationGranularity / sizeof(FPoolInfoLarge);
	check(OsAllocationGranularity % sizeof(FPoolInfoLarge) == 0);  // these need to divide evenly!
	PtrToPoolMapping.Init(OsAllocationGranularity, NumLargePoolsPerPage, Constants.AddressStart, Constants.AddressLimit);

	checkf(Constants.AddressLimit > OsAllocationGranularity, TEXT("OS address limit must be greater than the page size")); // Check to catch 32 bit overflow in AddressLimit
	static_assert(BINNED3_SMALL_POOL_COUNT <= 256, "Small block size array size must fit in a byte");
	static_assert(sizeof(FFreeBlock) <= BINNED3_MINIMUM_ALIGNMENT, "Free block struct must be small enough to fit into a block.");

	// Init pool tables.

	FSizeTableEntry SizeTable[BINNED3_SMALL_POOL_COUNT];

	verify(FSizeTableEntry::FillSizeTable(OsAllocationGranularity, SizeTable, BINNED3_BASE_PAGE_SIZE, BINNED3_MINIMUM_ALIGNMENT, BINNED3_MAX_SMALL_POOL_SIZE, BINNED3_BASE_PAGE_SIZE) == BINNED3_SMALL_POOL_COUNT);
	checkf(SizeTable[BINNED3_SMALL_POOL_COUNT - 1].BlockSize == BINNED3_MAX_SMALL_POOL_SIZE, TEXT("BINNED3_MAX_SMALL_POOL_SIZE must equal the largest block size"));
	checkf(sizeof(FMallocBinned3::FFreeBlock) <= SizeTable[0].BlockSize, TEXT("Pool header must be able to fit into the smallest block"));

	SmallPoolInfosPerPlatformPage = OsAllocationGranularity / sizeof(FPoolInfoSmall);

	uint32 RequiredMetaMem = 0;
	for (uint32 Index = 0; Index < BINNED3_SMALL_POOL_COUNT; ++Index)
	{
		checkf(Index == 0 || SizeTable[Index - 1].BlockSize < SizeTable[Index].BlockSize, TEXT("Small block sizes must be strictly increasing"));
		checkf(SizeTable[Index].BlockSize % BINNED3_MINIMUM_ALIGNMENT == 0, TEXT("Small block size must be a multiple of BINNED3_MINIMUM_ALIGNMENT"));

		SmallPoolTables[Index].BlockSize = SizeTable[Index].BlockSize;
		SmallPoolTables[Index].BlocksPerBlockOfBlocks = SizeTable[Index].BlocksPerBlockOfBlocks;
		SmallPoolTables[Index].PagesPlatformForBlockOfBlocks = SizeTable[Index].PagesPlatformForBlockOfBlocks;

		SmallPoolTables[Index].UnusedAreaOffsetLow = 0;
		SmallPoolTables[Index].NumEverUsedBlockOfBlocks = 0;
#if BINNED3_ALLOCATOR_PER_BIN_STATS
		SmallPoolTables[Index].TotalRequestedAllocSize.Store(0);
		SmallPoolTables[Index].TotalAllocCount.Store(0);
		SmallPoolTables[Index].TotalFreeCount.Store(0);
#endif

		const int64 TotalNumberOfBlocksOfBlocks = MAX_MEMORY_PER_BLOCK_SIZE / (SizeTable[Index].PagesPlatformForBlockOfBlocks * OsAllocationGranularity);
		const uint32 Size = Align(sizeof(FPoolInfoSmall**) * (TotalNumberOfBlocksOfBlocks + SmallPoolInfosPerPlatformPage - 1) / SmallPoolInfosPerPlatformPage, PLATFORM_CACHE_LINE_SIZE);
		RequiredMetaMem += Size;
#if BINNED3_ALLOCATOR_STATS
		Binned3PoolInfoMemory += Size;
#endif

		const int64 AllocationSize = Align(FBitTree::GetMemoryRequirements(TotalNumberOfBlocksOfBlocks), PLATFORM_CACHE_LINE_SIZE);
		RequiredMetaMem += AllocationSize * 2;
#if BINNED3_ALLOCATOR_STATS
		Binned3FreeBitsMemory += AllocationSize * 2;
#endif
	}

	RequiredMetaMem = Align(RequiredMetaMem, OsAllocationGranularity);
	uint8* MetaMem = (uint8*)AllocateMetaDataMemory(RequiredMetaMem);
	const uint8* MetaMemEnd = MetaMem + RequiredMetaMem;
	FMemory::Memzero(MetaMem, RequiredMetaMem);

	for (uint32 Index = 0; Index < BINNED3_SMALL_POOL_COUNT; ++Index)
	{
		const int64 TotalNumberOfBlocksOfBlocks = MAX_MEMORY_PER_BLOCK_SIZE / (SizeTable[Index].PagesPlatformForBlockOfBlocks * OsAllocationGranularity);
		const uint32 Size = Align(sizeof(FPoolInfoSmall**) * (TotalNumberOfBlocksOfBlocks + SmallPoolInfosPerPlatformPage - 1) / SmallPoolInfosPerPlatformPage, PLATFORM_CACHE_LINE_SIZE);

		SmallPoolTables[Index].PoolInfos = (FPoolInfoSmall**)MetaMem;
		MetaMem += Size;

		const int64 AllocationSize = Align(FBitTree::GetMemoryRequirements(TotalNumberOfBlocksOfBlocks), PLATFORM_CACHE_LINE_SIZE);
		SmallPoolTables[Index].BlockOfBlockAllocationBits.FBitTreeInit(TotalNumberOfBlocksOfBlocks, MetaMem, AllocationSize, false);
		MetaMem += AllocationSize;

		SmallPoolTables[Index].BlockOfBlockIsExhausted.FBitTreeInit(TotalNumberOfBlocksOfBlocks, MetaMem, AllocationSize, true);
		MetaMem += AllocationSize;
	}
	check(MetaMem <= MetaMemEnd);

	// Set up pool mappings
	uint8* IndexEntry = MemSizeToIndex;
	uint32  PoolIndex  = 0;
	for (uint32 Index = 0; Index != 1 + (BINNED3_MAX_SMALL_POOL_SIZE >> BINNED3_MINIMUM_ALIGNMENT_SHIFT); ++Index)
	{
		uint32 BlockSize = Index << BINNED3_MINIMUM_ALIGNMENT_SHIFT; // inverse of int32 Index = int32((Size >> BINNED3_MINIMUM_ALIGNMENT_SHIFT));
		while (SizeTable[PoolIndex].BlockSize < BlockSize)
		{
			++PoolIndex;
			check(PoolIndex != BINNED3_SMALL_POOL_COUNT);
		}
		check(PoolIndex < 256);
		*IndexEntry++ = uint8(PoolIndex);
	}

	// now reverse the pool sizes for cache coherency
	for (uint32 Index = 0; Index != BINNED3_SMALL_POOL_COUNT; ++Index)
	{
		uint32 Partner = BINNED3_SMALL_POOL_COUNT - Index - 1;
		SmallBlockSizesReversedShifted[Index] = (SizeTable[Partner].BlockSize >> BINNED3_MINIMUM_ALIGNMENT_SHIFT);
	}

	uint64 MaxHashBuckets = PtrToPoolMapping.GetMaxHashBuckets();
	{
		int64 HashAllocSize = Align(MaxHashBuckets * sizeof(PoolHashBucket), OsAllocationGranularity);
		HashBuckets = (PoolHashBucket*)AllocateMetaDataMemory(HashAllocSize);
#if BINNED3_ALLOCATOR_STATS
		Binned3HashMemory += HashAllocSize;
#endif
		verify(HashBuckets);
	}

	DefaultConstructItems<PoolHashBucket>(HashBuckets, MaxHashBuckets);
	MallocBinned3 = this;
	GFixedMallocLocationPtr = (FMalloc**)(&MallocBinned3);
}

FMallocBinned3::~FMallocBinned3()
{
}

void FMallocBinned3::Commit(uint32 InPoolIndex, void *Ptr, SIZE_T Size)
{

#if BINNED3_ALLOCATOR_STATS
	Binned3Commits++;
#endif
#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	Binned3BaseVMBlock.CommitByPtr(Ptr, Size);
#else
	PoolBaseVMBlock[InPoolIndex].CommitByPtr(Ptr, Size);
#endif
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Ptr, Size));
}
void FMallocBinned3::Decommit(uint32 InPoolIndex, void *Ptr, SIZE_T Size)
{
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));

#if BINNED3_ALLOCATOR_STATS
	Binned3Decommits++;
#endif
#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	Binned3BaseVMBlock.DecommitByPtr(Ptr, Size);
#else
	PoolBaseVMBlock[InPoolIndex].DecommitByPtr(Ptr, Size);
#endif
}

void* FMallocBinned3::AllocateMetaDataMemory(SIZE_T Size)
{
	LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
	size_t VirtualAlignedSize = Align(Size, FPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment());
	FPlatformMemory::FPlatformVirtualMemoryBlock Block = FPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(VirtualAlignedSize);
	size_t CommitAlignedSize = Align(Size, FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment());
	Block.Commit(0, CommitAlignedSize);
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Block.GetVirtualPointer(), CommitAlignedSize));
	return Block.GetVirtualPointer();
}

void FMallocBinned3::FreeMetaDataMemory(void *Ptr, SIZE_T InSize)
{
	if (Ptr)
	{
		LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));

		InSize = Align(InSize, FPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment());
		FPlatformMemory::FPlatformVirtualMemoryBlock Block(Ptr, InSize / FPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment());
		Block.FreeVirtual();
	}
}

bool FMallocBinned3::IsInternallyThreadSafe() const
{ 
	return true;
}

void* FMallocBinned3::MallocExternal(SIZE_T Size, uint32 Alignment)
{
	static_assert(DEFAULT_ALIGNMENT <= BINNED3_MINIMUM_ALIGNMENT, "DEFAULT_ALIGNMENT is assumed to be zero"); // used below

	// Fast path: Allocate from the small pools if the size is small enough and the alignment <= binned3 min alignment.
	//            Larger alignments can waste a lot of memory allocating an entire page, so some smaller alignments are 
	//			  handled in the fallback path if less than a predefined max small pool alignment. 

	bool UsePools = (Size <= BINNED3_MAX_SMALL_POOL_SIZE) && (Alignment <= BINNED3_MINIMUM_ALIGNMENT);
	
	if (!UsePools)
	{
		// fallback: test for non-default/mininum alignments and handle a subset of them to reduce memory waste of 64KB page requirements
		// 			 e.g. code that wants to use aligned avx loads will use 32 byte alignments 

		UsePools = PromoteToLargerBin(Size, Alignment, *this);
	}

	if (UsePools) 
	{
		uint32 PoolIndex = BoundSizeToPoolIndex(Size, MemSizeToIndex);
		FPerThreadFreeBlockLists* Lists = GMallocBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
		if (Lists)
		{
			if (Lists->ObtainRecycledPartial(PoolIndex, Private::GGlobalRecycler))
			{
				if (void* Result = Lists->Malloc(PoolIndex))
				{
#if BINNED3_ALLOCATOR_STATS
					SmallPoolTables[PoolIndex].HeadEndAlloc(Size);
					uint32 BlockSize = PoolIndexToBlockSize(PoolIndex);
					Lists->AllocatedMemory += BlockSize;
#endif
					return Result;
				}
			}
		}

		FScopeLock Lock(&Mutex);

		// Allocate from small object pool.
		FPoolTable& Table = SmallPoolTables[PoolIndex];

		uint32 BlockOfBlocksIndex = MAX_uint32;
		FPoolInfoSmall* Pool = GetFrontPool(Table, PoolIndex, BlockOfBlocksIndex);
		if (!Pool)
		{
			Pool = PushNewPoolToFront(Table, Table.BlockSize, PoolIndex, BlockOfBlocksIndex);
			
			//Indicates that we run out of Pool memory (512 MB) for this block type
			if (!Pool)
			{
				if ((PoolIndex + 1) < BINNED3_SMALL_POOL_COUNT)
				{
					return MallocExternal(SmallPoolTables[PoolIndex + 1].BlockSize, Alignment);
				}
				else
				{
					return MallocExternal(BINNED3_MAX_SMALL_POOL_SIZE + 1, Alignment);
				}
			}
		}

		const uint32 BlockOfBlocksSize = OsAllocationGranularity * Table.PagesPlatformForBlockOfBlocks;
		uint8* BlockOfBlocksPtr = BlockPointerFromIndecies(PoolIndex, BlockOfBlocksIndex, BlockOfBlocksSize);

		void* Result = Pool->AllocateRegularBlock(BlockOfBlocksPtr, Table.BlockSize);
#if BINNED3_ALLOCATOR_STATS
		Table.HeadEndAlloc(Size);
		Binned3AllocatedSmallPoolMemory += PoolIndexToBlockSize(PoolIndex);
#endif // BINNED3_ALLOCATOR_STATS
		if (GMallocBinned3AllocExtra)
		{
			if (Lists)
			{
				// prefill the free list with some allocations so we are less likely to hit this slow path with the mutex 
				for (int32 Index = 0; Index < GMallocBinned3AllocExtra && Pool->HasFreeRegularBlock(); Index++)
				{
					if (!Lists->Free(Result, PoolIndex, Table.BlockSize))
					{
						break;
					}
					Result = Pool->AllocateRegularBlock(BlockOfBlocksPtr, Table.BlockSize);
				}
			}
		}
		if (!Pool->HasFreeRegularBlock())
		{
			Table.BlockOfBlockIsExhausted.AllocBit(BlockOfBlocksIndex);
		}

		return Result;
	}
	Alignment = FMath::Max<uint32>(Alignment, BINNED3_MINIMUM_ALIGNMENT);
	Size = Align(FMath::Max((SIZE_T)1, Size), Alignment);

	check(FMath::IsPowerOfTwo(Alignment));

	// Use OS for non-pooled allocations.
	UPTRINT AlignedSize = Align(Size, FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment());

#if BINNED3_TIME_LARGE_BLOCKS
	double StartTime = FPlatformTime::Seconds();
#endif

	LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);

#if USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
	FScopeLock Lock(&Mutex);
	void* Result = GetCachedOSPageAllocator().Allocate(AlignedSize);
	check(IsAligned(Result, Alignment));
#else

	FPlatformMemory::FPlatformVirtualMemoryBlock Block = FPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(AlignedSize, Alignment);
	Block.Commit(0, AlignedSize);
	void* Result = Block.GetVirtualPointer();
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Result, AlignedSize));
#endif

#if BINNED3_TIME_LARGE_BLOCKS
	double Add = FPlatformTime::Seconds() - StartTime;
	double Old;
	do
	{
		Old = MemoryRangeReserveTotalTime.Load();
	} while (!MemoryRangeReserveTotalTime.CompareExchange(Old, Old + Add));
	MemoryRangeReserveTotalCount++;
#endif

	UE_CLOG(!IsAligned(Result, Alignment) ,LogMemory, Fatal, TEXT("FMallocBinned3 alignment was too large for OS. Alignment=%d   Ptr=%p"), Alignment, Result);

	if (!Result)
	{
		Private::OutOfMemory(AlignedSize);
	}
	check(IsOSAllocation(Result));
#if! USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
	FScopeLock Lock(&Mutex);
#endif


#if BINNED3_ALLOCATOR_STATS
	Binned3AllocatedLargePoolMemory += (int64)Size;
	Binned3AllocatedLargePoolMemoryWAlignment += AlignedSize;
#endif

	// Create pool.
	FPoolInfoLarge* Pool = Private::GetOrCreatePoolInfoLarge(*this, Result);
	check(Size > 0 && Size <= AlignedSize && AlignedSize >= FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment());
#if USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
	Pool->SetOSAllocationSizes(Size, AlignedSize, AlignedSize / FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment());
#else
	Pool->SetOSAllocationSizes(Size, AlignedSize, Block.GetActualSizeInPages());
#endif

	return Result;
}


void* FMallocBinned3::ReallocExternal(void* Ptr, SIZE_T NewSize, uint32 Alignment)
{
	if (NewSize == 0)
	{
		FMallocBinned3::FreeExternal(Ptr);
		return nullptr;
	}
	static_assert(DEFAULT_ALIGNMENT <= BINNED3_MINIMUM_ALIGNMENT, "DEFAULT_ALIGNMENT is assumed to be zero"); // used below
	check(FMath::IsPowerOfTwo(Alignment));
	check(Alignment <= OsAllocationGranularity);

	uint64 PoolIndex = PoolIndexFromPtr(Ptr);
	if (PoolIndex < BINNED3_SMALL_POOL_COUNT)
	{
		check(Ptr); // null is an OS allocation because it will not fall in our VM block
		uint32 BlockSize = PoolIndexToBlockSize(PoolIndex);
		if (
			((NewSize <= BlockSize) & (IsAligned(BlockSize, Alignment))) && // one branch, not two
			(PoolIndex == 0 || NewSize > PoolIndexToBlockSize(PoolIndex - 1)))
		{
#if BINNED3_ALLOCATOR_STATS
			SmallPoolTables[PoolIndex].HeadEndAlloc(NewSize);
			SmallPoolTables[PoolIndex].HeadEndFree();
#endif
			return Ptr;
		}

		// Reallocate and copy the data across
		void* Result = FMallocBinned3::MallocExternal(NewSize, Alignment);
		FMemory::Memcpy(Result, Ptr, FMath::Min<SIZE_T>(NewSize, BlockSize));
		FMallocBinned3::FreeExternal(Ptr);
		return Result;
	}
	if (!Ptr)
	{
		void* Result = FMallocBinned3::MallocExternal(NewSize, Alignment);
		return Result;
	}

	Mutex.Lock();

	// Allocated from OS.
	FPoolInfoLarge* Pool = Private::FindPoolInfo(*this, Ptr);
	if (!Pool)
	{
		UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to realloc an unrecognized block %p"), Ptr);
	}
	UPTRINT PoolOsBytes = Pool->GetOsCommittedBytes();
	uint32 PoolOSRequestedBytes = Pool->GetOSRequestedBytes();
	checkf(PoolOSRequestedBytes <= PoolOsBytes, TEXT("FMallocBinned3::ReallocExternal %d %d"), int32(PoolOSRequestedBytes), int32(PoolOsBytes));
	if (NewSize > PoolOsBytes || // can't fit in the old block
		(NewSize <= BINNED3_MAX_SMALL_POOL_SIZE && Alignment <= BINNED3_MINIMUM_ALIGNMENT) || // can switch to the small block allocator
		Align(NewSize, OsAllocationGranularity) < PoolOsBytes) // we can get some pages back
	{
		Mutex.Unlock();
		// Grow or shrink.
		void* Result = FMallocBinned3::MallocExternal(NewSize, Alignment);
		SIZE_T CopySize = FMath::Min<SIZE_T>(NewSize, PoolOSRequestedBytes);
		FMemory::Memcpy(Result, Ptr, CopySize);
		FMallocBinned3::FreeExternal(Ptr);
		return Result;
	}

#if BINNED3_ALLOCATOR_STATS
	Binned3AllocatedLargePoolMemory += ((int64)NewSize) - ((int64)PoolOSRequestedBytes);
	// don't need to change the Binned3AllocatedLargePoolMemoryWAlignment because we didn't reallocate so it's the same size
#endif
	
	Pool->SetOSAllocationSizes(NewSize);
	Mutex.Unlock();
	return Ptr;
}

void FMallocBinned3::FreeExternal(void* Ptr)
{
	uint64 PoolIndex = PoolIndexFromPtr(Ptr);
	if (PoolIndex < BINNED3_SMALL_POOL_COUNT)
	{
		check(Ptr); // null is an OS allocation because it will not fall in our VM block
		uint32 BlockSize = PoolIndexToBlockSize(PoolIndex);

		FBundleNode* BundlesToRecycle = nullptr;
		FPerThreadFreeBlockLists* Lists = GMallocBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
		if (Lists)
		{
			BundlesToRecycle = Lists->RecycleFullBundle(PoolIndex, Private::GGlobalRecycler);
			bool bPushed = Lists->Free(Ptr, PoolIndex, BlockSize);
			check(bPushed);
#if BINNED3_ALLOCATOR_STATS
			SmallPoolTables[PoolIndex].HeadEndFree();
			Lists->AllocatedMemory -= BlockSize;
#endif
		}
		else
		{
			BundlesToRecycle = (FBundleNode*)Ptr;
			BundlesToRecycle->NextNodeInCurrentBundle = nullptr;
		}
		if (BundlesToRecycle)
		{
			BundlesToRecycle->NextBundle = nullptr;
			FScopeLock Lock(&Mutex);
			Private::FreeBundles(*this, BundlesToRecycle, BlockSize, PoolIndex);
#if BINNED3_ALLOCATOR_STATS
			if (!Lists)
			{
				SmallPoolTables[PoolIndex].HeadEndFree();
				// lists track their own stat track them instead in the global stat if we don't have lists
				Binned3AllocatedSmallPoolMemory -= ((int64)(BlockSize));
			}
#endif
		}
	}
	else if (Ptr)
	{
#if USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
		FScopeLock Lock(&Mutex);
#endif
		uint32 VMPages;
		{
#if !USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
			FScopeLock Lock(&Mutex);
#endif
			FPoolInfoLarge* Pool = Private::FindPoolInfo(*this, Ptr);
			if (!Pool)
			{
				UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to free an unrecognized block %p"), Ptr);
			}
			UPTRINT PoolOsBytes = Pool->GetOsCommittedBytes();
			uint32 PoolOSRequestedBytes = Pool->GetOSRequestedBytes();
			VMPages = Pool->GetOsVMPages();

#if BINNED3_ALLOCATOR_STATS
			Binned3AllocatedLargePoolMemory -= ((int64)PoolOSRequestedBytes);
			Binned3AllocatedLargePoolMemoryWAlignment -= ((int64)PoolOsBytes);
#endif

			checkf(PoolOSRequestedBytes <= PoolOsBytes, TEXT("FMallocBinned3::FreeExternal %d %d"), int32(PoolOSRequestedBytes), int32(PoolOsBytes));
			Pool->SetCanary(FPoolInfoLarge::ECanary::LargeUnassigned, true, false);
		}

		// Free an OS allocation.
#if BINNED3_TIME_LARGE_BLOCKS
		double StartTime = FPlatformTime::Seconds();
#endif
		{
			LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));
#if USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
			GetCachedOSPageAllocator().Free(Ptr, VMPages * FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment());
#else
			FPlatformMemory::FPlatformVirtualMemoryBlock Block(Ptr, VMPages);
			Block.FreeVirtual();
#endif
		}
#if BINNED3_TIME_LARGE_BLOCKS
		double Add = FPlatformTime::Seconds() - StartTime;
		double Old;
		do
		{
			Old = MemoryRangeFreeTotalTime.Load();
		} while (!MemoryRangeFreeTotalTime.CompareExchange(Old, Old + Add));
		MemoryRangeFreeTotalCount++;
#endif
	}
}

bool FMallocBinned3::GetAllocationSizeExternal(void* Ptr, SIZE_T& SizeOut)
{
	uint64 PoolIndex = PoolIndexFromPtr(Ptr);
	if (PoolIndex < BINNED3_SMALL_POOL_COUNT)
	{
		check(Ptr); // null is an OS allocation because it will not fall in our VM block
		SizeOut = PoolIndexToBlockSize(PoolIndex);
		return true;
	}
	if (!Ptr)
	{
		return false;
	}
	FScopeLock Lock(&Mutex);
	FPoolInfoLarge* Pool = Private::FindPoolInfo(*this, Ptr);
	if (!Pool)
	{
		UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to GetAllocationSizeExternal an unrecognized block %p"), Ptr);
	}
	UPTRINT PoolOsBytes = Pool->GetOsCommittedBytes();
	uint32 PoolOSRequestedBytes = Pool->GetOSRequestedBytes();
	checkf(PoolOSRequestedBytes <= PoolOsBytes, TEXT("FMallocBinned3::GetAllocationSizeExternal %d %d"), int32(PoolOSRequestedBytes), int32(PoolOsBytes));
	SizeOut = PoolOsBytes;
	return true;
}

bool FMallocBinned3::ValidateHeap()
{
	// Not implemented
	// NumEverUsedBlockOfBlocks gives us all of the information we need to examine each pool, so it is doable.
	return true;
}

const TCHAR* FMallocBinned3::GetDescriptiveName()
{
	return TEXT("Binned3");
}

void FMallocBinned3::FlushCurrentThreadCache()
{
	double StartTimeInner = FPlatformTime::Seconds();
	TRACE_CPUPROFILER_EVENT_SCOPE(FMallocBinned3::FlushCurrentThreadCache);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FMallocBinned3_FlushCurrentThreadCache);
	FPerThreadFreeBlockLists* Lists = FPerThreadFreeBlockLists::Get();

	float WaitForMutexTime = 0.0f;
	float WaitForMutexAndTrimTime = 0.0f;

	if (Lists)
	{
		FScopeLock Lock(&Mutex);
		WaitForMutexTime = FPlatformTime::Seconds() - StartTimeInner;
		for (int32 PoolIndex = 0; PoolIndex != BINNED3_SMALL_POOL_COUNT; ++PoolIndex)
		{
			FBundleNode* Bundles = Lists->PopBundles(PoolIndex);
			if (Bundles)
			{
				Private::FreeBundles(*this, Bundles, PoolIndexToBlockSize(PoolIndex), PoolIndex);
			}
		}
		WaitForMutexAndTrimTime = FPlatformTime::Seconds() - StartTimeInner;
	}

	// These logs must happen outside the above mutex to avoid deadlocks
	if (WaitForMutexTime > GMallocBinned3FlushThreadCacheMaxWaitTime)
	{
		UE_LOG(LogMemory, Warning, TEXT("FMallocBinned3 took %6.2fms to wait for mutex for trim."), WaitForMutexTime * 1000.0f);
	}
	if (WaitForMutexAndTrimTime > GMallocBinned3FlushThreadCacheMaxWaitTime)
	{
		UE_LOG(LogMemory, Warning, TEXT("FMallocBinned3 took %6.2fms to wait for mutex AND trim."), WaitForMutexAndTrimTime * 1000.0f);
	}
}

#include "Async/TaskGraphInterfaces.h"

void FMallocBinned3::Trim(bool bTrimThreadCaches)
{

	if (GMallocBinned3PerThreadCaches  &&  bTrimThreadCaches)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FMallocBinned3_Trim);
		//double StartTime = FPlatformTime::Seconds();
		TFunction<void(ENamedThreads::Type CurrentThread)> Broadcast =
			[this](ENamedThreads::Type MyThread)
		{
			FlushCurrentThreadCache();
		};
		// Skip task threads on desktop platforms as it is too slow and they don't have much memory
		FTaskGraphInterface::BroadcastSlow_OnlyUseForSpecialPurposes(!PLATFORM_DESKTOP, false, Broadcast);
		//UE_LOG(LogTemp, Display, TEXT("Trim Broadcast = %6.2fms"), 1000.0f * float(FPlatformTime::Seconds() - StartTime));
	}
}

void FMallocBinned3::SetupTLSCachesOnCurrentThread()
{
	if (!BINNED3_ALLOW_RUNTIME_TWEAKING && !GMallocBinned3PerThreadCaches)
	{
		return;
	}
	if (!FPlatformTLS::IsValidTlsSlot(FMallocBinned3::BinnedTlsSlot))
	{
		FMallocBinned3::BinnedTlsSlot = FPlatformTLS::AllocTlsSlot();
	}
	check(FPlatformTLS::IsValidTlsSlot(FMallocBinned3::BinnedTlsSlot));
	FPerThreadFreeBlockLists::SetTLS();
}

void FMallocBinned3::ClearAndDisableTLSCachesOnCurrentThread()
{
	if (!BINNED3_ALLOW_RUNTIME_TWEAKING && !GMallocBinned3PerThreadCaches)
	{
		return;
	}
	FlushCurrentThreadCache();
	FPerThreadFreeBlockLists::ClearTLS();
}

void FMallocBinned3::FFreeBlock::CanaryFail() const
{
	UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to realloc an unrecognized block %p   canary == 0x%x != 0x%x"), (void*)this, (int32)Canary, (int32)FMallocBinned3::FFreeBlock::CANARY_VALUE);
}

#if BINNED3_ALLOCATOR_STATS
int64 FMallocBinned3::GetTotalAllocatedSmallPoolMemory() const
{
	int64 FreeBlockAllocatedMemory = 0;
	{
		FScopeLock Lock(&Private::GetFreeBlockListsRegistrationMutex());
		for (const FPerThreadFreeBlockLists* FreeBlockLists : Private::GetRegisteredFreeBlockLists())
		{
			FreeBlockAllocatedMemory += FreeBlockLists->AllocatedMemory;
		}
		FreeBlockAllocatedMemory += ConsolidatedMemory.load(std::memory_order_relaxed);
	}

	return Binned3AllocatedSmallPoolMemory + FreeBlockAllocatedMemory;
}
#endif

void FMallocBinned3::GetAllocatorStats( FGenericMemoryStats& OutStats )
{
#if BINNED3_ALLOCATOR_STATS

	int64 TotalAllocatedSmallPoolMemory = GetTotalAllocatedSmallPoolMemory();

	OutStats.Add(TEXT("Binned3AllocatedSmallPoolMemory"), TotalAllocatedSmallPoolMemory);
	OutStats.Add(TEXT("Binned3AllocatedOSSmallPoolMemory"), Binned3AllocatedOSSmallPoolMemory);
	OutStats.Add(TEXT("Binned3AllocatedLargePoolMemory"), Binned3AllocatedLargePoolMemory);
	OutStats.Add(TEXT("Binned3AllocatedLargePoolMemoryWAlignment"), Binned3AllocatedLargePoolMemoryWAlignment);

	uint64 TotalAllocated = TotalAllocatedSmallPoolMemory + Binned3AllocatedLargePoolMemory;
	uint64 TotalOSAllocated = Binned3AllocatedOSSmallPoolMemory + Binned3AllocatedLargePoolMemoryWAlignment;

	OutStats.Add(TEXT("TotalAllocated"), TotalAllocated);
	OutStats.Add(TEXT("TotalOSAllocated"), TotalOSAllocated);
#endif
	FMalloc::GetAllocatorStats(OutStats);
}

#if BINNED3_ALLOCATOR_STATS && BINNED3_USE_SEPARATE_VM_PER_POOL
void FMallocBinned3::RecordPoolSearch(uint32 Tests)
{
	Binned3TotalPoolSearches++;
	Binned3TotalPointerTests += Tests;
}
#endif


void FMallocBinned3::DumpAllocatorStats(class FOutputDevice& Ar)
{
#if BINNED3_ALLOCATOR_STATS

	int64 TotalAllocatedSmallPoolMemory = GetTotalAllocatedSmallPoolMemory();

	Ar.Logf(TEXT("FMallocBinned3 Mem report"));
	Ar.Logf(TEXT("Constants.BinnedAllocationGranularity = %d"), int32(OsAllocationGranularity));
	Ar.Logf(TEXT("BINNED3_MAX_SMALL_POOL_SIZE = %d"), int32(BINNED3_MAX_SMALL_POOL_SIZE));
	Ar.Logf(TEXT("MAX_MEMORY_PER_BLOCK_SIZE = %llu"), uint64(MAX_MEMORY_PER_BLOCK_SIZE));
	Ar.Logf(TEXT("Small Pool Allocations: %fmb  (including block size padding)"), ((double)TotalAllocatedSmallPoolMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Small Pool OS Allocated: %fmb"), ((double)Binned3AllocatedOSSmallPoolMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Large Pool Requested Allocations: %fmb"), ((double)Binned3AllocatedLargePoolMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Large Pool OS Allocated: %fmb"), ((double)Binned3AllocatedLargePoolMemoryWAlignment) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("PoolInfo: %fmb"), ((double)Binned3PoolInfoMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Hash: %fmb"), ((double)Binned3HashMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Free Bits: %fmb"), ((double)Binned3FreeBitsMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("TLS: %fmb"), ((double)TLSMemory.load(std::memory_order_relaxed)) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Slab Commits: %llu"), Binned3Commits.Load());
	Ar.Logf(TEXT("Slab Decommits: %llu"), Binned3Decommits.Load());
#if BINNED3_USE_SEPARATE_VM_PER_POOL
	Ar.Logf(TEXT("BINNED3_USE_SEPARATE_VM_PER_POOL is true - VM is Contiguous = %d"), PoolSearchDiv == 0);
	if (PoolSearchDiv)
	{
		Ar.Logf(TEXT("%llu Pointer Searches   %llu Pointer Compares    %llu Compares/Search"), Binned3TotalPoolSearches.Load(), Binned3TotalPointerTests.Load(), Binned3TotalPointerTests.Load() / Binned3TotalPoolSearches.Load());
		uint64 TotalMem = PoolBaseVMPtr[BINNED3_SMALL_POOL_COUNT - 1] + MAX_MEMORY_PER_BLOCK_SIZE - PoolBaseVMPtr[0];
		uint64 MinimumMem = uint64(BINNED3_SMALL_POOL_COUNT) * MAX_MEMORY_PER_BLOCK_SIZE;
		Ar.Logf(TEXT("Percent of gaps in the address range %6.4f  (hopefully < 1, or the searches above will suffer)"), 100.0f * (1.0f - float(MinimumMem) / float(TotalMem)));
	}
#else
	Ar.Logf(TEXT("BINNED3_USE_SEPARATE_VM_PER_POOL is false"));
#endif
	Ar.Logf(TEXT("Total allocated from OS: %fmb"), 
		((double)
			Binned3AllocatedOSSmallPoolMemory + Binned3AllocatedLargePoolMemoryWAlignment + Binned3PoolInfoMemory + Binned3HashMemory + Binned3FreeBitsMemory + TLSMemory.load(std::memory_order_relaxed)
			) / (1024.0f * 1024.0f));


#if BINNED3_TIME_LARGE_BLOCKS
	Ar.Logf(TEXT("MemoryRangeReserve %d calls %6.3fs    %6.3fus / call"), MemoryRangeReserveTotalCount.Load(), float(MemoryRangeReserveTotalTime.Load()), float(MemoryRangeReserveTotalTime.Load()) * 1000000.0f / float(MemoryRangeReserveTotalCount.Load()));
	Ar.Logf(TEXT("MemoryRangeFree    %d calls %6.3fs    %6.3fus / call"), MemoryRangeFreeTotalCount.Load(), float(MemoryRangeFreeTotalTime.Load()), float(MemoryRangeFreeTotalTime.Load()) * 1000000.0f / float(MemoryRangeFreeTotalCount.Load()));
#endif

#if BINNED3_ALLOCATOR_PER_BIN_STATS
	for (int32 PoolIndex = 0; PoolIndex < BINNED3_SMALL_POOL_COUNT; PoolIndex++)
	{
		
		int64 VM = SmallPoolTables[PoolIndex].UnusedAreaOffsetLow;
		uint32 CommittedBlocks = SmallPoolTables[PoolIndex].BlockOfBlockAllocationBits.CountOnes(SmallPoolTables[PoolIndex].NumEverUsedBlockOfBlocks);
		uint32 PartialBlocks = SmallPoolTables[PoolIndex].NumEverUsedBlockOfBlocks - SmallPoolTables[PoolIndex].BlockOfBlockIsExhausted.CountOnes(SmallPoolTables[PoolIndex].NumEverUsedBlockOfBlocks);
		uint32 FullBlocks = CommittedBlocks - PartialBlocks;
		int64 ComittedVM = VM - (SmallPoolTables[PoolIndex].NumEverUsedBlockOfBlocks - CommittedBlocks) * SmallPoolTables[PoolIndex].PagesPlatformForBlockOfBlocks * OsAllocationGranularity;

		int64 AveSize = SmallPoolTables[PoolIndex].TotalAllocCount.Load() ? SmallPoolTables[PoolIndex].TotalRequestedAllocSize.Load() / SmallPoolTables[PoolIndex].TotalAllocCount.Load() : 0;
		int64 EstPadWaste = ((SmallPoolTables[PoolIndex].TotalAllocCount.Load() - SmallPoolTables[PoolIndex].TotalFreeCount.Load()) * (PoolIndexToBlockSize(PoolIndex) - AveSize));

		Ar.Logf(TEXT("Pool %2d   Size %6d   Allocs %8lld  Frees %8lld  AveAllocSize %6d  EstPadWaste %4dKB  UsedVM %3dMB  CommittedVM %3dMB  HighSlabs %6d  CommittedSlabs %6d  FullSlabs %6d  PartialSlabs  %6d"), 
			PoolIndex,
			PoolIndexToBlockSize(PoolIndex),
			SmallPoolTables[PoolIndex].TotalAllocCount.Load(),
			SmallPoolTables[PoolIndex].TotalFreeCount.Load(),
			AveSize,
			EstPadWaste / 1024,
			VM / (1024 * 1024),
			ComittedVM / (1024 * 1024),
			SmallPoolTables[PoolIndex].NumEverUsedBlockOfBlocks,
			CommittedBlocks,
			FullBlocks,
			PartialBlocks
			);
	}
#else
#endif

#else
	Ar.Logf(TEXT("Allocator Stats for Binned3 are not in this build set BINNED3_ALLOCATOR_STATS 1 in MallocBinned3.cpp"));
#endif
}
#if !BINNED3_INLINE
	#if PLATFORM_USES_FIXED_GMalloc_CLASS && !FORCE_ANSI_ALLOCATOR && USE_MALLOC_BINNED3
		//#define FMEMORY_INLINE_FUNCTION_DECORATOR  FORCEINLINE
		#define FMEMORY_INLINE_GMalloc (FMallocBinned3::MallocBinned3)
		#include "FMemory.inl"
	#endif
#endif
#endif

PRAGMA_RESTORE_UNSAFE_TYPECAST_WARNINGS

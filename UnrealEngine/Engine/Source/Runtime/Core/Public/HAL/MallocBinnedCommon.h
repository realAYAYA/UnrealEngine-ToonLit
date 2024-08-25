// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/MemoryBase.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/PlatformTLS.h"
#include "Templates/AlignmentTemplates.h"

#if PLATFORM_HAS_FPlatformVirtualMemoryBlock

#include "Containers/Array.h"
#include "HAL/PlatformMemory.h"
#include "Templates/Function.h"

#define BINNEDCOMMON_MAX_LISTED_SMALL_POOL_SIZE	28672
#define BINNEDCOMMON_NUM_LISTED_SMALL_POOLS	49

#if !defined(BINNEDCOMMON_USE_SEPARATE_VM_PER_POOL)
	#if PLATFORM_WINDOWS
		#define BINNEDCOMMON_USE_SEPARATE_VM_PER_POOL (1)
	#else
		#define BINNEDCOMMON_USE_SEPARATE_VM_PER_POOL (0)
	#endif
#endif


class FBitTree
{
	uint64* Bits; // one bits in middle layers mean "all allocated"
	uint32 Capacity; // rounded up to a power of two
	uint32 DesiredCapacity;
	uint32 Rows;
	uint32 OffsetOfLastRow;
	uint32 AllocationSize;

public:
	FBitTree()
		: Bits(nullptr)
	{
	}

	static constexpr uint32 GetMemoryRequirements(uint32 NumPages)
	{
		uint32 AllocationSize = 8;
		uint32 RowsUint64s = 1;
		uint32 Capacity = 64;
		uint32 OffsetOfLastRow = 0;

		while (Capacity < NumPages)
		{
			Capacity *= 64;
			RowsUint64s *= 64;
			OffsetOfLastRow = AllocationSize / 8;
			AllocationSize += 8 * RowsUint64s;
		}

		uint32 LastRowTotal = (AllocationSize - OffsetOfLastRow * 8) * 8;
		uint32 ExtraBits = LastRowTotal - NumPages;
		AllocationSize -= (ExtraBits / 64) * 8;
		return AllocationSize;
	}

	void FBitTreeInit(uint32 InDesiredCapacity, void * Memory, uint32 MemorySize, bool InitialValue);
	uint32 AllocBit();
	bool IsAllocated(uint32 Index) const;
	void AllocBit(uint32 Index);
	uint32 NextAllocBit() const;
	uint32 NextAllocBit(uint32 StartIndex) const;
	void FreeBit(uint32 Index);
	uint32 CountOnes(uint32 UpTo) const;
};

struct FSizeTableEntry
{
	uint32 BlockSize;
	uint16 BlocksPerBlockOfBlocks;
	uint8 PagesPlatformForBlockOfBlocks;

	FSizeTableEntry()
	{
	}

	FSizeTableEntry(uint32 InBlockSize, uint64 PlatformPageSize, uint8 Pages4k, uint32 BasePageSize, uint32 MinimumAlignment);

	bool operator<(const FSizeTableEntry& Other) const
	{
		return BlockSize < Other.BlockSize;
	}
	static uint8 FillSizeTable(uint64 PlatformPageSize, FSizeTableEntry* SizeTable, uint32 BasePageSize, uint32 MinimumAlignment, uint32 MaxSize, uint32 SizeIncrement);
};

struct FArenaParams
{
	// these are parameters you set
	uint64 AddressLimit = 1024 * 1024 * 1024; // this controls the size of the root hash table
	uint32 BasePageSize = 4096; // this is used to make sensible calls to malloc and figures into the standard pool sizes if bUseStandardSmallPoolSizes is true
	uint32 AllocationGranularity = 4096; // this is the granularity of the commit and decommit calls used on the VM slabs
	uint32 MaxSizePerBundle = 8192;
	uint32 MaxStandardPoolSize = 128 * 1024; // these are added to the standard pool sizes, mainly to use the TLS caches, they are typically one block per slab
	uint16 MaxBlocksPerBundle = 64;
	uint8 MaxMemoryPerBlockSizeShift = 29;
	uint8 EmptyCacheAllocExtra = 32;
	uint8 MaxGlobalBundles = 32;
	uint8 MinimumAlignmentShift = 4;
	uint8 PoolCount;
	bool bUseSeparateVMPerPool = !!(BINNEDCOMMON_USE_SEPARATE_VM_PER_POOL);
	bool bPerThreadCaches = true;
	bool bUseStandardSmallPoolSizes = true;
	bool bAttemptToAlignSmallBocks = true;
	TArray<uint32> AdditionalBlockSizes;

	// This lambdas is similar to the platform virtual memory HAL and by default just call that. 
	TFunction<FPlatformMemory::FPlatformVirtualMemoryBlock(SIZE_T)> ReserveVM;

	// These allow you to override the large block allocator. The value add here is that MBA tracks the metadata for you and call tell the difference between a large block pointer and a small block pointer.
	// By defaults these just use the platform VM interface to allocate some committed memory
	TFunction<void*(SIZE_T, SIZE_T, SIZE_T&, uint32&)> LargeBlockAlloc;
	TFunction<void(void*, uint32)> LargeBlockFree;


	// these are parameters are derived from other parameters
	uint64 MaxMemoryPerBlockSize;
	uint32 MaxPoolSize;
	uint32 MinimumAlignment;
	uint32 MaximumAlignmentForSmallBlock;

};

#endif	//~PLATFORM_HAS_FPlatformVirtualMemoryBlock


#if !defined(AGGRESSIVE_MEMORY_SAVING)
#	error "AGGRESSIVE_MEMORY_SAVING must be defined"
#endif

#if AGGRESSIVE_MEMORY_SAVING
#	define DEFAULT_GMallocBinnedBundleSize 8192
#else
#	define DEFAULT_GMallocBinnedBundleSize 65536
#endif

#define DEFAULT_GMallocBinnedBundleCount 64

#define UE_BINNEDCOMMON_ALLOW_RUNTIME_TWEAKING 0
#if UE_BINNEDCOMMON_ALLOW_RUNTIME_TWEAKING
extern CORE_API int32 GMallocBinnedBundleSize;
extern CORE_API int32 GMallocBinnedBundleCount;
#else
#	define GMallocBinnedBundleSize	DEFAULT_GMallocBinnedBundleSize
#	define GMallocBinnedBundleCount	DEFAULT_GMallocBinnedBundleCount
#endif

#ifndef UE_BINNEDCOMMON_ALLOCATOR_STATS
#	define UE_BINNEDCOMMON_ALLOCATOR_STATS (!UE_BUILD_SHIPPING || WITH_EDITOR)
#endif


class FMallocBinnedCommonBase : public FMalloc
{
protected:
	struct FPtrToPoolMapping
	{
		FPtrToPoolMapping()
			: PtrToPoolPageBitShift(0)
			, HashKeyShift(0)
			, PoolMask(0)
			, MaxHashBuckets(0)
			, AddressSpaceBase(0)
		{
		}
		explicit FPtrToPoolMapping(uint32 InPageSize, uint64 InNumPoolsPerPage, uint64 AddressBase, uint64 AddressLimit)
		{
			Init(InPageSize, InNumPoolsPerPage, AddressBase, AddressLimit);
		}

		void Init(uint32 InPageSize, uint64 InNumPoolsPerPage, uint64 AddressBase, uint64 AddressLimit)
		{
			uint64 PoolPageToPoolBitShift = FPlatformMath::CeilLogTwo64(InNumPoolsPerPage);

			PtrToPoolPageBitShift = FPlatformMath::CeilLogTwo(InPageSize);
			HashKeyShift = PtrToPoolPageBitShift + PoolPageToPoolBitShift;
			PoolMask = (1ull << PoolPageToPoolBitShift) - 1;
			MaxHashBuckets = FMath::RoundUpToPowerOfTwo64(AddressLimit - AddressBase) >> HashKeyShift;
			AddressSpaceBase = AddressBase;
		}

		FORCEINLINE void GetHashBucketAndPoolIndices(const void* InPtr, uint32& OutBucketIndex, UPTRINT& OutBucketCollision, uint32& OutPoolIndex) const
		{
			check((UPTRINT)InPtr >= AddressSpaceBase);
			const UPTRINT Ptr = (UPTRINT)InPtr - AddressSpaceBase;
			OutBucketCollision = Ptr >> HashKeyShift;
			OutBucketIndex = uint32(OutBucketCollision & (MaxHashBuckets - 1));
			OutPoolIndex = uint32((Ptr >> PtrToPoolPageBitShift) & PoolMask);
		}

		FORCEINLINE uint64 GetMaxHashBuckets() const
		{
			return MaxHashBuckets;
		}

	private:
		/** Shift to apply to a pointer to get the reference from the indirect tables */
		uint64 PtrToPoolPageBitShift;

		/** Shift required to get required hash table key. */
		uint64 HashKeyShift;

		/** Used to mask off the bits that have been used to lookup the indirect table */
		uint64 PoolMask;

		// PageSize dependent constants
		uint64 MaxHashBuckets;

		// Base address for any virtual allocations. Can be non 0 on some platforms
		uint64 AddressSpaceBase;
	};

	// This needs to be small enough to fit inside the smallest allocation handled by MallocBinned2\3, hence the union.
	struct FBundleNode
	{
		FBundleNode* NextNodeInCurrentBundle;

		// NextBundle ptr is valid when node is stored in FFreeBlockList in a thread-local list of reusable allocations.
		// Count is valid when node is stored in global recycler and caches the number of nodes in the list formed by NextNodeInCurrentBundle.
		union
		{
			FBundleNode* NextBundle;
			int32 Count;
		};
	};

	struct FBundle
	{
		FORCEINLINE FBundle()
		{
			Reset();
		}

		FORCEINLINE void Reset()
		{
			Head = nullptr;
			Count = 0;
		}

		FORCEINLINE void PushHead(FBundleNode* Node)
		{
			Node->NextNodeInCurrentBundle = Head;
			Node->NextBundle = nullptr;
			Head = Node;
			Count++;
		}

		FORCEINLINE FBundleNode* PopHead()
		{
			FBundleNode* Result = Head;

			Count--;
			Head = Head->NextNodeInCurrentBundle;
			return Result;
		}

		FBundleNode* Head;
		uint32       Count;
	};

	/** Hash table struct for retrieving allocation book keeping information */
	template <class T>
	struct TPoolHashBucket
	{
		UPTRINT         BucketIndex;
		T* FirstPool;
		TPoolHashBucket* Prev;
		TPoolHashBucket* Next;

		TPoolHashBucket()
		{
			BucketIndex = 0;
			FirstPool = nullptr;
			Prev = this;
			Next = this;
		}

		void Link(TPoolHashBucket* After)
		{
			After->Prev = Prev;
			After->Next = this;
			Prev->Next = After;
			this->Prev = After;
		}

		void Unlink()
		{
			Next->Prev = Prev;
			Prev->Next = Next;
			Prev = this;
			Next = this;
		}
	};

	FPtrToPoolMapping PtrToPoolMapping;
	static CORE_API uint32 BinnedTlsSlot;

#if UE_BINNEDCOMMON_ALLOCATOR_STATS
	static std::atomic<int64> TLSMemory;
	static std::atomic<int64> ConsolidatedMemory;
#endif
};

template <class AllocType, int MinAlign, int MaxAlign, int MinAlignShift, int NumSmallPools, int MaxSmallPoolSize>
class TMallocBinnedCommon : public FMallocBinnedCommonBase
{
	static_assert(sizeof(FBundleNode) <= MinAlign, "Bundle nodes must fit into the smallest block size");

protected:
	struct FFreeBlockList
	{
		// return true if we actually pushed it
		FORCEINLINE bool PushToFront(void* InPtr, uint32 InPoolIndex, uint32 InBlockSize)
		{
			checkSlow(InPtr);

			if ((PartialBundle.Count >= (uint32)GMallocBinnedBundleCount) | (PartialBundle.Count * InBlockSize >= (uint32)GMallocBinnedBundleSize))
			{
				if (FullBundle.Head)
				{
					return false;
				}
				FullBundle = PartialBundle;
				PartialBundle.Reset();
			}
			PartialBundle.PushHead((FBundleNode*)InPtr);
			return true;
		}

		FORCEINLINE bool CanPushToFront(uint32 InPoolIndex, uint32 InBlockSize) const
		{
			return !((!!FullBundle.Head) & ((PartialBundle.Count >= (uint32)GMallocBinnedBundleCount) | (PartialBundle.Count * InBlockSize >= (uint32)GMallocBinnedBundleSize)));
		}

		FORCEINLINE void* PopFromFront(uint32 InPoolIndex)
		{
			if ((!PartialBundle.Head) & (!!FullBundle.Head))
			{
				PartialBundle = FullBundle;
				FullBundle.Reset();
			}
			return PartialBundle.Head ? PartialBundle.PopHead() : nullptr;
		}

		// tries to recycle the full bundle, if that fails, it is returned for freeing
		template <class T>
		FBundleNode* RecyleFull(uint32 InPoolIndex, T& InGlobalRecycler)
		{
			FBundleNode* Result = nullptr;
			if (FullBundle.Head)
			{
				FullBundle.Head->Count = FullBundle.Count;
				if (!InGlobalRecycler.PushBundle(InPoolIndex, FullBundle.Head))
				{
					Result = FullBundle.Head;
					Result->NextBundle = nullptr;
				}
				FullBundle.Reset();
			}
			return Result;
		}

		template <class T>
		bool ObtainPartial(uint32 InPoolIndex, T& InGlobalRecycler)
		{
			if (!PartialBundle.Head)
			{
				PartialBundle.Count = 0;
				PartialBundle.Head = InGlobalRecycler.PopBundle(InPoolIndex);
				if (PartialBundle.Head)
				{
					PartialBundle.Count = PartialBundle.Head->Count;
					PartialBundle.Head->NextBundle = nullptr;
					return true;
				}
				return false;
			}
			return true;
		}

		FBundleNode* PopBundles(uint32 InPoolIndex)
		{
			FBundleNode* Partial = PartialBundle.Head;
			if (Partial)
			{
				PartialBundle.Reset();
				Partial->NextBundle = nullptr;
			}

			FBundleNode* Full = FullBundle.Head;
			if (Full)
			{
				FullBundle.Reset();
				Full->NextBundle = nullptr;
			}

			FBundleNode* Result = Partial;
			if (Result)
			{
				Result->NextBundle = Full;
			}
			else
			{
				Result = Full;
			}

			return Result;
		}

	private:
		FBundle PartialBundle;
		FBundle FullBundle;
	};

	struct FPerThreadFreeBlockLists
	{
		FORCEINLINE static FPerThreadFreeBlockLists* Get()
		{
			return FPlatformTLS::IsValidTlsSlot(BinnedTlsSlot) ? (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(BinnedTlsSlot) : nullptr;
		}

		static void SetTLS()
		{
			check(FPlatformTLS::IsValidTlsSlot(BinnedTlsSlot));
			FPerThreadFreeBlockLists* ThreadSingleton = (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(BinnedTlsSlot);
			if (!ThreadSingleton)
			{
				const int64 TLSSize = Align(sizeof(FPerThreadFreeBlockLists), AllocType::OsAllocationGranularity);
				ThreadSingleton = new (AllocType::AllocateMetaDataMemory(TLSSize)) FPerThreadFreeBlockLists();
#if UE_BINNEDCOMMON_ALLOCATOR_STATS
				TLSMemory.fetch_add(TLSSize, std::memory_order_relaxed);
#endif
				verify(ThreadSingleton);
				FPlatformTLS::SetTlsValue(BinnedTlsSlot, ThreadSingleton);
				AllocType::RegisterThreadFreeBlockLists(ThreadSingleton);
			}
		}

		static void ClearTLS()
		{
			check(FPlatformTLS::IsValidTlsSlot(BinnedTlsSlot));
			FPerThreadFreeBlockLists* ThreadSingleton = (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(BinnedTlsSlot);
			if (ThreadSingleton)
			{
				const int64 TLSSize = Align(sizeof(FPerThreadFreeBlockLists), AllocType::OsAllocationGranularity);
#if UE_BINNEDCOMMON_ALLOCATOR_STATS
				TLSMemory.fetch_sub(TLSSize, std::memory_order_relaxed);
#endif
				AllocType::UnregisterThreadFreeBlockLists(ThreadSingleton);

				ThreadSingleton->~FPerThreadFreeBlockLists();

				AllocType::FreeMetaDataMemory(ThreadSingleton, TLSSize);
			}
			FPlatformTLS::SetTlsValue(BinnedTlsSlot, nullptr);
		}

		FORCEINLINE void* Malloc(uint32 InPoolIndex)
		{
			return FreeLists[InPoolIndex].PopFromFront(InPoolIndex);
		}

		// return true if the pointer was pushed
		FORCEINLINE bool Free(void* InPtr, uint32 InPoolIndex, uint32 InBlockSize)
		{
			return FreeLists[InPoolIndex].PushToFront(InPtr, InPoolIndex, InBlockSize);
		}

		// return true if a pointer can be pushed
		FORCEINLINE bool CanFree(uint32 InPoolIndex, uint32 InBlockSize) const
		{
			return FreeLists[InPoolIndex].CanPushToFront(InPoolIndex, InBlockSize);
		}

		// returns a bundle that needs to be freed if it can't be recycled
		template <class T>
		FBundleNode* RecycleFullBundle(uint32 InPoolIndex, T& InGlobalRecycler)
		{
			return FreeLists[InPoolIndex].RecyleFull(InPoolIndex, InGlobalRecycler);
		}

		// returns true if we have anything to pop
		template <class T>
		bool ObtainRecycledPartial(uint32 InPoolIndex, T& InGlobalRecycler)
		{
			return FreeLists[InPoolIndex].ObtainPartial(InPoolIndex, InGlobalRecycler);
		}

		FBundleNode* PopBundles(uint32 InPoolIndex)
		{
			return FreeLists[InPoolIndex].PopBundles(InPoolIndex);
		}

#if UE_BINNEDCOMMON_ALLOCATOR_STATS
	public:
		int64 AllocatedMemory = 0;
#endif
	private:
		FFreeBlockList FreeLists[NumSmallPools];
	};

	FORCEINLINE SIZE_T QuantizeSizeCommon(SIZE_T Count, uint32 Alignment, const AllocType& Alloc) const
	{
		static_assert(DEFAULT_ALIGNMENT <= MinAlign, "DEFAULT_ALIGNMENT is assumed to be zero"); // used below
		checkSlow((Alignment & (Alignment - 1)) == 0); // Check the alignment is a power of two
		SIZE_T SizeOut;
		if ((Count <= MaxSmallPoolSize) & (Alignment <= MinAlign)) // one branch, not two
		{
			SizeOut = Alloc.PoolIndexToBlockSize(BoundSizeToPoolIndex(Count, Alloc.MemSizeToIndex));
			check(SizeOut >= Count);
			return SizeOut;
		}
		Alignment = FMath::Max<uint32>(Alignment, MinAlign);
		Count = Align(Count, Alignment);
		if ((Count <= MaxSmallPoolSize) & (Alignment <= MaxAlign))
		{
			uint32 PoolIndex = BoundSizeToPoolIndex(Count, Alloc.MemSizeToIndex);
			do
			{
				uint32 BlockSize = Alloc.PoolIndexToBlockSize(PoolIndex);
				if (IsAligned(BlockSize, Alignment))
				{
					SizeOut = SIZE_T(BlockSize);
					check(SizeOut >= Count);
					return SizeOut;
				}

				PoolIndex++;
			} while (PoolIndex < NumSmallPools);
		}

		Alignment = FPlatformMath::Max<uint32>(Alignment, Alloc.OsAllocationGranularity);
		SizeOut = Align(Count, Alignment);
		check(SizeOut >= Count);
		return SizeOut;
	}

	FORCEINLINE uint32 BoundSizeToPoolIndex(SIZE_T Size, const uint8(&MemSizeToIndex)[1 + (MaxSmallPoolSize >> MinAlignShift)]) const
	{
		auto Index = ((Size + MinAlign - 1) >> MinAlignShift);
		checkSlow(Index >= 0 && Index <= (MaxSmallPoolSize >> MinAlignShift)); // and it should be in the table
		uint32 PoolIndex = uint32(MemSizeToIndex[Index]);
		checkSlow(PoolIndex >= 0 && PoolIndex < NumSmallPools);
		return PoolIndex;
	}

	bool PromoteToLargerBin(SIZE_T& Size, uint32& Alignment, const AllocType& Alloc) const
	{
		// try to promote our allocation request to a larger bin with a matching natural alignment
		// if requested alignment is larger than MinAlign but smaller than MaxAlign
		// so we don't do a page allocation with a lot of memory waste
		Alignment = FMath::Max<uint32>(Alignment, MinAlign);
		const SIZE_T AlignedSize = Align(Size, Alignment);
		if (UNLIKELY((AlignedSize <= MaxSmallPoolSize) && (Alignment <= MaxAlign)))
		{
			uint32 PoolIndex = BoundSizeToPoolIndex(AlignedSize, Alloc.MemSizeToIndex);
			do
			{
				uint32 BlockSize = Alloc.PoolIndexToBlockSize(PoolIndex);
				if (IsAligned(BlockSize, Alignment))
				{
					// we found a matching pool for our alignment and size requirements, so modify the size request to match
					Size = SIZE_T(BlockSize);
					Alignment = MinAlign;
					return true;
				}

				PoolIndex++;
			} while (PoolIndex < NumSmallPools);
		}

		return false;
	}
};
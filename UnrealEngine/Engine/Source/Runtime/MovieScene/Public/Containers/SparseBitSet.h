// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/Array.h"

namespace UE::MovieScene
{

template<typename T, int32 InlineSize>
struct TDynamicSparseBitSetBucketStorage;

template<typename T>
struct TFixedSparseBitSetBucketStorage;

template<typename HashType, typename BucketStorage = TDynamicSparseBitSetBucketStorage<uint8, 4>>
struct TSparseBitSet;


enum class ESparseBitSetBitResult
{
	NewlySet,
	AlreadySet,
};

namespace Private
{
	static uint32 CountTrailingZeros(uint8 In)
	{
		const uint32 X = 0xFFFFFF00 | uint32(In);
		return FMath::CountTrailingZeros(X);
	}
	static uint32 CountTrailingZeros(uint16 In)
	{
		const uint32 X = 0xFFFF0000 | uint32(In);
		return FMath::CountTrailingZeros(X);
	}
	static uint32 CountTrailingZeros(uint32 In)
	{
		return FMath::CountTrailingZeros(In);
	}
	static uint32 CountTrailingZeros(uint64 In)
	{
		return static_cast<uint32>(FMath::CountTrailingZeros64(In));
	}

	/** Return a bitmask of all the bits less-than BitOffset */
	template<typename T, typename U>
	static T BitOffsetToLowBitMask(U BitOffset)
	{
		constexpr T One(1);
		const T Index = static_cast<T>(BitOffset);
		return (One << Index)-One;
	}

	/** Return a bitmask of all the bits greater-than-or-equal to BitOffset */
	template<typename T, typename U>
	static T BitOffsetToHighBitMask(U BitOffset)
	{
		return ~BitOffsetToLowBitMask<T>(BitOffset);
	}
}

/**
 * NOTE: This class is currently considered internal only, and should only be used by engine code.
 * A sparse bitset comprising a hash of integer indexes with set bits, and a sparse array of unsigned integers (referred to as buckets) whose width is defined by the storage.
 *
 * The maximum size bitfield that is representible by this bitset is defined as sizeof(HashType)*sizeof(BucketStorage::BucketType). For example, a 64 bit hash with 32 bit buckets
 * can represent a bitfield of upto 2048 bits.
 *
 * The hash allows for empty buckets to be completely omitted from memory, and affords very fast comparison for buckets that have no set bits.
 * This container is specialized for relatively large bitfields that have small numbers of set bits (ie, needles in haystacks) as they will provide the best memory vs CPU tradeoffs.
 */
template<typename HashType, typename BucketStorage>
struct TSparseBitSet
{
	using BucketType = typename BucketStorage::BucketType;
	static constexpr uint32 HashSize    = sizeof(HashType)*8;
	static constexpr uint32 BucketSize  = sizeof(typename BucketStorage::BucketType)*8;
	static constexpr uint32 MaxNumBits  = HashSize * BucketSize;

	explicit TSparseBitSet()
		: BucketHash(0)
	{}

	template<typename ...StorageArgs>
	explicit TSparseBitSet(StorageArgs&& ...Storage)
		: Buckets(Forward<StorageArgs>(Storage)...)
		, BucketHash(0)
	{}

	TSparseBitSet(const TSparseBitSet&) = default;
	TSparseBitSet& operator=(const TSparseBitSet&) = default;

	TSparseBitSet(TSparseBitSet&&) = default;
	TSparseBitSet& operator=(TSparseBitSet&&) = default;

	template<typename OtherHashType, typename OtherStorageType>
	void CopyTo(TSparseBitSet<OtherHashType, OtherStorageType>& Other) const
	{
		static_assert(TSparseBitSet<OtherHashType, OtherStorageType>::BucketSize == BucketSize, "Cannot copy sparse bitsets of different bucket sizes");

		// Copy the buckets
		const uint32 NumBuckets = FMath::CountBits(Other.BucketHash);
		Other.Buckets.SetNum(NumBuckets);
		CopyToUnsafe(Other, NumBuckets);
	}

	/** Copy this bitset to another without resizing the destination's bucket storage. Bucket storage must be >= this size. */
	template<typename OtherHashType, typename OtherStorageType>
	void CopyToUnsafe(TSparseBitSet<OtherHashType, OtherStorageType>& Other, uint32 OtherBucketCapacity) const
	{
		static_assert(TSparseBitSet<OtherHashType, OtherStorageType>::BucketSize == BucketSize, "Cannot copy sparse bitsets of different bucket sizes");

		const uint32 ThisNumBuckets = this->NumBuckets();
		checkf(OtherBucketCapacity >= ThisNumBuckets, TEXT("Attempting to copy a sparse bitset without enough capacity in the destination (%d, required %d)"), OtherBucketCapacity, ThisNumBuckets);

		// Copy the hash
		Other.BucketHash = this->BucketHash;

		// Copy the buckets
		FMemory::Memcpy(Other.Buckets.GetData(), this->Buckets.Storage.GetData(), sizeof(typename BucketStorage::BucketType)*ThisNumBuckets);
	}

	TSparseBitSet& operator|=(const TSparseBitSet& Other)
	{
		using namespace Private;

		HashType One(1u);
		HashType NewHash   = Other.BucketHash | BucketHash;
		HashType OtherHash = Other.BucketHash;

		uint32 OtherBucketIndex    = 0;
		uint32 OtherBucketBitIndex = Private::CountTrailingZeros(OtherHash);

		while (OtherBucketBitIndex < HashSize)
		{
			const HashType HashBit = HashType(1) << OtherBucketBitIndex;
			const uint32 ThisBucketIndex = FMath::CountBits(BucketHash & (HashBit-1));

			if ((BucketHash & HashBit) == 0)
			{
				Buckets.Insert(Other.Buckets.Get(OtherBucketIndex), ThisBucketIndex);
			}
			else
			{
				Buckets.Get(ThisBucketIndex) |= Other.Buckets.Get(OtherBucketIndex);
			}
			
			BucketHash |= HashBit;

			++OtherBucketIndex;

			// Mask out this bit and find the index of the next one
			OtherHash &= ~(One << OtherBucketBitIndex);
			OtherBucketBitIndex = Private::CountTrailingZeros(OtherHash);
		}

		return *this;
	}

	/**
	 * Count the number of buckets in this bitset
	 */
	uint32 NumBuckets() const
	{
		return FMath::CountBits(this->BucketHash);
	}

	uint32 CountSetBits() const
	{
		uint32 Total = 0;
		const uint32 TotalNumBuckets = NumBuckets();
		for (uint32 Index = 0; Index < TotalNumBuckets; ++Index)
		{
			Total += FMath::CountBits(Buckets.Get(Index));
		}
		return Total;
	}

	uint32 GetMaxNumBits() const
	{
		return MaxNumBits;
	}

	bool IsEmpty() const
	{
		return this->BucketHash == 0;
	}

	/**
	 * Set the bit at the specified index.
	 * Any bits between Num and BitIndex will be considered 0.
	 *
	 * @return true if the bit was previously considered 0 and is now set, false if it was already set.
	 */
	ESparseBitSetBitResult SetBit(uint32 BitIndex)
	{
		CheckIndex(BitIndex);

		FBitOffsets Offsets(BucketHash, BitIndex);

		// Do we need to add a new bucket?
		if ( (BucketHash & Offsets.HashBit) == 0)
		{
			BucketHash |= Offsets.HashBit;
			Buckets.Insert(Offsets.BitMaskWithinBucket, Offsets.BucketIndex);
			return ESparseBitSetBitResult::NewlySet;
		}
		else if ((Buckets.Get(Offsets.BucketIndex) & Offsets.BitMaskWithinBucket) == 0)
		{
			Buckets.Get(Offsets.BucketIndex) |= Offsets.BitMaskWithinBucket;
			return ESparseBitSetBitResult::NewlySet;
		}

		return ESparseBitSetBitResult::AlreadySet;
	}

	/**
	 * Check whether the specified bit index is set
	 */
	bool IsBitSet(uint32 BitIndex) const
	{
		CheckIndex(BitIndex);

		const uint32   Hash    = BitIndex / BucketSize;
		const HashType HashBit = (HashType(1) << Hash);
		if (BucketHash & HashBit)
		{
			const uint32     BucketIndex  = FMath::CountBits(BucketHash & (HashBit-1));
			const uint32     ThisBitIndex = (BitIndex-BucketSize*Hash);
			const BucketType ThisBitMask  = BucketType(1u) << ThisBitIndex;

			return Buckets.Get(BucketIndex) & ThisBitMask;
		}
		return false;
	}

	/**
	 * Get the sparse bucket index of the specified bit
	 */
	int32 GetSparseBucketIndex(uint32 BitIndex) const
	{
		CheckIndex(BitIndex);

		const uint32   Hash    = BitIndex / BucketSize;
		const HashType HashBit = (HashType(1) << Hash);
		if (BucketHash & HashBit)
		{
			uint32 BucketIndex = FMath::CountBits(BucketHash & (HashBit-1));

			const uint32 ThisBitIndex = (BitIndex-BucketSize*Hash);
			const BucketType ThisBitMask = static_cast<BucketType>(BucketType(1u) << ThisBitIndex);

			BucketType ThisBucket = Buckets.Get(BucketIndex);
			if (ThisBucket & ThisBitMask)
			{
				// Compute the offset
				int32 SparseIndex = FMath::CountBits(ThisBucket & (ThisBitMask-1));

				// Count all the preceding buckets to find the final sparse index
				while (BucketIndex > 0)
				{
					--BucketIndex;
					SparseIndex += FMath::CountBits(Buckets.Get(BucketIndex));
				}
				return SparseIndex;
			}
		}
		return INDEX_NONE;
	}

	struct FIterator
	{
		static FIterator Begin(const TSparseBitSet<HashType, BucketStorage>* InBitSet)
		{
			FIterator It;
			It.BitSet = InBitSet;
			It.CurrentBucket = 0;

			if (InBitSet->BucketHash != 0)
			{
				It.BucketBitIndex = Private::CountTrailingZeros(InBitSet->BucketHash);
				It.CurrentBucket = InBitSet->Buckets.Get(0);
				It.IndexWithinBucket = Private::CountTrailingZeros(It.CurrentBucket);
			}
			else
			{
				It.BucketBitIndex = HashSize;
				It.IndexWithinBucket = 0;
			}
			return It;
		}
		static FIterator End(const TSparseBitSet<HashType, BucketStorage>* InBitSet)
		{
			FIterator It;
			It.BitSet = InBitSet;
			It.CurrentBucket = 0;
			It.BucketBitIndex = HashSize;
			It.IndexWithinBucket = 0;
			return It;
		}

		void operator++()
		{
			using namespace Private;

			CurrentBucket &= ~(BucketType(1u)<<IndexWithinBucket);
			IndexWithinBucket = CountTrailingZeros(CurrentBucket);

			if (IndexWithinBucket == BucketSize || CurrentBucket == 0)
			{
				// If this was the last bit, reset the iterator to end()
				if (BucketBitIndex == HashSize-1)
				{
					IndexWithinBucket = 0;
					BucketBitIndex = HashSize;
					return;
				}

				HashType UnvisitedBucketBitMask = BitOffsetToHighBitMask<HashType>(BucketBitIndex+1);
				BucketBitIndex = CountTrailingZeros(HashType(BitSet->BucketHash & UnvisitedBucketBitMask));

				// Check whether we're at the end
				if (BucketBitIndex == HashSize)
				{
					IndexWithinBucket = 0;
				}
				else
				{
					const uint8 NextBucketIndex = FMath::CountBits(BitSet->BucketHash & BitOffsetToLowBitMask<HashType>(BucketBitIndex));
					CurrentBucket = BitSet->Buckets.Get(NextBucketIndex);
					IndexWithinBucket = CountTrailingZeros(CurrentBucket);
				}
			}
		}

		int32 operator*() const
		{
			return BucketSize*BucketBitIndex + IndexWithinBucket;
		}

		explicit operator bool() const
		{
			return BucketBitIndex < HashSize;
		}

		friend bool operator==(FIterator A, FIterator B)
		{
			return A.BitSet == B.BitSet && A.BucketBitIndex == B.BucketBitIndex && A.IndexWithinBucket == B.IndexWithinBucket;
		}
		friend bool operator!=(FIterator A, FIterator B)
		{
			return !(A == B);
		}
private:
		FIterator() = default;

		const TSparseBitSet<HashType, BucketStorage>* BitSet;
		uint8 BucketBitIndex;
		uint8 IndexWithinBucket;

		BucketType CurrentBucket;
	};

	friend FIterator begin(const TSparseBitSet<HashType, BucketStorage>& In) { return FIterator::Begin(&In); }
	friend FIterator end(const TSparseBitSet<HashType, BucketStorage>& In)   { return FIterator::End(&In); }

private:

	template<typename, typename>
	friend struct TSparseBitSet;

	FORCEINLINE void CheckIndex(uint32 BitIndex) const
	{
		checkfSlow(BitIndex < MaxNumBits, TEXT("Invalid index (%d) specified for a sparse bitset of maximum size (%d)"), BitIndex, MaxNumBits);
	}

	struct FBitOffsets
	{
		HashType HashBit;
		BucketType BitMaskWithinBucket;
		int32  BucketIndex;
		FBitOffsets(HashType InBucketHash, uint32 BitIndex)
		{
			const HashType Hash(BitIndex / BucketSize);
			HashBit = HashType(1) << Hash;

			BucketIndex = FMath::CountBits(InBucketHash & (HashBit-1u));

			const uint32 ThisBitIndex = (BitIndex-BucketSize*Hash);
			BitMaskWithinBucket = BucketType(1u) << ThisBitIndex;
		}
	};

	BucketStorage Buckets;
	HashType      BucketHash;
};



template<typename T, int32 InlineSize = 8>
struct TDynamicSparseBitSetBucketStorage
{
	using BucketType = T;

	TArray<BucketType, TInlineAllocator<InlineSize>> Storage;

	void Insert(BucketType InitialValue, int32 Index)
	{
		Storage.Insert(InitialValue, Index);
	}

	BucketType* GetData()              { return Storage.GetData(); }

	BucketType& Get(int32 Index)       { return Storage[Index]; }
	BucketType  Get(int32 Index) const { return Storage[Index]; }
};

template<typename T>
struct TDynamicSparseBitSetBucketStorage<T, 0>
{
	using BucketType = T;

	TArray<BucketType> Storage;

	void Insert(BucketType InitialValue, int32 Index)
	{
		Storage.Insert(InitialValue, Index);
	}

	BucketType* GetData()              { return Storage.GetData(); }

	BucketType& Get(int32 Index)       { return Storage[Index]; }
	BucketType  Get(int32 Index) const { return Storage[Index]; }
};

template<typename T>
struct TFixedSparseBitSetBucketStorage
{
	using BucketType = T;

	explicit TFixedSparseBitSetBucketStorage()
		: Storage(nullptr)
	{}

	explicit TFixedSparseBitSetBucketStorage(BucketType* StoragePtr)
		: Storage(StoragePtr)
	{}

	TFixedSparseBitSetBucketStorage(const TFixedSparseBitSetBucketStorage&) = delete;
	void operator=(const TFixedSparseBitSetBucketStorage&) = delete;

	TFixedSparseBitSetBucketStorage(TFixedSparseBitSetBucketStorage&&) = delete;
	void operator=(TFixedSparseBitSetBucketStorage&&) = delete;

	BucketType* Storage;

	BucketType* GetData()              { return Storage; }

	BucketType& Get(int32 Index)       { return Storage[Index]; }
	BucketType  Get(int32 Index) const { return Storage[Index]; }
};

} // namespace UE::MovieScene

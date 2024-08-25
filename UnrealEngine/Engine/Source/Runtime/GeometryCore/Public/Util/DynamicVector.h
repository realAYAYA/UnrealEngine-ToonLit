// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3cpp dvector

#pragma once

#include <CoreMinimal.h>
#include "Containers/StaticArray.h"
#include "Serialization/Archive.h"
#include <UObject/UE5MainStreamObjectVersion.h>
#include "VectorTypes.h"
#include "IndexTypes.h"
#include "Math/NumericLimits.h"

namespace UE
{
namespace Geometry
{

using namespace UE::Math;

/*
 * Blocked array with fixed, power-of-two sized blocks.
 *
 * Iterator functions suitable for use with range-based for are provided
 */
template <class Type>
class TDynamicVector
{
public:
	static constexpr uint32 MaxSize = MAX_uint32;

	TDynamicVector()
	{
		Blocks.Add(new BlockType());
	}

	TDynamicVector(const TDynamicVector& Copy) = default;
	TDynamicVector(TDynamicVector&& Moved)
	{
		Blocks = MoveTemp(Moved.Blocks);
		CurBlock = MoveTemp(Moved.CurBlock);
		CurBlockUsed = MoveTemp(Moved.CurBlockUsed);
		Moved.Blocks.Add(new BlockType());
		Moved.CurBlock = 0;
		Moved.CurBlockUsed = 0;
	}
	TDynamicVector& operator=(const TDynamicVector& Copy) = default;
	TDynamicVector& operator=(TDynamicVector&& Moved)
	{
		Blocks = MoveTemp(Moved.Blocks);
		CurBlock = MoveTemp(Moved.CurBlock);
		CurBlockUsed = MoveTemp(Moved.CurBlockUsed);
		Moved.Blocks.Add(new BlockType());
		Moved.CurBlock = 0;
		Moved.CurBlockUsed = 0;
		return *this;
	}

	TDynamicVector(const TArray<Type>& Array)
	{
		SetNum((unsigned int) Array.Num());
		for (int32 Idx = 0; Idx < Array.Num(); Idx++)
		{
			(*this)[(unsigned int) Idx] = Array[Idx];
		}
	}
	TDynamicVector(TArrayView<const Type> Array)
	{
		SetNum((unsigned int) Array.Num());
		for (int32 Idx = 0; Idx < Array.Num(); Idx++)
		{
			(*this)[(unsigned int) Idx] = Array[Idx];
		}
	}

	inline void Clear();
	inline void Fill(const Type& Value);
	inline void Resize(unsigned int Count);
	inline void Resize(unsigned int Count, const Type& InitValue);
	/// Resize if Num() is less than Count; returns true if resize occurred
	inline bool SetMinimumSize(unsigned int Count, const Type& InitValue);
	inline void SetNum(unsigned int Count) { Resize(Count); }

	inline bool IsEmpty() const { return CurBlock == 0 && CurBlockUsed == 0; }
	inline size_t GetLength() const { return CurBlock * BlockSize + CurBlockUsed; }
	inline size_t Num() const { return GetLength(); }
	static int GetBlockSize() { return BlockSize; }
	inline size_t GetByteCount() const { return Blocks.Num() * BlockSize * sizeof(Type); }

	inline void Add(const Type& Data);
	inline void Add(const TDynamicVector& Data);
	inline void PopBack();

	inline void InsertAt(const Type& Data, unsigned int Index);
	inline void InsertAt(const Type& Data, unsigned int Index, const Type& InitValue);
	inline Type& ElementAt(unsigned int Index, Type InitialValue = Type{});

	inline const Type& Front() const
	{
		checkSlow(CurBlockUsed > 0);
		return Blocks[0][0];
	}

	inline const Type& Back() const
	{
		checkSlow(CurBlockUsed > 0);
		return Blocks[CurBlock][CurBlockUsed - 1];
	}


#if USING_ADDRESS_SANITISER
	FORCENOINLINE Type& operator[](unsigned int Index)
	{
		checkSlow(Index < Num());

		return Blocks[Index >> nShiftBits][Index & BlockIndexBitmask];
	}
	FORCENOINLINE const Type& operator[](unsigned int Index) const
	{
		checkSlow(Index < Num());

		return Blocks[Index >> nShiftBits][Index & BlockIndexBitmask];
	}
#else
	inline Type& operator[](unsigned int Index)
	{
		checkSlow(Index < Num());

		return Blocks[Index >> nShiftBits][Index & BlockIndexBitmask];
	}
	inline const Type& operator[](unsigned int Index) const
	{
		checkSlow(Index < Num());

		return Blocks[Index >> nShiftBits][Index & BlockIndexBitmask];
	}
#endif

	// apply f() to each member sequentially
	template <typename Func>
	void Apply(const Func& f);

	/**
	 * Serialization operator for TDynamicVector.
	 *
	 * @param Ar Archive to serialize with.
	 * @param Vec Vector to serialize.
	 * @returns Passing down serializing archive.
	 */
	friend FArchive& operator<<(FArchive& Ar, TDynamicVector& Vec)
	{
		Vec.Serialize<false, false>(Ar);
		return Ar;
	}

	/**
	 * Serialize vector to and from an archive
	 * @tparam bForceBulkSerialization Forces serialization to consider data to be trivial and serialize it in bulk to potentially achieve better performance.
	 * @tparam bUseCompression Use compression to serialize data; the resulting size will likely be smaller but serialization will take significantly longer.
	 * @param Ar Archive to serialize with
	 */
	template <bool bForceBulkSerialization = false, bool bUseCompression = false>
	void Serialize(FArchive& Ar)
	{
		Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
		if (Ar.IsLoading() && Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::DynamicMeshCompactedSerialization)
		{
			const SIZE_T CountBytes = 3 * sizeof(uint32) + Blocks.Num() * BlockSize * sizeof(Type);
			Ar.CountBytes(CountBytes, CountBytes);
			Ar << CurBlock;
			Ar << CurBlockUsed;
			Blocks.Serialize_LegacyLoad(Ar);

			// Account for a previously existing issue where one additional empty block was allocated and the resulting values for the current block can cause
			// crashes, for example when using Back().
			if (CurBlock > 0 && CurBlockUsed == 0)
			{
				const SIZE_T Count = Num();
				checkSlow(Count % BlockSize == 0);
				checkSlow(Count / BlockSize < Blocks.Num());

				// Fix CurBlock and CurBlockUsed.
				SetCurBlock(Count);

				// Remove empty block.
				Blocks.Truncate(int32(Count / BlockSize), EAllowShrinking::No);
			}
		}
		else
		{
			uint32 SerializeNum = Num();
			const SIZE_T CountBytes =  sizeof(uint32) + SerializeNum * sizeof(Type);
			Ar.CountBytes(CountBytes, CountBytes);
			Ar << SerializeNum;
			if (SerializeNum == 0 && Ar.IsLoading())
			{
				Clear();
			}
			else if (SerializeNum > 0)
			{
				SetCurBlock(SerializeNum);
				Blocks.template Serialize<bForceBulkSerialization, bUseCompression>(Ar, SerializeNum);
			}
		}
	}

public:
	/*
	 * FIterator class iterates over values of vector
	 */
	class FIterator
	{
	public:
		inline const Type& operator*() const
		{
			return (*DVector)[Idx];
		}
		inline Type& operator*()
		{
			return (*DVector)[Idx];
		}
		inline FIterator& operator++()   // prefix
		{
			Idx++;
			return *this;
		}
		inline FIterator operator++(int) // postfix
		{
			FIterator Copy(*this);
			Idx++;
			return Copy;
		}
		inline bool operator==(const FIterator& Itr2) const
		{
			return DVector == Itr2.DVector && Idx == Itr2.Idx;
		}
		inline bool operator!=(const FIterator& Itr2) const
		{
			return DVector != Itr2.DVector || Idx != Itr2.Idx;
		}

	private:
		friend class TDynamicVector;
		FIterator(TDynamicVector* DVectorIn, unsigned int IdxIn)
			: DVector(DVectorIn), Idx(IdxIn){}
		TDynamicVector* DVector{};
		unsigned int Idx{0};
	};

	/** @return iterator at beginning of vector */
	FIterator begin()
	{
		return IsEmpty() ? end() : FIterator{this, 0};
	}
	/** @return iterator at end of vector */
	FIterator end()
	{
		return FIterator{this,  (unsigned int)GetLength()};
	}

	/*
	 * FConstIterator class iterates over values of vector
	 */
	class FConstIterator
	{
	public:
		inline const Type& operator*() const
		{
			return (*DVector)[Idx];
		}
		inline FConstIterator& operator++()   // prefix
		{
			Idx++;
			return *this;
		}
		inline FConstIterator operator++(int) // postfix
		{
			FConstIterator Copy(*this);
			Idx++;
			return Copy;
		}
		inline bool operator==(const FConstIterator& Itr2) const
		{
			return DVector == Itr2.DVector && Idx == Itr2.Idx;
		}
		inline bool operator!=(const FConstIterator& Itr2) const
		{
			return DVector != Itr2.DVector || Idx != Itr2.Idx;
		}

	private:
		friend class TDynamicVector;
		FConstIterator(const TDynamicVector* DVectorIn, unsigned int IdxIn)
			: DVector(DVectorIn), Idx(IdxIn){}
		const TDynamicVector* DVector{};
		unsigned int Idx{0};
	};

	/** @return iterator at beginning of vector */
	FConstIterator begin() const
	{
		return IsEmpty() ? end() : FConstIterator{this, 0};
	}
	/** @return iterator at end of vector */
	FConstIterator end() const
	{
		return FConstIterator{this,  (unsigned int)GetLength()};
	}

private:
	static constexpr int nShiftBits = 9;
	static constexpr int BlockSize = 1 << nShiftBits;
	static constexpr int BlockIndexBitmask = BlockSize - 1; // low 9 bits
	static_assert( BlockSize && ((BlockSize & (BlockSize - 1)) == 0), "DynamicVector: BlockSize must be a power of two");
	static constexpr unsigned int MaxBlockCount = MaxSize / BlockSize + 1;

	unsigned int CurBlock{0};  //< Current block index; always points to the block with the last item in the vector or it is set to zero if the vector is empty. 
	unsigned int CurBlockUsed{0};  //< Number of used items in the current block.

	using BlockType = TStaticArray<Type, BlockSize>;
	

	/**
	 * This is used to store/manage an array of BlockType*. Basically a stripped
	 * down TIndirectArray that does not do range-checking on operator[] in non-debug builds
	 */
	template<typename ArrayType>
	class TBlockVector
	{
	protected:
		TArray<ArrayType*> Elements;

	public: 
		TBlockVector() = default;
		TBlockVector(TBlockVector&& Moved) = default;

		TBlockVector(const TBlockVector& Copy)
		{
			int32 N = Copy.Num();
			Elements.Reserve(N);
			for (int32 k = 0; k < N; ++k)
			{
				Elements.Add(new ArrayType(*Copy.Elements[k]));
			}
		}
		TBlockVector& operator=(const TBlockVector& Copy)
		{
			if (&Copy != this)
			{
				int32 N = Copy.Num();
				Empty(N);
				Elements.Reserve(N);
				for (int32 k = 0; k < N; ++k)
				{
					Elements.Add(new ArrayType(*Copy.Elements[k]));
				}
			}
			return *this;
		}
		TBlockVector& operator=(TBlockVector&& Moved)
		{
			if (&Moved != this)
			{
				Empty();
				Elements = MoveTemp(Moved.Elements);
			}
			return *this;
		}
		~TBlockVector()
		{
			Empty();
		}

		int32 Num() const { return Elements.Num(); }

#if UE_BUILD_DEBUG
		ArrayType& operator[](int32 Index) { return *Elements[Index]; }
		const ArrayType& operator[](int32 Index) const { return *Elements[Index]; }
#else
		ArrayType& operator[](int32 Index) { return *Elements.GetData()[Index]; }
		const ArrayType& operator[](int32 Index) const { return *Elements.GetData()[Index]; }
#endif

		void Add(ArrayType* NewElement)
		{
			Elements.Add(NewElement);
		}

		void Truncate(int32 NewElementCount, EAllowShrinking AllowShrinking)
		{
			for (int32 k = NewElementCount; k < Elements.Num(); ++k)
			{
				delete Elements[k];
				Elements[k] = nullptr;
			}
			Elements.RemoveAt(NewElementCount, Elements.Num() - NewElementCount, AllowShrinking);
		}

		void Empty(int32 NewReservedSize = 0)
		{
			for (int32 k = 0, N = Elements.Num(); k < N; ++k)
			{
				delete Elements[k];
			}
			Elements.Empty(NewReservedSize);
		}

		/** Serialize TBlockVector to archive. */
		void Serialize_LegacyLoad(FArchive& Ar)
		{
			// Only loading of legacy archives is allowed.
			checkSlow(Ar.IsLoading());
			if (Ar.IsLoading())
			{
				// Bulk serialization for a number of double types was enabled as part of the transition to Large World Coordinates.
				// If the currently stored type is one of these types, and the archive is from before bulk serialization for these types was enabled,
				// we need to still use per element serialization for legacy data.
				constexpr bool bIsLWCBulkSerializedDoubleType =
					std::is_same_v<Type, FVector2d> ||
					std::is_same_v<Type, FVector3d> ||
					std::is_same_v<Type, FVector4d> ||
					std::is_same_v<Type, FQuat4d> ||
					std::is_same_v<Type, FTransform3d>;
				const bool bUseBulkSerialization = TCanBulkSerialize<Type>::Value && !(bIsLWCBulkSerializedDoubleType && Ar.UEVer() <
					EUnrealEngineObjectUE5Version::LARGE_WORLD_COORDINATES);

				// Lambda for serializing a block element either via bulk serializing the contained data or via serializing the element container itself.
				// Note that the static_cast<> was necessary to resolve compiler errors when using MSVC.
				const auto SerializeElement = bUseBulkSerialization
					                              ? static_cast<void(*)(FArchive&, ArrayType*)>([](FArchive& Archive, ArrayType* Element)
					                              {
						                              Archive.Serialize(Element->GetData(), Element->Num() * sizeof(Type));
					                              })
					                              : static_cast<void(*)(FArchive&, ArrayType*)>([](FArchive& Archive, ArrayType* Element)
					                              {
						                              Archive << *Element;
					                              });

				int32 BlockNum = Num();
				Ar << BlockNum;
				Empty(BlockNum);

				for (int32 Index = 0; Index < BlockNum; Index++)
				{
					ArrayType* NewElement = new ArrayType;
					SerializeElement(Ar, NewElement);
					Add(NewElement);
				}
			}
		}

		template <bool bForceBulkSerialization, bool bUseCompression>
		void Serialize(FArchive& Ar, uint32 Num)
		{
			constexpr bool bUseBulkSerialization = bForceBulkSerialization || TCanBulkSerialize<Type>::Value || sizeof(Type) == 1;
			static_assert(!bUseCompression || bUseBulkSerialization, "Compression only available when using bulk serialization");

			checkSlow(Num > 0);

			// Serialize compression flag, which adds flexibility when de-serializing existing data even if some implementation details change.  
			bool bUseCompressionForBulkSerialization = bUseBulkSerialization && bUseCompression;
			Ar << bUseCompressionForBulkSerialization;

			// Determine number of blocks.
			const bool bNumIsNotMultipleOfBlockSize = Num % BlockSize != 0;
			const uint32 NumBlocks = Num / BlockSize + bNumIsNotMultipleOfBlockSize;
			
			if (bUseCompressionForBulkSerialization)
			{
				// When using compression, copy everything to/from a big single buffer and serialize the big buffer.
				// This results in better compression ratios while at the same time accelerating compression. 

				TArray<Type> Buffer;
				Buffer.SetNumUninitialized(Num);
				Type* BufferPtr = Buffer.GetData();
				SIZE_T NumCopyRemaining = Num;

				if (!Ar.IsLoading())
				{					
					for (uint32 Index = 0; Index < NumBlocks; ++Index)
					{
						const SIZE_T NumCopy = FMath::template Min<SIZE_T>(NumCopyRemaining, (SIZE_T)BlockSize);
						FMemory::Memcpy(BufferPtr, Elements[Index]->GetData(), NumCopy * sizeof(Type));
						BufferPtr += NumCopy;
						NumCopyRemaining -= BlockSize;
					}
				}

				Ar.SerializeCompressedNew(Buffer.GetData(), Num * sizeof(Type), NAME_Oodle, NAME_Oodle, COMPRESS_NoFlags, false, nullptr);

				if (Ar.IsLoading())
				{
					Empty(NumBlocks);
					for (uint32 Index = 0; Index < NumBlocks; ++Index)
					{
						ArrayType *const NewElement = new ArrayType;
						const SIZE_T NumCopy = FMath::template Min<SIZE_T>(NumCopyRemaining, (SIZE_T)BlockSize);
						FMemory::Memcpy(NewElement->GetData(), BufferPtr, NumCopy * sizeof(Type));
						Add(NewElement);
						BufferPtr += NumCopy;
						NumCopyRemaining -= BlockSize;
					}
				}
			}
			else
			{
				const auto SerializeElement = [&Ar, bUseBulkSerialization](ArrayType* Element, uint32 ElementNum)
				{
					if (bUseBulkSerialization)
					{
						Ar.Serialize(Element->GetData(), ElementNum * sizeof(Type));
					}
					else
					{
						for (uint32 Index = 0; Index < ElementNum; ++Index)
						{
							Ar << (*Element)[Index];
						}
					}
				};

				if (Ar.IsLoading())
				{
					Empty(NumBlocks);
					for (uint32 Index = 0; Index < NumBlocks; ++Index)
					{
						ArrayType *const NewElement = new ArrayType;
						SerializeElement(NewElement, FMath::template Min<uint32>(Num, BlockSize));
						Add(NewElement);
						Num -= BlockSize;
					}
				}
				else
				{
					for (uint32 Index = 0; Index < NumBlocks; ++Index)
					{
						SerializeElement(&(*this)[Index], FMath::template Min<uint32>(Num, BlockSize));
						Num -= BlockSize;
					}
				}
			}
		}
	};

	friend bool operator==(const TDynamicVector& Lhs, const TDynamicVector& Rhs)
	{
		if (Lhs.Num() != Rhs.Num())
		{
			return false;
		}

		if (Lhs.IsEmpty())
		{
			return true;
		}
		
		const uint32 LhsCurBlock = Lhs.CurBlock;
		for (uint32 BlockIndex = 0; BlockIndex < LhsCurBlock; ++BlockIndex)
		{
			if (!CompareItems(Lhs.Blocks[BlockIndex].GetData(), Rhs.Blocks[BlockIndex].GetData(), BlockSize))
			{
				return false;
			}
		}
		return CompareItems(Lhs.Blocks[LhsCurBlock].GetData(), Rhs.Blocks[LhsCurBlock].GetData(), Lhs.CurBlockUsed);
	}

	friend bool operator!=(const TDynamicVector& Lhs, const TDynamicVector& Rhs)
	{
		return !(Lhs == Rhs);
	}

	void SetCurBlock(SIZE_T Count)
	{
		// Reset block index for the last item and used item count within the last block.
		// This is similar to what happens when computing the indices in operator[], but we additionally account for (1) the vector being empty and (2) that the
		// used item count within the last block needs to be one more than the index of the last item. 
		const int32 LastItemIndex = int32(Count - 1);
		CurBlock = Count != 0 ? LastItemIndex >> nShiftBits : 0;
		CurBlockUsed = Count != 0 ? (LastItemIndex & BlockIndexBitmask) + 1 : 0;
	}

	// memory chunks for dynamic vector
	TBlockVector<BlockType> Blocks;
};

template <class Type, int N>
class TDynamicVectorN
{
public:
	TDynamicVectorN() = default;
	TDynamicVectorN(const TDynamicVectorN& Copy) = default;
	TDynamicVectorN(TDynamicVectorN&& Moved) = default;
	TDynamicVectorN& operator=(const TDynamicVectorN& Copy) = default;
	TDynamicVectorN& operator=(TDynamicVectorN&& Moved) = default;

	inline void Clear()
	{
		Data.Clear();
	}
	inline void Fill(const Type& Value)
	{
		Data.Fill(Value);
	}
	inline void Resize(unsigned int Count)
	{
		Data.Resize(Count * N);
	}
	inline void Resize(unsigned int Count, const Type& InitValue)
	{
		Data.Resize(Count * N, InitValue);
	}
	inline bool IsEmpty() const
	{
		return Data.IsEmpty();
	}
	inline size_t GetLength() const
	{
		return Data.GetLength() / N;
	}
	inline int GetBlockSize() const
	{
		return Data.GetBlockSize();
	}
	inline size_t GetByteCount() const
	{
		return Data.GetByteCount();
	}

	// simple struct to help pass N-dimensional data without presuming a vector type (e.g. just via initializer list)
	struct ElementVectorN
	{
		Type Data[N];
	};

	inline void Add(const ElementVectorN& AddData)
	{
		// todo specialize for N=2,3,4
		for (int i = 0; i < N; i++)
		{
			Data.Add(AddData.Data[i]);
		}
	}

	inline void PopBack()
	{
		for (int i = 0; i < N; i++)
		{
			PopBack();
		}
	} // TODO specialize

	inline void InsertAt(const ElementVectorN& AddData, unsigned int Index)
	{
		for (int i = 1; i <= N; i++)
		{
			Data.InsertAt(AddData.Data[N - i], N * (Index + 1) - i);
		}
	}

	inline Type& operator()(unsigned int TopIndex, unsigned int SubIndex)
	{
		return Data[TopIndex * N + SubIndex];
	}
	inline const Type& operator()(unsigned int TopIndex, unsigned int SubIndex) const
	{
		return Data[TopIndex * N + SubIndex];
	}
	inline void SetVector2(unsigned int TopIndex, const TVector2<Type>& V)
	{
		check(N >= 2);
		unsigned int i = TopIndex * N;
		Data[i] = V.X;
		Data[i + 1] = V.Y;
	}
	inline void SetVector3(unsigned int TopIndex, const TVector<Type>& V)
	{
		check(N >= 3);
		unsigned int i = TopIndex * N;
		Data[i] = V.X;
		Data[i + 1] = V.Y;
		Data[i + 2] = V.Z;
	}
	inline TVector2<Type> AsVector2(unsigned int TopIndex) const
	{
		check(N >= 2);
		return TVector2<Type>(
			Data[TopIndex * N + 0],
			Data[TopIndex * N + 1]);
	}
	inline TVector<Type> AsVector3(unsigned int TopIndex) const
	{
		check(N >= 3);
		return TVector<Type>(
			Data[TopIndex * N + 0],
			Data[TopIndex * N + 1],
			Data[TopIndex * N + 2]);
	}
	inline FIndex2i AsIndex2(unsigned int TopIndex) const
	{
		check(N >= 2);
		return FIndex2i(
			(int)Data[TopIndex * N + 0],
			(int)Data[TopIndex * N + 1]);
	}
	inline FIndex3i AsIndex3(unsigned int TopIndex) const
	{
		check(N >= 3);
		return FIndex3i(
			(int)Data[TopIndex * N + 0],
			(int)Data[TopIndex * N + 1],
			(int)Data[TopIndex * N + 2]);
	}
	inline FIndex4i AsIndex4(unsigned int TopIndex) const
	{
		check(N >= 4);
		return FIndex4i(
			(int)Data[TopIndex * N + 0],
			(int)Data[TopIndex * N + 1],
			(int)Data[TopIndex * N + 2],
			(int)Data[TopIndex * N + 3]);
	}

private:
	TDynamicVector<Type> Data;

	friend class FIterator;
};

template class TDynamicVectorN<double, 2>;

using  TDynamicVector3f = TDynamicVectorN<float, 3>;
using  TDynamicVector2f = TDynamicVectorN<float, 2>;
using  TDynamicVector3d = TDynamicVectorN<double, 3>;
using  TDynamicVector2d = TDynamicVectorN<double, 2>;
using  TDynamicVector3i = TDynamicVectorN<int, 3>;
using  TDynamicVector2i = TDynamicVectorN<int, 2>;

template <class Type>
void TDynamicVector<Type>::Clear()
{
	Blocks.Truncate(1, EAllowShrinking::No);
	CurBlock = 0;
	CurBlockUsed = 0;
	if (Blocks.Num() == 0)
	{
		Blocks.Add(new BlockType());
	}
}

template <class Type>
void TDynamicVector<Type>::Fill(const Type& Value)
{
	size_t Count = Blocks.Num();
	for (unsigned int i = 0; i < Count; ++i)
	{
		for (uint32 ElementIndex = 0; ElementIndex < BlockSize; ++ElementIndex)
		{
			Blocks[i][ElementIndex] = Value;
		}
	}
}

template <class Type>
void TDynamicVector<Type>::Resize(unsigned int Count)
{
	if (GetLength() == Count)
	{
		return;
	}

	// Determine how many blocks we need, but make sure we have at least one block available.
	const bool bCountIsNotMultipleOfBlockSize = Count % BlockSize != 0;
	const int32 NumBlocksNeeded = FMath::Max(1, static_cast<int32>(Count) / BlockSize + bCountIsNotMultipleOfBlockSize);

	// Determine how many blocks are currently allocated.
	int32 NumBlocksCurrent = Blocks.Num();

	// Allocate needed additional blocks.
	while (NumBlocksCurrent < NumBlocksNeeded)
	{
		Blocks.Add(new BlockType());
		++NumBlocksCurrent;
	}

	// Remove unneeded blocks.
	if (NumBlocksCurrent > NumBlocksNeeded)
	{
		Blocks.Truncate(NumBlocksNeeded, EAllowShrinking::No);
	}

	// Set current block.
	SetCurBlock(Count);
}

template <class Type>
void TDynamicVector<Type>::Resize(unsigned int Count, const Type& InitValue)
{
	size_t nCurSize = GetLength();
	Resize(Count);
	for (unsigned int Index = (unsigned int)nCurSize; Index < Count; ++Index)
	{
		this->operator[](Index) = InitValue;
	}
}

template <class Type>
bool TDynamicVector<Type>::SetMinimumSize(unsigned int Count, const Type& InitValue)
{
	size_t nCurSize = GetLength();
	if (Count <= nCurSize)
	{
		return false;
	}
	Resize(Count);
	for (unsigned int Index = (unsigned int)nCurSize; Index < Count; ++Index)
	{
		this->operator[](Index) = InitValue;
	}
	return true;
}

template <class Type>
void TDynamicVector<Type>::Add(const Type& Value)
{
	checkSlow(size_t(MaxSize) >= GetLength() + 1)
	if (CurBlockUsed == BlockSize)
	{
		if (CurBlock == Blocks.Num() - 1)
		{
			Blocks.Add(new BlockType());
		}
		CurBlock++;
		CurBlockUsed = 0;
	}
	Blocks[CurBlock][CurBlockUsed] = Value;
	CurBlockUsed++;
}


template <class Type>
void TDynamicVector<Type>::Add(const TDynamicVector<Type>& AddData)
{
	// @todo it could be more efficient to use memcopies here...
	size_t nSize = AddData.Num();
	for (unsigned int k = 0; k < (unsigned int) nSize; ++k)
	{
		Add(AddData[k]);
	}
}

template <class Type>
void TDynamicVector<Type>::PopBack()
{
	if (CurBlockUsed > 0)
	{
		CurBlockUsed--;
	}
	if (CurBlockUsed == 0 && CurBlock > 0)
	{
		CurBlock--;
		CurBlockUsed = BlockSize;
		// remove block ??
	}
}

template <class Type>
Type& TDynamicVector<Type>::ElementAt(unsigned int Index, Type InitialValue)
{
	size_t s = GetLength();
	if (Index == s)
	{
		Add(InitialValue);
	}
	else if (Index > s)
	{
		Resize(Index);
		Add(InitialValue);
	}
	return this->operator[](Index);
}

template <class Type>
void TDynamicVector<Type>::InsertAt(const Type& AddData, unsigned int Index)
{
	size_t s = GetLength();
	if (Index == s)
	{
		Add(AddData);
	}
	else if (Index > s)
	{
		Resize(Index);
		Add(AddData);
	}
	else
	{
		(*this)[Index] = AddData;
	}
}

template <class Type>
void TDynamicVector<Type>::InsertAt(const Type& AddData, unsigned int Index, const Type& InitValue)
{
	size_t nCurSize = GetLength();
	InsertAt(AddData, Index);
	// initialize all new values up to (but not including) the inserted index
	for (unsigned int i = (unsigned int)nCurSize; i < Index; ++i)
	{
		this->operator[](i) = InitValue;
	}
}

template <typename Type>
template <typename Func>
void TDynamicVector<Type>::Apply(const Func& applyF)
{
	for (int bi = 0; bi < CurBlock; ++bi)
	{
		auto block = Blocks[bi];
		for (int k = 0; k < BlockSize; ++k)
		{
			applyF(block[k], k);
		}
	}
	auto lastblock = Blocks[CurBlock];
	for (int k = 0; k < CurBlockUsed; ++k)
	{
		applyF(lastblock[k], k);
	}
}


} // end namespace UE::Geometry
} // end namespace UE

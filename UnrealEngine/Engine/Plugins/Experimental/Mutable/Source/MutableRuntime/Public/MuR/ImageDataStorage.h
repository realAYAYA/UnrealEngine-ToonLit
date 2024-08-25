// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImageTypes.h"
#include "MuR/MemoryTrackingAllocationPolicy.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/StaticArray.h"

namespace mu::MemoryCounters
{
	struct FImageMemoryCounterTag {};
	using FImageMemoryCounter = TMemoryCounter<FImageMemoryCounterTag>;
}

namespace mu
{
	using FImageArray = TArray<uint8, FDefaultMemoryTrackingAllocator<MemoryCounters::FImageMemoryCounter>>;
}

namespace mu::FImageDataStorageInternal
{
	FORCEINLINE int32 ComputeNumLODsForSize(FImageSize Size)
	{
		return static_cast<int32>(FMath::CeilLogTwo(FMath::Max(Size.X, Size.Y))) + 1;
	}
}

namespace mu
{
class MUTABLERUNTIME_API FImageDataStorage
{
public:
	TArray<FImageArray, TInlineAllocator<1>> Buffers;
	
	FImageSize ImageSize = FImageSize(0, 0);
	EImageFormat ImageFormat = EImageFormat::IF_NONE;
	uint8 NumLODs = 0;

	static constexpr int32 NumLODsInCompactedTail = 7;
	// This is needed for images its size cannot be known from dimensions and format, e.g., RLE compressed formats.
	// It stores the offset to the end of the LOD so that CompactedTailOffsets[LOD] - CompactedTailOffsets[LOD - 1]
	// is the size of LOD.
	TStaticArray<uint16, NumLODsInCompactedTail> CompactedTailOffsets = MakeUniformStaticArray<uint16, NumLODsInCompactedTail>(0);

public:

	FImageDataStorage();
	FImageDataStorage(const FImageDesc& Desc);

	FImageDataStorage(const FImageDataStorage& Other);
	FImageDataStorage& operator=(const FImageDataStorage& Other);
	
	FImageDataStorage(FImageDataStorage&& Other) = default;
	FImageDataStorage& operator=(FImageDataStorage&& Other) = default;

	bool operator==(const FImageDataStorage& Other) const;

	FORCEINLINE FImageDesc MakeImageDesc() const
	{
		return FImageDesc(ImageSize, ImageFormat, NumLODs);
	}

	FORCEINLINE void InitVoid(const FImageDesc& Desc) 
	{
		Buffers.Empty();	
		CompactedTailOffsets[0] = TNumericLimits<uint16>::Max();

		ImageSize = Desc.m_size;
		ImageFormat = Desc.m_format;
		NumLODs = Desc.m_lods;
	}

	FORCEINLINE bool IsVoid() const
	{
		return Buffers.IsEmpty() && CompactedTailOffsets[0] == TNumericLimits<uint16>::Max();
	}

	void InitInternalArray(int32 Index, int32 Size, EInitializationType InitType);
	void Init(const FImageDesc& ImageDesc, EInitializationType InitType);

	void CopyFrom(const FImageDataStorage& Other);

	FORCEINLINE int32 GetNumLODs() const
	{
		return NumLODs;
	}

	/**
	* Get a const view to the data containing the LODIndex.
	*/
	FORCEINLINE TArrayView<const uint8> GetLOD(int32 LODIndex) const
	{
		check(!IsVoid());
		if (LODIndex >= NumLODs)
		{
			//check(false);
			return TArrayView<const uint8>();
		}

		const int32 FirstTailLOD = ComputeFirstCompactedTailLOD();
		if (LODIndex >= FirstTailLOD)
		{
			const int32 LODIndexInTail = FMath::Max(0, LODIndex - FirstTailLOD);
			
			const int32 LODInTailBegin = LODIndexInTail == 0 ? 0 : CompactedTailOffsets[LODIndexInTail - 1];
			const int32 LODInTailEnd = CompactedTailOffsets[LODIndexInTail];

			const FImageArray& TailBuffer = Buffers.Last();
			return MakeArrayView(TailBuffer.GetData() + LODInTailBegin, LODInTailEnd - LODInTailBegin);
		}

		return MakeArrayView(Buffers[LODIndex].GetData(), Buffers[LODIndex].Num());
	}

	/**
	* Get a view to the data containing the LODIndex.
	*/
	FORCEINLINE TArrayView<uint8> GetLOD(int32 LODIndex)
	{	
		const TArrayView<const uint8> ConstLODView = const_cast<const FImageDataStorage*>(this)->GetLOD(LODIndex);
		return TArrayView<uint8>(const_cast<uint8*>(ConstLODView.GetData()), ConstLODView.Num());
	}

	/**
	 * Change the number of lods in the image. If InNumLODs is smaller than the current lod count, 
	 * LODs are dropped from the tail up.
	 */
	void SetNumLODs(int32 InNumLODs, EInitializationType InitType = EInitializationType::NotInitialized);

	/**
	 * Remove NumLODsToDrop starting form LOD 0. The resulting image will be smaller. 
	 */
	void DropLODs(int32 NumLODsToDrop);

	/**
	 * Resizes LODIndex Data to NewSizeBytes
	 */
	void ResizeLOD(int32 LODIndex, int32 NewSizeBytes);

	FORCEINLINE const FImageArray& GetInternalArray(int32 LODIndex) const
	{	
		check(!IsVoid());
		check(LODIndex < NumLODs);
		return Buffers[FMath::Min(LODIndex, ComputeFirstCompactedTailLOD())];
	}

	FORCEINLINE FImageArray& GetInternalArray(int32 LODIndex)
	{	
		check(!IsVoid());
		check(LODIndex < NumLODs);
		return Buffers[FMath::Min(LODIndex, ComputeFirstCompactedTailLOD())];
	}

	FORCEINLINE int32 ComputeFirstCompactedTailLOD() const
	{
		using namespace FImageDataStorageInternal;
		return FMath::Max(0, ComputeNumLODsForSize(ImageSize) - NumLODsInCompactedTail); 
	}

	/**
	 * Stateless iteration in batches.
	 */
	FORCEINLINE int32 GetNumBatches(int32 BatchSizeInElems, int32 BatchElemSizeInBytes) const
	{	
		return GetNumBatchesLODRange(BatchSizeInElems, BatchElemSizeInBytes, 0, NumLODs);
	}

	FORCEINLINE TArrayView<const uint8> GetBatch(int32 BatchId, int32 BatchSizeInElems, int32 BatchElemSizeInBytes) const
	{
		return GetBatchLODRange(BatchId, BatchSizeInElems, BatchElemSizeInBytes, 0, NumLODs);
	}

	FORCEINLINE TArrayView<uint8> GetBatch(int32 BatchId, int32 BatchSizeInElems, int32 BatchElemSizeInBytes)
	{
		// Use the const_cast idiom to generate the non-const operation. 
		const TArrayView<const uint8> ConstBatchView = 
				const_cast<const FImageDataStorage*>(this)->GetBatch(BatchId, BatchSizeInElems, BatchElemSizeInBytes);
		return TArrayView<uint8>(const_cast<uint8*>(ConstBatchView.GetData()), ConstBatchView.Num());
	}

	/**
	 * Returns the number of batches GetBatchFirtsLODOffet will admit and that will cover
	 * the first LOD buffer with OffsetInBytes removed from the front. 
	 * 
	 * A batch cannot be larger than BatchElemsSize. The last batch of a buffer will be smaller if 
	 * BatchSizeInElems*BatchElemSizeInBytes is not multiple of the buffer size.
	 *
	 **/
	FORCEINLINE int32 GetNumBatchesFirstLODOffset(int32 BatchSizeInElems, int32 BatchElemSizeInBytes, int32 OffsetInBytes) const
	{
		check(!IsVoid());
		TArrayView<const uint8> FirstLODView = GetLOD(0);
		
		check(OffsetInBytes % BatchElemSizeInBytes == 0);
		check(FirstLODView.Num() % BatchElemSizeInBytes == 0);
		check(FirstLODView.Num() > OffsetInBytes); 

		return FMath::DivideAndRoundUp(FirstLODView.Num() - OffsetInBytes, BatchSizeInElems*BatchElemSizeInBytes);
	}

	/**
	 * Returns a non modifiable view to the portion of the first LOD buffer with OffsetInBytes for the BatchId 
	 *
	 * A batch cannot be larger than BatchElemsSize. The last batch of a buffer will be smaller if 
	 * BatchSizeInElems*BatchElemSizeInBytes is not multiple of the buffer size.
	 *
	 */
	FORCEINLINE TArrayView<const uint8> GetBatchFirstLODOffset(int32 BatchId, int32 BatchSizeInElems, int32 BatchElemSizeInBytes, int32 OffsetInBytes = 0) const
	{
		check(!IsVoid());
		TArrayView<const uint8> FirstLODView = GetLOD(0);
		
		check(OffsetInBytes < FirstLODView.Num());
		check(FirstLODView.Num() % BatchElemSizeInBytes == 0);
		check(OffsetInBytes % BatchElemSizeInBytes == 0);
	
		TArrayView<const uint8> OffsetedLODView = MakeArrayView(FirstLODView.GetData() + OffsetInBytes, FirstLODView.Num() - OffsetInBytes);

		const int32 BatchOffset = BatchId * BatchSizeInElems * BatchElemSizeInBytes;

		if (OffsetedLODView.Num() > BatchOffset)
		{
			return TArrayView<const uint8>();
		}

		return MakeArrayView(OffsetedLODView.GetData() + BatchOffset, FMath::Min(BatchSizeInElems, OffsetedLODView.Num() - BatchOffset));
	}

	/**
	 * Non const version of GetBatchFirstLODOffset(). 
	 */
	FORCEINLINE TArrayView<uint8> GetBatchFirstLODOffset(int32 BatchId, int32 BatchSizeInElems, int32 BatchElemSizeInBytes, int32 OffsetInBytes = 0)
	{
		const TArrayView<const uint8> ConstBatchView = 
				const_cast<const FImageDataStorage*>(this)->GetBatchFirstLODOffset(BatchId, BatchSizeInElems, BatchElemSizeInBytes, OffsetInBytes);

		return TArrayView<uint8>(const_cast<uint8*>(ConstBatchView.GetData()), ConstBatchView.Num());
	}

	/**
	 * Returns the number of batches GetBatchLODRange will admit and that will cover
	 * the [LODBegin, LODEnd) range.
	 *
	 **/
	FORCEINLINE int32 GetNumBatchesLODRange(int32 BatchSizeInElems, int32 BatchElemSizeInBytes, int32 LODBegin, int32 LODEnd) const
	{
		check(!IsVoid());
		check(LODBegin < LODEnd);
		check(LODBegin >= 0 && LODEnd <= NumLODs);

		const int32 BatchNumBytes = BatchSizeInElems*BatchElemSizeInBytes;
		
		const int32 FirstCompactedTailLOD = ComputeFirstCompactedTailLOD();
		const int32 NumLODsInTail = FMath::Max(0, NumLODs - FirstCompactedTailLOD);
		const int32 NumLODsNotInTail = FMath::Max(0, NumLODs - NumLODsInTail);

		const int32 LODsNotInTailLODRangeEnd = FMath::Min(LODEnd, NumLODsNotInTail);

		int32 NumBatches = 0;
		for (int32 BufferIndex = LODBegin; BufferIndex < LODsNotInTailLODRangeEnd; ++BufferIndex)
		{	
			NumBatches += FMath::DivideAndRoundUp(Buffers[BufferIndex].Num(), BatchNumBytes);
		}

		if (FirstCompactedTailLOD < LODEnd)
		{
			TArrayView<const uint8> FirstLODInRangeView = GetLOD(FirstCompactedTailLOD);
			TArrayView<const uint8> LastLODInRangeView = GetLOD(LODEnd - 1);

			const int32 TailInLODRangeNumBytes = LastLODInRangeView.GetData() - FirstLODInRangeView.GetData() + LastLODInRangeView.Num();
			NumBatches += FMath::DivideAndRoundUp(TailInLODRangeNumBytes, BatchNumBytes);
		}
		return NumBatches;
	}

	
	/**
	 * Returns a non modifiable view to the portion of the [LODBegin, LODEnd) for the BatchId. 
	 *
	 * A batch cannot be larger than BatchElemsSize. The last batch of a buffer will be smaller if 
	 * BatchSizeInElems*BatchElemSizeInBytes is not multiple of the buffer size.
	 *
	 */
	FORCEINLINE TArrayView<const uint8> GetBatchLODRange(int32 BatchId, int32 BatchSizeInElems, int32 BatchElemSizeInBytes, int32 LODBegin, int32 LODEnd) const
	{
		check(!IsVoid());
		check(LODBegin < LODEnd);
		check(LODBegin >= 0 && LODEnd <= NumLODs);

		const int32 BatchNumBytes = BatchSizeInElems*BatchElemSizeInBytes;

		const int32 FirstCompactedTailLOD = ComputeFirstCompactedTailLOD();
		const int32 NonTailBuffersEnd = FMath::Min(LODEnd, FirstCompactedTailLOD);

		int32 NumBatches = 0;	
		for (int32 BufferIndex = LODBegin; BufferIndex < NonTailBuffersEnd; ++BufferIndex)
		{
			const TArrayView<const uint8> BufferView = MakeArrayView(Buffers[BufferIndex].GetData(), Buffers[BufferIndex].Num());
			const int32 BufferNumBatches = FMath::DivideAndRoundUp(BufferView.Num(), BatchNumBytes); 
			
			check(BatchId >= NumBatches);
			int32 BufferBatchId = BatchId - NumBatches;

			if (BufferBatchId < BufferNumBatches)
			{ 
				const int32 BufferBatchBeginOffset = BufferBatchId * BatchNumBytes;

				return TArrayView<const uint8>(
						BufferView.GetData() + BufferBatchBeginOffset,
						FMath::Min(BufferView.Num() - BufferBatchBeginOffset, BatchNumBytes));
			}

			NumBatches += BufferNumBatches;
		}

		if (FirstCompactedTailLOD < LODEnd)
		{
			TArrayView<const uint8> FirstLODInRangeView = GetLOD(FirstCompactedTailLOD);
			TArrayView<const uint8> LastLODInRangeView = GetLOD(LODEnd - 1);
			
			const int32 TailInLODRangeNumBytes = LastLODInRangeView.GetData() - FirstLODInRangeView.GetData() + LastLODInRangeView.Num();
			TArrayView<const uint8> TailInLODRangeView = MakeArrayView(FirstLODInRangeView.GetData(), TailInLODRangeNumBytes);

			check(BatchId >= NumBatches);
			const int32 TailInLODRangeBatchId = BatchId - NumBatches;
			const int32 TailInLODRangeNumBatches = FMath::DivideAndRoundUp(TailInLODRangeView.Num(), BatchNumBytes);

			if (TailInLODRangeBatchId < TailInLODRangeNumBatches)
			{
				const int32 BufferBatchBeginOffset = TailInLODRangeBatchId * BatchNumBytes;
				
				return TArrayView<const uint8>(
							TailInLODRangeView.GetData() + BufferBatchBeginOffset,
							FMath::Min(TailInLODRangeView.Num() - BufferBatchBeginOffset, BatchNumBytes));
			}
		}
		// If the BatchId is not found, return an empty view.
		return TArrayView<const uint8>();
	}

	/**
	 * Non const version of GetBatchLODRange().
	 */
	FORCEINLINE TArrayView<uint8> GetBatchLODRange(int32 BatchId, int32 BatchSizeInElems, int32 BatchElemSizeInBytes, int32 LODBegin, int32 LODEnd)
	{
		const TArrayView<const uint8> ConstBatchView = 
			const_cast<const FImageDataStorage*>(this)->GetBatchLODRange(
					BatchId, BatchSizeInElems, BatchElemSizeInBytes, LODBegin, LODEnd);

		return TArrayView<uint8>(const_cast<uint8*>(ConstBatchView.GetData()), ConstBatchView.Num());
	}

	FORCEINLINE int32 GetAllocatedSize() const
	{ 
		SIZE_T Result = 0;
		for (const FImageArray& Buffer : Buffers)
		{
			Result += Buffer.GetAllocatedSize();
		}

		check(Result < TNumericLimits<int32>::Max())
		return (int32)Result;
	}

	FORCEINLINE int32 GetDataSize() const
	{
		SIZE_T Result = 0;
		for (const FImageArray& Buffer : Buffers)
		{
			Result += Buffer.Num();
		}

		check(Result < TNumericLimits<int32>::Max())
		return (int32)Result;
	}

	/**
	 * Returns true if all buffers are empty. This can be true after initialization for image formats that report 
	 * BytesPerBlock 0, e.g., RLE formats. If not initialized will also return true.
	 */
	FORCEINLINE bool IsEmpty() const
	{
		check(!IsVoid());

		for (const FImageArray& Buffer : Buffers)
		{
			if (!Buffer.IsEmpty())
			{
				return false;
			}
		}

		return true;
	}

	void Serialise(OutputArchive& Arch) const;
	void Unserialise(InputArchive& Arch);
};

}

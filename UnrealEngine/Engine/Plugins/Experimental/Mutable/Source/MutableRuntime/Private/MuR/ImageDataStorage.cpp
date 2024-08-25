// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/ImageDataStorage.h"
#include "MuR/SerialisationPrivate.h"

namespace mu::FImageDataStorageInternal
{
	FORCEINLINE FImageSize ComputeLODSize(FImageSize BaseSize, int32 LOD)
	{
		for (int32 L = 0; L < LOD; ++L)
		{
			BaseSize = FImageSize(
					FMath::DivideAndRoundUp<uint16>(BaseSize.X, 2),
					FMath::DivideAndRoundUp<uint16>(BaseSize.Y, 2));
		}

		return BaseSize;
	}

	FORCEINLINE void InitViewToBlack(const TArrayView<uint8>& View, EImageFormat Format)
	{
		const FImageFormatData FormatData = GetImageFormatData(Format);
		constexpr uint8 ZeroBuffer[FImageFormatData::MAX_BYTES_PER_BLOCK] = {0};		

		const bool bIsFormatBlackBlockZeroed = 
				FMemory::Memcmp(ZeroBuffer, FormatData.BlackBlock, FImageFormatData::MAX_BYTES_PER_BLOCK) == 0;
		if (bIsFormatBlackBlockZeroed)
		{
			FMemory::Memzero(View.GetData(), View.Num());
		}
		else
		{
			const int32 FormatBlockSize = FormatData.BytesPerBlock;
			
			if (FormatData.BytesPerBlock == 0)
			{
				return;
			}

			check(FormatData.BytesPerBlock != 0);
			check(FormatBlockSize <= FImageFormatData::MAX_BYTES_PER_BLOCK);
			check(View.Num() % FormatBlockSize == 0);

			uint8* const BufferBegin = View.GetData();
			const int32 DataSize = View.Num();

			for (int32 BlockDataOffset = 0; BlockDataOffset < DataSize; BlockDataOffset += FormatBlockSize)
			{
				FMemory::Memcpy(BufferBegin + BlockDataOffset, FormatData.BlackBlock, FormatBlockSize);
			}
		}
	}
}

namespace mu
{
	FImageDataStorage::FImageDataStorage()
	{
	}

	FImageDataStorage::FImageDataStorage(const FImageDesc& Desc)
	{
		Init(Desc, EInitializationType::NotInitialized);
	}

	FImageDataStorage::FImageDataStorage(const FImageDataStorage& Other)
	{
		CopyFrom(Other);
	}

	FImageDataStorage& FImageDataStorage::operator=(const FImageDataStorage& Other)
	{
		CopyFrom(Other);
		return *this;
	}

	bool FImageDataStorage::operator==(const FImageDataStorage& Other) const
	{
		const bool bSameMetadata =
				ImageSize == Other.ImageSize &&
				ImageFormat == Other.ImageFormat &&
				NumLODs == Other.NumLODs;

		if (!bSameMetadata)
		{
			return false;
		}

		if (Buffers.Num() != Other.Buffers.Num())
		{
			return false;
		}

		// Buffers are sorted from large to small, but mips usually have information about the whole image.
		// Process the small ones first.
		for (int32 BufferIndex = Buffers.Num() - 1; BufferIndex >= 0; --BufferIndex)
		{
			if (Buffers[BufferIndex] != Other.Buffers[BufferIndex])
			{
				return false;
			}
		}

		return true;
	}

	void FImageDataStorage::InitInternalArray(int32 Index, int32 Size, EInitializationType InitType)
	{
		using namespace FImageDataStorageInternal;

		check(Index < Buffers.Num());

		Buffers[Index].SetNumUninitialized(Size);

		if (InitType == EInitializationType::Black)
		{
			const TArrayView<uint8> BufferView = MakeArrayView(Buffers[Index].GetData(), Buffers[Index].Num());
			InitViewToBlack(BufferView, ImageFormat);
		}

		check(InitType == EInitializationType::Black || InitType == EInitializationType::NotInitialized);
	}

	void FImageDataStorage::Init(const FImageDesc& ImageDesc, EInitializationType InitType)
	{
		using namespace FImageDataStorageInternal;

		if (ImageFormat != ImageDesc.m_format || ImageSize != ImageDesc.m_size)
		{
			NumLODs = 0;
			Buffers.Empty();
		}

		ImageSize = ImageDesc.m_size;
		ImageFormat = ImageDesc.m_format;

		SetNumLODs(ImageDesc.m_lods, EInitializationType::NotInitialized);

		if (InitType == EInitializationType::Black)
		{
			for (FImageArray& Buffer : Buffers)
			{
				InitViewToBlack(MakeArrayView(Buffer.GetData(), Buffer.Num()), ImageFormat);
			}
		}
	}

	void FImageDataStorage::CopyFrom(const FImageDataStorage& Other)
	{
		// Copy images that have different format is not allowed.
		check(ImageFormat == EImageFormat::IF_NONE || Other.ImageFormat == ImageFormat);

		ImageSize = Other.ImageSize;
		ImageFormat = Other.ImageFormat;
		NumLODs = Other.NumLODs;

		Buffers = Other.Buffers;
		CompactedTailOffsets = Other.CompactedTailOffsets;
	}

	void FImageDataStorage::SetNumLODs(int32 InNumLODs, EInitializationType InitType)
	{
		using namespace FImageDataStorageInternal;

		check(!IsVoid());

		if (InNumLODs == NumLODs)
		{
			return;
		}

		const int32 FirstCompactedTailLOD = ComputeFirstCompactedTailLOD();
		const int32 ValidNumLODsToSet = FMath::Min(InNumLODs, ComputeNumLODsForSize(ImageSize)); 

		const int32 NumOldLODsInTail = FMath::Max(0, NumLODs - FirstCompactedTailLOD);
		const int32 NumOldLODsNotInTail = FMath::Max(0, NumLODs - NumOldLODsInTail);

		const int32 NumNewLODsInTail = FMath::Max(0, ValidNumLODsToSet - FirstCompactedTailLOD);
		const int32 NumNewLODsNotInTail = FMath::Max(0, ValidNumLODsToSet - NumNewLODsInTail);

		Buffers.SetNum(NumNewLODsNotInTail + static_cast<int32>(NumNewLODsInTail > 0));

		const int32 BlockSizeX = GetImageFormatData(ImageFormat).PixelsPerBlockX;
		const int32 BlockSizeY = GetImageFormatData(ImageFormat).PixelsPerBlockY;
		const int32 BytesPerBlock = GetImageFormatData(ImageFormat).BytesPerBlock;

		// Do not allocate memory for formats like RLE that are not block based.
		if (BlockSizeX == 0 || BlockSizeY == 0 || BytesPerBlock == 0)
		{
			NumLODs = ValidNumLODsToSet;
			return;
		}

		FImageSize LODDims = ComputeLODSize(ImageSize, NumOldLODsNotInTail);
		// Update non tail buffers.
		{		
			const int32 NumAddedLODsNotInTail = FMath::Max(0, NumNewLODsNotInTail - NumOldLODsNotInTail);
			
			for (int32 L = 0; L < NumAddedLODsNotInTail; ++L)
			{
				const int32 BlocksX = FMath::DivideAndRoundUp<int32>(LODDims.X, BlockSizeX);
				const int32 BlocksY = FMath::DivideAndRoundUp<int32>(LODDims.Y, BlockSizeY);

				const int32 DataSize = BlocksX * BlocksY * BytesPerBlock;
				InitInternalArray(L + NumOldLODsNotInTail, DataSize, InitType);

				LODDims = FImageSize(
						FMath::DivideAndRoundUp<uint16>(LODDims.X, 2),
						FMath::DivideAndRoundUp<uint16>(LODDims.Y, 2));
			}
		}
		// Update tail buffer.
		if (NumNewLODsInTail > 0)
		{	
			const int32 NumAddedLODsInTail = FMath::Max(0, NumNewLODsInTail - NumOldLODsInTail);
			
			int32 TailBufferSize = 0;
			for (int32 L = 0; L < NumNewLODsInTail; ++L)
			{
				const int32 BlocksX = FMath::DivideAndRoundUp<int32>(LODDims.X, BlockSizeX);
				const int32 BlocksY = FMath::DivideAndRoundUp<int32>(LODDims.Y, BlockSizeY);

				TailBufferSize += BlocksX * BlocksY * BytesPerBlock;
				
				CompactedTailOffsets[L] = TailBufferSize;

				LODDims = FImageSize(
						FMath::DivideAndRoundUp<uint16>(LODDims.X, 2),
						FMath::DivideAndRoundUp<uint16>(LODDims.Y, 2));
			}

			FImageArray& TailBuffer = Buffers.Last();
			TailBuffer.SetNumUninitialized(TailBufferSize);

			if (NumAddedLODsInTail > 0 && InitType == EInitializationType::Black)
			{
				const int32 FirstAddedLODInTailOffset = NumOldLODsInTail == 0 ? 0 : CompactedTailOffsets[NumOldLODsInTail - 1];
				InitViewToBlack(MakeArrayView(TailBuffer.GetData() + FirstAddedLODInTailOffset, TailBuffer.Num()), ImageFormat);
			}

			// Fix the unused tail offsets, this is important to make sure serialized data is deterministic.
			const int32 UnusedTailOffsetValue = CompactedTailOffsets[NumNewLODsInTail - 1];  
			for (int32 I = NumNewLODsInTail; I < NumLODsInCompactedTail; ++I)
			{
				CompactedTailOffsets[I] = UnusedTailOffsetValue;
			}
		}

		NumLODs = ValidNumLODsToSet;
	}

	void FImageDataStorage::DropLODs(int32 NumLODsToDrop)
	{
		check(!IsVoid());
		
		if (NumLODsToDrop >= NumLODs)
		{
			//check(false);
			Buffers.Empty();
			ImageSize = FImageSize(0, 0);
			NumLODs = 0;

			return;
		}

		const int32 FirstCompactedTailLOD = ComputeFirstCompactedTailLOD();
		const int32 NumLODsToDropInTail = FMath::Max(0, NumLODsToDrop - FirstCompactedTailLOD); 

		for (int32 DestBufferIndex = 0, BufferIndex = NumLODsToDrop - NumLODsToDropInTail; 
			 BufferIndex < FirstCompactedTailLOD; 
			 ++DestBufferIndex, ++BufferIndex)
		{
			Buffers[DestBufferIndex] = MoveTemp(Buffers[BufferIndex]);
		}
		
		if (NumLODsToDropInTail)
		{
			int32 TailBufferOffset = NumLODsToDropInTail == 0 ? 0 : CompactedTailOffsets[NumLODsToDropInTail - 1];

			FImageArray& TailBuffer = Buffers.Last();

			int32 NewBufferSize = TailBuffer.Num() - TailBufferOffset; 
			FMemory::Memmove(TailBuffer.GetData(), TailBuffer.GetData() + TailBufferOffset, NewBufferSize);
			TailBuffer.SetNum(NewBufferSize);

			int32 NumLODsInTail = FMath::Max(0, NumLODs - FirstCompactedTailLOD);
			int32 OffsetIndex = NumLODsToDropInTail;

			for (int32 DestOffsetsIndex = 0; OffsetIndex < NumLODsInTail; ++DestOffsetsIndex, ++OffsetIndex)
			{
				CompactedTailOffsets[DestOffsetsIndex] = CompactedTailOffsets[OffsetIndex]; 
			}

			// Fix the unused tail offsets, this is important to make sure serialized data is deterministic.
			const uint32 UnusedTailValue = NumLODsInTail - 1 <= 0 ? 0 : CompactedTailOffsets[NumLODsInTail - 1];
			for (; OffsetIndex < NumLODsInCompactedTail; ++OffsetIndex)
			{
				CompactedTailOffsets[OffsetIndex] = UnusedTailValue;
			}
		}

		// Update metadata.
		NumLODs = FMath::Max(0, NumLODs - NumLODsToDrop);
		
		for (int32 I = 0; I < NumLODsToDrop; ++I)
		{
			ImageSize = FImageSize(
					FMath::DivideAndRoundUp<uint16>(ImageSize.X, 2),
					FMath::DivideAndRoundUp<uint16>(ImageSize.Y, 2));
		}
	}

	void FImageDataStorage::ResizeLOD(int32 LODIndex, int32 NewSizeBytes)
	{
		check(!IsVoid());
		check(NewSizeBytes >= 0);

		const int32 FirstCompactedTailLOD = ComputeFirstCompactedTailLOD();
		FImageArray& Buffer = GetInternalArray(LODIndex);

		if (LODIndex < FirstCompactedTailLOD)
		{
			Buffer.SetNum(NewSizeBytes);
			return;
		}

		check(&Buffer == &Buffers.Last());

		// Compacted tail resize.
		const int32 LODSize = GetLOD(LODIndex).Num();
		const int32 OldBufferSize = Buffer.Num();

		const int32 SizeDifference = NewSizeBytes - LODSize;
		const int32 NewBufferSize = OldBufferSize + SizeDifference;

		// If the buffer is growing we need to resize before moving the content. 
		if (SizeDifference > 0)
		{
			Buffer.SetNum(NewBufferSize);
		}

		// Move content and update tail offsets.
		const int32 LODIndexInTail = LODIndex - FirstCompactedTailLOD;
		
		// Last LOD does not need to move the content.
		if (LODIndex < NumLODs - 1) 
		{
			const int32 LODEndOffset = CompactedTailOffsets[LODIndexInTail];

			FMemory::Memmove(
					Buffer.GetData() + LODEndOffset + SizeDifference, 
					Buffer.GetData() + LODEndOffset,
					OldBufferSize - LODEndOffset);
		}

		// If the buffer is shrinking we need to resize after moving the content. 
		if (SizeDifference < 0)
		{
			Buffer.SetNum(NewBufferSize);
		}

		// Tail buffer offsets are represented with uint16 for compactness, check any overflow.
		check(Buffer.Num() < TNumericLimits<uint16>::Max());

		// Keep offset updated even if there is no LOD so that serialization can be deterministic.
		for (int32 I = LODIndexInTail; I < NumLODsInCompactedTail; ++I)
		{
			check(int32(CompactedTailOffsets[I]) + SizeDifference >= 0); 
			CompactedTailOffsets[I] += SizeDifference; 	
		}
	}

	void FImageDataStorage::Serialise(OutputArchive& Arch) const
	{
		int32 Version = 0;
		Arch << Version;
		
		Arch << ImageSize.X;
		Arch << ImageSize.Y;
		Arch << ImageFormat;
		Arch << NumLODs;

		Arch << Buffers.Num();

		for (const FImageArray& Buffer : Buffers)
		{
			Arch << Buffer;
		}
	
		Arch << CompactedTailOffsets.Num();
		Arch << CompactedTailOffsets;
	}

	void FImageDataStorage::Unserialise(InputArchive& Arch)
	{
		int32 Version;
		Arch >> Version;

		check(Version == 0);
	
		Arch >> ImageSize.X;
		Arch >> ImageSize.Y;
		Arch >> ImageFormat;
		Arch >> NumLODs;

		int32 BuffersNum = 0;
		Arch >> BuffersNum;

		Buffers.SetNum(BuffersNum);

		for (int32 I = 0; I < BuffersNum; ++I)
		{
			Arch >> Buffers[I];
		}

		int32 NumTailOffsets = 0;
		Arch >> NumTailOffsets;
		check(NumTailOffsets == CompactedTailOffsets.Num());
		
		Arch >> CompactedTailOffsets;
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compression/OodleDataCompressionUtil.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace FOodleCompressedArray
{
	bool CORE_API CompressData(TArray<uint8>& OutCompressed, const void* InData, int32 InDataSize, FOodleDataCompression::ECompressor InCompressor, FOodleDataCompression::ECompressionLevel InLevel)
	{
		if (InDataSize < 0 || InData == nullptr)
		{
			return false;
		}

		// For 32 bit sizes we only ever need 32 bits.
		int32 HeaderSizeNeeded = sizeof(int32) * 2;

		// Size the array so that it fits our header and the memory oodle requires to do the work.
		int32 CompSizeNeeded = IntCastChecked<int32>(FOodleDataCompression::CompressedBufferSizeNeeded(InDataSize));
		OutCompressed.SetNum( IntCastChecked<int32>( HeaderSizeNeeded + CompSizeNeeded) );

		// We compress in to the buffer past our header, then write the header below.
		void* CompPtr = OutCompressed.GetData() + HeaderSizeNeeded;
		int32 CompressedSize = IntCastChecked<int32>(FOodleDataCompression::CompressParallel(CompPtr, CompSizeNeeded, InData, InDataSize, InCompressor, InLevel));
		if ( CompressedSize <= 0 )
		{
			// Probably a bad parameter.
			OutCompressed.Empty();
			return false;
		}
		
		int32* Sizes = (int32*)OutCompressed.GetData();

		Sizes[0] = InDataSize;
		Sizes[1] = CompressedSize;

#if !PLATFORM_LITTLE_ENDIAN
		Sizes[0] = BYTESWAP_ORDER32(Sizes[0]);
		Sizes[1] = BYTESWAP_ORDER32(Sizes[1]);
#endif

		// Trim off the end working space Oodle needed to do the compress work.
		OutCompressed.SetNum( IntCastChecked<int32>(CompressedSize + HeaderSizeNeeded) , EAllowShrinking::No);
		return true;
	}
	bool CORE_API CompressData64(TArray64<uint8>& OutCompressed, const void* InData, int64 InDataSize, FOodleDataCompression::ECompressor InCompressor, FOodleDataCompression::ECompressionLevel InLevel)
	{
		if (InDataSize < 0 || InData == nullptr)
		{
			return false;
		}

		// for large sizes use 64 bit ints for our header
		int32 HeaderSizeNeeded = sizeof(int32) * 2;
		if (InDataSize >= (1U << 31))
		{
			HeaderSizeNeeded = sizeof(int64) * 2;
		}

		// Size the array so that it fits our header and the memory oodle requires to do the work.		
		int64 WorkingSizeNeeded = FOodleDataCompression::CompressedBufferSizeNeeded(InDataSize);
		OutCompressed.SetNum(HeaderSizeNeeded + WorkingSizeNeeded);

		// We compress in to the buffer past our header, then write the header below.
		void* CompPtr = OutCompressed.GetData() + HeaderSizeNeeded;
		int64 CompressedSize = FOodleDataCompression::CompressParallel(CompPtr, WorkingSizeNeeded, InData, InDataSize, InCompressor, InLevel);
		if (CompressedSize <= 0)
		{
			// Probably a bad parameter.
			OutCompressed.Empty();
			return false;
		}

		if (HeaderSizeNeeded == sizeof(int64)*2)
		{
			uint64* Sizes = (uint64*)OutCompressed.GetData();

			Sizes[0] = (uint64)InDataSize | (1ULL << 63);
			Sizes[1] = (uint64)CompressedSize;

#if !PLATFORM_LITTLE_ENDIAN
			Sizes[0] = BYTESWAP_ORDER64(Sizes[0]);
			Sizes[1] = BYTESWAP_ORDER64(Sizes[1]);
#endif
		}
		else
		{
			// technically we checked the high bit for the decompressed size, when we're switching
			// on the compressed size. This should be always less, so this should never happen..
			// but if it does, we need to know.
			check(CompressedSize < (1U << 31));

			// 32 bit sizes
			int32* Sizes = (int32*)OutCompressed.GetData();

			Sizes[0] = (int32)InDataSize;
			Sizes[1] = (int32)CompressedSize;

#if !PLATFORM_LITTLE_ENDIAN
			Sizes[0] = BYTESWAP_ORDER32(Sizes[0]);
			Sizes[1] = BYTESWAP_ORDER32(Sizes[1]);
#endif
		}

		// Trim off the end working space Oodle needed to do the compress work.
		OutCompressed.SetNum(CompressedSize + HeaderSizeNeeded, EAllowShrinking::No);
		return true;
	}

	bool CORE_API DecompressToExistingBuffer(void* InDestinationBuffer, int64 InDestinationBufferSize, TArray<uint8> const& InCompressed)
	{
		int32 DecompressedSize, CompressedSize;
		int32 OffsetToCompressedData = PeekSizes(InCompressed, CompressedSize, DecompressedSize);
		if (OffsetToCompressedData == 0)
		{
			return false;
		}
		// DestinationBuffer provided is not large enough to hold decompressed data :
		if ( DecompressedSize > InDestinationBufferSize )
		{
			return false;
		}

		// If we have a valid header, then if we don't have the actual data, it's corrupted data.
		if ( OffsetToCompressedData + CompressedSize > InCompressed.Num() )
		{
			return false;
		}

		return FOodleDataCompression::DecompressParallel(InDestinationBuffer, DecompressedSize, InCompressed.GetData() + OffsetToCompressedData, CompressedSize);
	}
	bool CORE_API DecompressToExistingBuffer64(void* InDestinationBuffer, int64 InDestinationBufferSize, TArray64<uint8> const& InCompressed)
	{
		int64 DecompressedSize, CompressedSize;
		int32 OffsetToCompressedData = PeekSizes64(InCompressed, CompressedSize, DecompressedSize);
		if (OffsetToCompressedData == 0)
		{
			return false;
		}
		// DestinationBuffer provided is not large enough to hold decompressed data :
		if ( DecompressedSize > InDestinationBufferSize )
		{
			return false;
		}
		
		// If we have a valid header, then if we don't have the actual data, it's corrupted data.
		if ( OffsetToCompressedData + CompressedSize > InCompressed.Num() )
		{
			return false;
		}

		return FOodleDataCompression::DecompressParallel(InDestinationBuffer, DecompressedSize, InCompressed.GetData() + OffsetToCompressedData, CompressedSize);
	}

	bool CORE_API DecompressToAllocatedBuffer(void*& OutDestinationBuffer, int32& OutDestinationBufferSize, TArray<uint8> const& InCompressed)
	{
		int32 DecompressedSize, CompressedSize;
		int32 OffsetToCompressedData = PeekSizes(InCompressed, CompressedSize, DecompressedSize);
		if (OffsetToCompressedData == 0)
		{
			return false;
		}

		// If we have a valid header, then if we don't have the actual data, it's corrupted data.
		check(OffsetToCompressedData + CompressedSize <= InCompressed.Num());

		void* DestinationBuffer = FMemory::Malloc(DecompressedSize);
		if (DestinationBuffer == 0)
		{
			return false;
		}

		OutDestinationBufferSize = DecompressedSize;
		OutDestinationBuffer = DestinationBuffer;

		if (FOodleDataCompression::DecompressParallel(DestinationBuffer, DecompressedSize, InCompressed.GetData() + OffsetToCompressedData, CompressedSize) == false)
		{
			FMemory::Free(DestinationBuffer);
			OutDestinationBuffer = 0;
			return false;
		}

		return true;
	}
	bool CORE_API DecompressToAllocatedBuffer64(void*& OutDestinationBuffer, int64& OutDestinationBufferSize, TArray64<uint8> const& InCompressed)
	{
		int64 DecompressedSize, CompressedSize;
		int32 OffsetToCompressedData = PeekSizes64(InCompressed, CompressedSize, DecompressedSize);
		if (OffsetToCompressedData == 0)
		{
			return false;
		}

		// If we have a valid header, then if we don't have the actual data, it's corrupted data.
		check(OffsetToCompressedData + CompressedSize <= InCompressed.Num());

		void* DestinationBuffer = FMemory::Malloc(DecompressedSize);
		if (DestinationBuffer == 0)
		{
			return false;
		}

		OutDestinationBufferSize = DecompressedSize;
		OutDestinationBuffer = DestinationBuffer;

		if (FOodleDataCompression::DecompressParallel(DestinationBuffer, DecompressedSize, InCompressed.GetData() + OffsetToCompressedData, CompressedSize) == false)
		{
			FMemory::Free(DestinationBuffer);
			OutDestinationBuffer = 0;
			return false;
		}

		return true;
	}
};

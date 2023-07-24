// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCompression.h"
#include <zstd.h>

namespace unsync {

FBuffer
Compress(const uint8* Data, uint64 DataSize, int ZstdCompressionLevel)
{
	FBuffer Result;
	if (DataSize)
	{
		Result.Resize(ZSTD_compressBound(DataSize));

		uint64 CompressedSize = ZSTD_compress(&Result[0], Result.Size(), Data, DataSize, ZstdCompressionLevel);
		if (ZSTD_isError(CompressedSize))
		{
			const char* ZstdError = ZSTD_getErrorName(CompressedSize);
			UNSYNC_ERROR(L"ZSTD_compress failed: %hs", ZstdError);
		}
		Result.Resize(CompressedSize);
	}
	return Result;
}

bool
Decompress(const uint8* InputData, uint64 InputDataSize, uint8* OutputData, uint64 OutputDataSize)
{
	uint64 DecompressedSizeExpected = ZSTD_getDecompressedSize(InputData, InputDataSize);
	if (OutputDataSize >= DecompressedSizeExpected)
	{
		uint64 DecompressedSizeActual = ZSTD_decompress(OutputData, OutputDataSize, InputData, InputDataSize);
		return DecompressedSizeActual == OutputDataSize;
	}

	return false;
}

FBuffer
Decompress(const uint8* Data, uint64 DataSize)
{
	uint64	DecompressedSizeExpected = ZSTD_getDecompressedSize(Data, DataSize);
	FBuffer Result(DecompressedSizeExpected);
	uint64	DecompressedSizeActual = ZSTD_decompress(&Result[0], DecompressedSizeExpected, Data, DataSize);
	if (DecompressedSizeExpected != DecompressedSizeActual)
	{
		Result.Clear();
	}
	return Result;
}

FBuffer
Decompress(const FBuffer& Buffer)
{
	return Decompress(Buffer.Data(), Buffer.Size());
}

}  // namespace unsync

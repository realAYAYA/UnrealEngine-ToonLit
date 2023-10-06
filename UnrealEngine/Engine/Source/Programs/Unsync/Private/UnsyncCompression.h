// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncBuffer.h"
#include "UnsyncUtil.h"

namespace unsync {

FBuffer Compress(const uint8* Data, uint64 DataSize, int ZstdCompressionLevel = 3);
FBuffer Decompress(const uint8* Data, uint64 DataSize);
FBuffer Decompress(const FBuffer& Buffer);
bool	Decompress(const uint8* InputData, uint64 InputDataSize, uint8* OutputData, uint64 OutputDataSize);

}  // namespace unsync

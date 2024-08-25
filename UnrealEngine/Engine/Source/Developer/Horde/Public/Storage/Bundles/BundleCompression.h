// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Memory/MemoryView.h"

/** Indicates the compression format in a bundle */
enum EBundleCompressionFormat : unsigned char
{
	/** Packets are uncompressed. */
	None = 0,

	/** LZ4 compression. */
	LZ4 = 1,

	/** Gzip compression. */
	Gzip = 2,

	/** Oodle compression (Kraken). */
	Oodle = 3,

	/** Brotli compression. */
	Brotli = 4,
};

/*
 * Utility methods for compressing bundles
 */
struct HORDE_API FBundleCompression
{
	/** Gets the maximum size of the buffer required to compress the given data. */
	static size_t GetMaxSize(EBundleCompressionFormat Format, const FMemoryView& Input);

	/** Compress a data packet. */
	static size_t Compress(EBundleCompressionFormat Format, const FMemoryView& Input, const FMutableMemoryView& Output);

	/** Decompress a packet of data. */
	static void Decompress(EBundleCompressionFormat Format, const FMemoryView& Input, const FMutableMemoryView& Output);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Storage/Blob.h"

/** Bundle version number */
enum class EBundleVersion : unsigned char
{
	/** Initial version number. */
	Initial = 0,

	/** Added the BundleExport.Alias property. */
	ExportAliases = 1,

	/** Back out change to include aliases. Will likely do this through an API rather than baked into the data. */
	RemoveAliases = 2,

	/** Use data structures which support in-place reading and writing. */
	InPlace = 3,

	/** Add import hashes to imported nodes. */
	ImportHashes = 4,

	/** Structure bundles as a sequence of self-contained packets (uses V2 code). */
	PacketSequence = 5,

	/** Last item in the enum. Used for Latest. */
	LatestPlusOne,

	/** The current version number. */
	Latest = (int)LatestPlusOne - 1,

	/** Last version using the V1 pipeline. */
	LatestV1 = ImportHashes,

	/** Last version using the V2 pipeline. */
	LatestV2 = Latest,
};

/**
 * General bundle properties
 */
struct HORDE_API FBundle
{
	static const FBlobType BlobType;
};

/**
 * Signature for a bundle
 */
struct HORDE_API FBundleSignature
{
	/** Number of bytes in a serialized signature. */
	static const int NumBytes = 8;

	/** Version number for the following file data. */
	EBundleVersion Version;

	/** Length of the packet, including the length of the signature itself. */
	size_t Length;

	FBundleSignature(EBundleVersion InVersion, size_t InLength);
	static FBundleSignature Read(const void* Data);
	void Write(void* Data);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IO/IoHash.h"
#include "../BlobType.h"
#include "../BlobHandle.h"
#include "../BlobWriter.h"
#include "../../SharedBufferView.h"
#include "Hash/Blake3.h"
#include "Hash/BuzHash.h"

/**
 * Options for chunking data
 */
struct HORDE_API FChunkingOptions
{
	static const FChunkingOptions Default;

	int32 MinChunkSize = 4 * 1024;
	int32 TargetChunkSize = 64 * 1024;
	int32 MaxChunkSize = 128 * 1024;
};

/**
 * Node containing a chunk of data
 */
class HORDE_API FChunkNode
{
public:
	static const FBlobType LeafBlobType;
	static const FBlobType InteriorBlobType;

	TArray<FBlobHandleWithHash> Children;
	FSharedBufferView Data;

	FChunkNode();
	FChunkNode(TArray<FBlobHandleWithHash> InChildren, FSharedBufferView InData);
	~FChunkNode();

	static FChunkNode Read(FBlob Blob);
	FBlobHandleWithHash Write(FBlobWriter& Writer) const;

	static FBlobHandleWithHash Write(FBlobWriter& Writer, const TArrayView<const FBlobHandleWithHash>& Children, FMemoryView Data);
};

/**
 * Utility class for reading data a data stream from a tree of chunk nodes
 */
class HORDE_API FChunkNodeReader
{
public:
	FChunkNodeReader(FBlob Blob);
	FChunkNodeReader(const FBlobHandle& Handle);
	~FChunkNodeReader();

	bool IsComplete() const;
	FMemoryView GetBuffer() const;
	void Advance(int32 Length);

	operator bool() const;

private:
	struct FStackEntry;
	TArray<FStackEntry> Stack;
};

/**
 * Utility class for writing new data to a tree of chunk nodes
 */
class HORDE_API FChunkNodeWriter
{
public:
	FChunkNodeWriter(FBlobWriter& InWriter, const FChunkingOptions& InOptions = FChunkingOptions::Default);
	~FChunkNodeWriter();

	void Write(FMemoryView Data);

	FBlobHandleWithHash Flush(FIoHash& OutStreamHash);

private:
	FBlobWriter& Writer;
	const FChunkingOptions& Options;

	FBuzHash RollingHash;
	uint32 Threshold;
	int32 NodeLength;

	FBlake3 StreamHasher;

	TArray<FBlobHandleWithHash> Nodes;

	void WriteNode();
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlobWriter.h"

/**
 * Writes data to a series of memory blocks
 */
class HORDE_API FChunkedBufferWriter
{
public:
	FChunkedBufferWriter(size_t InitialSize = 1024);
	FChunkedBufferWriter(const FChunkedBufferWriter&) = delete;
	FChunkedBufferWriter& operator = (const FChunkedBufferWriter&) = delete;
	virtual ~FChunkedBufferWriter();

	/** Reset the contents of this writer. */
	void Reset();

	/** Gets the current written length of this buffer. */
	size_t GetLength() const;

	/** Get a handle to part of the written buffer */
	FSharedBufferView Slice(size_t Offset, size_t Length) const;

	/** Gets a view over the underlying buffer. */
	TArray<FMemoryView> GetView() const;

	/** Copies the entire contents of this writer to another buffer. */
	void CopyTo(void* Buffer) const;

	/** Gets an output buffer for writing. */
	FMutableMemoryView GetOutputBuffer(size_t UsedSize, size_t DesiredSize);

	/** Increase the length of the written data. */
	void Advance(size_t Size);

private:
	struct FChunk;

	TArray<FChunk> Chunks;
	size_t WrittenLength;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <memory>

class FComputeBufferReader;
class FComputeBufferWriter;

struct FComputeBufferHeader;
struct FComputeBufferResources;

//
// Implements a ring buffer using shared memory, with one writer and multiple readers. 
// 
// Contention between the writer and readers is resolved by splitting the buffer into a number of
// fixed-size chunks, which can be appended to and read immediately, but only reset once consumed
// by all readers.
//
// If the read position is never advanced and only one chunk is used, the buffer functions as an
// append-only buffer.
//
class FComputeBuffer
{
public:
	// Maximum allowed number of readers
	static const int MaxReaders = 16;

	// Maximum allowed number of chunks
	static const int MaxChunks = 16;

	// Parameters for constructing a compute buffer
	struct FParams
	{
		const wchar_t* Name = nullptr; // When creating a buffer, we should not normally set the name. We can only *OPEN* buffers with a name. Usually we create buffers and attach them to channels.
		int NumChunks = 2;
		int ChunkLength = 64 * 1024;
		int NumReaders = 1;
	};

	FComputeBuffer();
	FComputeBuffer(const FComputeBuffer& Buffer);
	FComputeBuffer(FComputeBuffer&& Buffer) noexcept;
	~FComputeBuffer();

	// Creates a new buffer
	bool CreateNew(const FParams& Params);

	// Opens an existing shared memory buffer (typically from handles created in another process)
	bool OpenExisting(const wchar_t* Name);

	// Close the current buffer and release all allocated resources
	void Close();

	// Test if the buffer is currently open
	bool IsOpen() const { return Resources != nullptr; }

	// Gets a reference to the reader instance
	FComputeBufferReader& GetReader();
	const FComputeBufferReader& GetReader() const;

	// Gets a reference to the writer instance
	FComputeBufferWriter& GetWriter();
	const FComputeBufferWriter& GetWriter() const;

private:
	std::shared_ptr<FComputeBufferResources> Resources;
};

//
// Facilitates reading data from a compute buffer
//
class FComputeBufferReader
{
public:
	FComputeBufferReader();
	FComputeBufferReader(std::shared_ptr<FComputeBufferResources> Resources, int ReaderIdx);
	~FComputeBufferReader();

	// Test if the reader is valid
	bool IsValid() const { return Resources.get() != nullptr; }

	// Test whether the buffer has finished being written to (ie. MarkComplete() has been called by the writer) and all data has been read from it.
	bool IsComplete() const;

	// Move the read cursor forwards by the given number of bytes
	void AdvanceReadPosition(size_t Size);

	// Gets the amount of data that is ready to be read from a contiguous block of memory.
	size_t GetMaxReadSize() const;

	// Waits until the given amount of data has been read, and returns a pointer to it. Returns nullptr if the timeout expires, or
	// if the requested amount of data is not in a contiguous block of memory.
	const unsigned char* WaitToRead(size_t MinSize, int TimeoutMs = -1);

private:
	friend class FWorkerComputeSocket;

	std::shared_ptr<FComputeBufferResources> Resources;
	int ReaderIdx;

	const wchar_t* GetName() const;
};

//
// Facilitates writing data to a compute buffer
//
class FComputeBufferWriter
{
public:
	FComputeBufferWriter();
	FComputeBufferWriter(std::shared_ptr<FComputeBufferResources> Resources);
	~FComputeBufferWriter();

	// Test if the writer is valid
	bool IsValid() const { return Resources.get() != nullptr; }

	// Signal that we've finished writing to this buffer
	void MarkComplete();

	// Move the write cursor forward by the given number of bytes
	void AdvanceWritePosition(size_t Size);

	// Gets the length of the current write buffer.
	size_t GetMaxWriteSize() const;

	// Waits until a write buffer of the requested size is available, and returns a pointer to it. Returns nullptr if the 
	// timeout expires before enough data has been flushed to return a buffer of the given length.
	unsigned char* WaitToWrite(size_t MinSize, int TimeoutMs = -1);

private:
	friend class FWorkerComputeSocket;

	std::shared_ptr<FComputeBufferResources> Resources;

	const wchar_t* GetName() const;
	void SetAllReaderEvents();
};

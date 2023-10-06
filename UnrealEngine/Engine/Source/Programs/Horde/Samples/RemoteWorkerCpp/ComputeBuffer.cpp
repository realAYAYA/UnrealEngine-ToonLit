// Copyright Epic Games, Inc. All Rights Reserved.

#include <windows.h>
#include <assert.h>
#include <iostream>
#include <rpcdcep.h>
#include <assert.h>
#include "ComputeBuffer.h"

namespace
{
	enum class EWriteState : int
	{
		// Writer has moved to the next chunk
		MovedToNext = 0b00,

		// Chunk is still being appended to
		Writing = 0b10,

		// This chunk marks the end of the stream
		Complete = 0b11,
	};

	struct FChunkState
	{
		const long long Value;

		FChunkState(long long InValue)
			: Value(InValue)
		{
		}

		// Constructor
		FChunkState(EWriteState WriteState, int ReaderFlags, int Length)
			: Value((unsigned long long)Length | ((unsigned long long)ReaderFlags << 31) | ((unsigned long long)WriteState << 62))
		{
		}

		// Written length of this chunk
		int GetLength() const { return (int)(Value & 0x7fffffff); }

		// Set of flags which are set for each reader that still has to read from a chunk
		int GetReaderFlags() const { return (int)((Value >> 31) & 0x7fffffff); }

		// State of the writer
		EWriteState GetWriteState() const { return (EWriteState)((unsigned long long)Value >> 62); }

		// Test whether a particular reader is still referencing the chunk
		bool HasReaderFlag(int ReaderIdx) const { return (Value & (1ULL << (31 + ReaderIdx))) != 0; }
	};

	struct FChunkStatePtr
	{
	public:
		// Read the state value from memory
		FChunkState Get() const
		{
			return FChunkState(InterlockedCompareExchange64((volatile long long*)&Value, 0, 0));
		}

		// Append data to the chunk
		void Append(long long length)
		{
			InterlockedAdd64(&Value, length);
		}

		// Mark the chunk as being written to
		void StartWriting(int numReaders)
		{
			InterlockedExchange64(&Value, FChunkState(EWriteState::Writing, (1 << numReaders) - 1, 0).Value);
		}

		// Mark this chunk as the end of the stream
		void MarkComplete()
		{
			InterlockedOr64(&Value, FChunkState(EWriteState::Complete, 0, 0).Value);
		}

		// Clear the reader flag
		void FinishReading(int ReaderIdx)
		{
			InterlockedAnd64(&Value, ~FChunkState((EWriteState)0, 1 << ReaderIdx, 0).Value);
		}

		// Move to the next chunk
		void FinishWriting()
		{
			InterlockedAnd64(&Value, ~FChunkState(EWriteState::Writing, 0, 0).Value);
		}

	private:
		volatile long long Value;
	};

	struct FReaderState
	{
		const long long Value;

		FReaderState(long long InValue) : Value(InValue) { }
		FReaderState(int chunkIdx, int offset) : Value(((unsigned long long)chunkIdx << 32) | offset) { }

		int GetChunkIdx() const { return (int)(Value >> 32); }
		int GetOffset() const { return (int)(unsigned int)Value; }
	};

	struct FReaderStatePtr
	{
	public:
		FReaderState Get() const
		{
			return FReaderState(InterlockedCompareExchange64((volatile long long*)&Value, 0, 0));
		}

		void Set(FReaderState State)
		{
			InterlockedExchange64(&Value, State.Value);
		}

		void Advance(size_t Length)
		{
			InterlockedAdd64(&Value, Length);
		}

	private:
		volatile long long Value;
	};
}

struct FComputeBufferHeader
{
	int NumReaders;
	int NumChunks;
	int ChunkLength;
	int WriteChunkIdx;
	FChunkStatePtr Chunks[FComputeBuffer::MaxChunks];
	FReaderStatePtr Readers[FComputeBuffer::MaxReaders];
};

struct FComputeBufferResources
{
	static unsigned int Counter;

	wchar_t Name[260];

	FComputeBufferHeader* Header;
	unsigned char* ChunkPtrs[FComputeBuffer::MaxChunks];

	void* WriterEvent;
	void* ReaderEvents[FComputeBuffer::MaxReaders];

	FComputeBufferReader Reader;
	FComputeBufferWriter Writer;

	FComputeBufferResources(const wchar_t* Name)
		: MemoryMappedFile(nullptr)
		, Header(nullptr)
		, ChunkPtrs{ nullptr, }
		, RefCount(1)
		, WriterEvent(nullptr)
		, ReaderEvents{ nullptr, }
	{
		wcscpy_s(this->Name, Name);
	}

	~FComputeBufferResources()
	{
		if (Header != nullptr)
		{
			UnmapViewOfFile(Header);
			Header = nullptr;
		}

		if (MemoryMappedFile != nullptr)
		{
			CloseHandle(MemoryMappedFile);
			MemoryMappedFile = nullptr;
		}

		if (WriterEvent != nullptr)
		{
			CloseHandle(WriterEvent);
			WriterEvent = nullptr;
		}

		for (int ReaderIdx = 0; ReaderIdx < FComputeBuffer::MaxReaders; ReaderIdx++)
		{
			if (ReaderEvents[ReaderIdx] != nullptr)
			{
				CloseHandle(ReaderEvents[ReaderIdx]);
				ReaderEvents[ReaderIdx] = nullptr;
			}
		}
	}

	static std::shared_ptr<FComputeBufferResources> CreateNew(const FComputeBuffer::FParams& Params)
	{
		unsigned long long Capacity = sizeof(FComputeBufferHeader) + (Params.NumChunks * sizeof(unsigned int)) + (Params.NumChunks * Params.ChunkLength);

		const wchar_t* Name = Params.Name;

		wchar_t BaseNameBuffer[MAX_PATH];
		if (Name == nullptr)
		{
			DWORD Pid = GetCurrentProcessId();
			ULONGLONG TickCount = GetTickCount64();
			swprintf_s(BaseNameBuffer, L"Local\\COMPUTE_%u_%llu_%u", Pid, TickCount, InterlockedIncrement(&Counter));
			Name = BaseNameBuffer;
		}

		wchar_t NameBuffer[MAX_PATH];
		swprintf_s(NameBuffer, L"%s_M", Name);

		std::shared_ptr<FComputeBufferResources> Resources = std::make_shared<FComputeBufferResources>(Name);

		LARGE_INTEGER LargeInteger;
		LargeInteger.QuadPart = Capacity;

		Resources->MemoryMappedFile = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, LargeInteger.HighPart, LargeInteger.LowPart, NameBuffer);
		if (Resources->MemoryMappedFile == nullptr)
		{
			return nullptr;
		}

		Resources->Header = (FComputeBufferHeader*)MapViewOfFile(Resources->MemoryMappedFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if (Resources->Header == nullptr)
		{
			return nullptr;
		}

		memset(Resources->Header, 0, sizeof(*Header));
		Resources->Header->NumReaders = Params.NumReaders;
		Resources->Header->NumChunks = Params.NumChunks;
		Resources->Header->ChunkLength = Params.ChunkLength;

		swprintf_s(NameBuffer, L"%s_W", Name);
		Resources->WriterEvent = CreateEventW(NULL, TRUE, FALSE, NameBuffer);
		if (Resources->WriterEvent == nullptr)
		{
			return nullptr;
		}

		for (int ReaderIdx = 0; ReaderIdx < Resources->Header->NumReaders; ReaderIdx++)
		{
			swprintf_s(NameBuffer, L"%s_R%d", Name, ReaderIdx);
			Resources->ReaderEvents[ReaderIdx] = CreateEventW(NULL, TRUE, FALSE, NameBuffer);
			if (Resources->ReaderEvents[ReaderIdx] == nullptr)
			{
				return nullptr;
			}
		}

		Resources->Header->Chunks[0].StartWriting(Resources->Header->NumReaders);

		InitShared(Resources);
		return Resources;
	}

	static std::shared_ptr<FComputeBufferResources> OpenExisting(const wchar_t* Name)
	{
		wchar_t NameBuffer[MAX_PATH];
		swprintf_s(NameBuffer, L"%s_M", Name);

		std::shared_ptr<FComputeBufferResources> Resources = std::make_shared<FComputeBufferResources>(Name);

		Resources->MemoryMappedFile = OpenFileMappingW(FILE_MAP_ALL_ACCESS, TRUE, NameBuffer);
		if (Resources->MemoryMappedFile == nullptr)
		{
			return nullptr;
		}

		Resources->Header = (FComputeBufferHeader*)MapViewOfFile(Resources->MemoryMappedFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if (Resources->Header == nullptr)
		{
			return nullptr;
		}

		swprintf_s(NameBuffer, L"%s_W", Name);
		Resources->WriterEvent = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, TRUE, NameBuffer);
		if (Resources->WriterEvent == nullptr)
		{
			return nullptr;
		}

		for (int ReaderIdx = 0; ReaderIdx < Resources->Header->NumReaders; ReaderIdx++)
		{
			swprintf_s(NameBuffer, L"%s_R%d", Name, ReaderIdx);
			Resources->ReaderEvents[ReaderIdx] = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, TRUE, NameBuffer);
			if (Resources->ReaderEvents[ReaderIdx] == nullptr)
			{
				return nullptr;
			}
		}

		InitShared(Resources);
		return Resources;
	}

	void AddRef()
	{
		InterlockedIncrement(&RefCount);
	}

	void Release()
	{
		if (InterlockedDecrement(&RefCount) == 0)
		{
			delete this;
		}
	}

private:
	void* MemoryMappedFile;
	volatile long RefCount;

	static void InitShared(std::shared_ptr<FComputeBufferResources> Resources)
	{
		Resources->Reader = FComputeBufferReader(Resources, 0);
		Resources->Writer = FComputeBufferWriter(Resources);

		FComputeBufferHeader* Header = Resources->Header;

		unsigned char* NextPtr = (unsigned char*)(Header + 1);
		for (int ChunkIdx = 0; ChunkIdx < Header->NumChunks; ChunkIdx++)
		{
			Resources->ChunkPtrs[ChunkIdx] = NextPtr;
			NextPtr += Header->ChunkLength;
		}
	}
};

unsigned int FComputeBufferResources::Counter = 0;



//// FComputeBuffer /////

FComputeBuffer::FComputeBuffer()
	: Resources(nullptr)
{
}

FComputeBuffer::FComputeBuffer(const FComputeBuffer& Buffer)
	: FComputeBuffer()
{
	if (Buffer.Resources != nullptr)
	{
		Resources = Buffer.Resources;
		Resources->AddRef();
	}
}

FComputeBuffer::FComputeBuffer(FComputeBuffer&& Buffer) noexcept
	: Resources(Buffer.Resources)
{
	Buffer.Resources = nullptr;
}

FComputeBuffer::~FComputeBuffer()
{
	Close();
}

bool FComputeBuffer::CreateNew(const FParams& Params)
{
	Resources = FComputeBufferResources::CreateNew(Params);
	return Resources != nullptr;
}

bool FComputeBuffer::OpenExisting(const wchar_t* Name)
{
	Resources = FComputeBufferResources::OpenExisting(Name);
	return Resources != nullptr;
}

void FComputeBuffer::Close()
{
	Resources.reset();
}

FComputeBufferReader& FComputeBuffer::GetReader()
{
	return Resources->Reader;
}

const FComputeBufferReader& FComputeBuffer::GetReader() const
{
	return Resources->Reader;
}

FComputeBufferWriter& FComputeBuffer::GetWriter()
{
	return Resources->Writer;
}

const FComputeBufferWriter& FComputeBuffer::GetWriter() const
{
	return Resources->Writer;
}







//// FComputeBufferReader /////

FComputeBufferReader::FComputeBufferReader()
	: Resources(nullptr)
	, ReaderIdx(-1)
{
}

FComputeBufferReader::FComputeBufferReader(std::shared_ptr<FComputeBufferResources> Resources, int ReaderIdx)
	: Resources(std::move(Resources))
	, ReaderIdx(ReaderIdx)
{
}

FComputeBufferReader::~FComputeBufferReader()
{
}

bool FComputeBufferReader::IsComplete() const
{
	FComputeBufferHeader* Header = Resources->Header;

	FReaderState ReaderState = Header->Readers[ReaderIdx].Get();
	FChunkState ChunkState = Header->Chunks[ReaderState.GetChunkIdx()].Get();

	return ChunkState.GetWriteState() == EWriteState::Complete && ReaderState.GetOffset() == ChunkState.GetLength();
}

void FComputeBufferReader::AdvanceReadPosition(size_t Size)
{
	FComputeBufferHeader* Header = Resources->Header;
	Header->Readers[ReaderIdx].Advance(Size);
}

size_t FComputeBufferReader::GetMaxReadSize() const
{
	FComputeBufferHeader* Header = Resources->Header;

	const FReaderStatePtr& readerStatePtr = Header->Readers[ReaderIdx];
	FReaderState readerState = readerStatePtr.Get();

	const FChunkStatePtr& chunkStatePtr = Header->Chunks[readerState.GetChunkIdx()];
	FChunkState chunkState = chunkStatePtr.Get();

	if (chunkState.HasReaderFlag(ReaderIdx))
	{
		return chunkState.GetLength() - readerState.GetOffset();
	}
	else
	{
		return 0;
	}
}

const unsigned char* FComputeBufferReader::WaitToRead(size_t MinSize, int TimeoutMs)
{
	FComputeBufferHeader* Header = Resources->Header;
	for (; ; )
	{
		FReaderStatePtr& readerStatePtr = Header->Readers[ReaderIdx];
		FReaderState readerState = readerStatePtr.Get();

		FChunkStatePtr& chunkStatePtr = Header->Chunks[readerState.GetChunkIdx()];
		FChunkState chunkState = chunkStatePtr.Get();

		if (!chunkState.HasReaderFlag(ReaderIdx))
		{
			// Wait until the current chunk is readable
			ResetEvent(Resources->ReaderEvents[ReaderIdx]);
			if (!chunkState.HasReaderFlag(ReaderIdx) && WaitForSingleObject(Resources->ReaderEvents[ReaderIdx], TimeoutMs) == WAIT_TIMEOUT)
			{
				return nullptr;
			}
		}
		else if (readerState.GetOffset() + MinSize <= chunkState.GetLength())
		{
			// We have enough data in the chunk to be able to read a message
			return Resources->ChunkPtrs[readerState.GetChunkIdx()] + readerState.GetOffset();
		}
		else if (chunkState.GetWriteState() == EWriteState::Writing)
		{
			// Wait until there is more data in the chunk
			ResetEvent(Resources->ReaderEvents[ReaderIdx]);
			if (Header->Chunks[readerState.GetChunkIdx()].Get().Value == chunkState.Value && WaitForSingleObject(Resources->ReaderEvents[ReaderIdx], TimeoutMs) == WAIT_TIMEOUT)
			{
				return nullptr;
			}
		}
		else if (readerState.GetOffset() < chunkState.GetLength() || chunkState.GetWriteState() == EWriteState::Complete)
		{
			// Cannot read the requested amount of data from this chunk.
			return nullptr;
		}
		else if (chunkState.GetWriteState() == EWriteState::MovedToNext)
		{
			// Move to the next chunk
			chunkStatePtr.FinishReading(ReaderIdx);
			SetEvent(Resources->WriterEvent);

			int chunkIdx = readerState.GetChunkIdx() + 1;
			if (chunkIdx == Header->NumChunks)
			{
				chunkIdx = 0;
			}

			readerStatePtr.Set(FReaderState(chunkIdx, 0));
		}
		else
		{
			assert(false);
		}
	}
}

const wchar_t* FComputeBufferReader::GetName() const
{
	return Resources->Name;
}





//// FComputeBufferWriter /////

FComputeBufferWriter::FComputeBufferWriter()
	: Resources(nullptr)
{
}

FComputeBufferWriter::FComputeBufferWriter(std::shared_ptr<FComputeBufferResources> Resources)
	: Resources(std::move(Resources))
{
}

FComputeBufferWriter::~FComputeBufferWriter()
{
}

void FComputeBufferWriter::MarkComplete()
{
	FComputeBufferHeader* Header = Resources->Header;
	Header->Chunks[Header->WriteChunkIdx].MarkComplete();
	SetAllReaderEvents();
}

void FComputeBufferWriter::AdvanceWritePosition(size_t Size)
{
	FComputeBufferHeader* Header = Resources->Header;
	Header->Chunks[Header->WriteChunkIdx].Append(Size);
	SetAllReaderEvents();
}

size_t FComputeBufferWriter::GetMaxWriteSize() const
{
	FComputeBufferHeader* Header = Resources->Header;
	FChunkState Value = Header->Chunks[Header->WriteChunkIdx].Get();
	return Header->ChunkLength - Value.GetLength();
}

unsigned char* FComputeBufferWriter::WaitToWrite(size_t MinSize, int TimeoutMs)
{
	FComputeBufferHeader* Header = Resources->Header;
	assert(MinSize <= Header->ChunkLength);

	DWORD WaitParam = (TimeoutMs == -1) ? INFINITE : (unsigned int)TimeoutMs;
	for (; ; )
	{
		FChunkStatePtr WriteChunkStatePtr = Header->Chunks[Header->WriteChunkIdx];

		FChunkState ChunkState = WriteChunkStatePtr.Get();
		if (ChunkState.GetWriteState() == EWriteState::Writing)
		{
			int Length = ChunkState.GetLength();
			if (Length + MinSize <= Header->ChunkLength)
			{
				return Resources->ChunkPtrs[Header->WriteChunkIdx] + Length;
			}

			WriteChunkStatePtr.FinishWriting(); // STATE CHANGE
			SetAllReaderEvents();
		}

		int NextWriteChunkIdx = Header->WriteChunkIdx + 1;
		if (NextWriteChunkIdx == Header->NumChunks)
		{
			NextWriteChunkIdx = 0;
		}

		FChunkStatePtr NextWriteChunkStatePtr = Header->Chunks[NextWriteChunkIdx];
		while (NextWriteChunkStatePtr.Get().GetReaderFlags() != 0)
		{
			if (WaitForSingleObject(Resources->WriterEvent, WaitParam) == WAIT_TIMEOUT)
			{
				return nullptr;
			}
			ResetEvent(Resources->WriterEvent);
		}

		Header->WriteChunkIdx = NextWriteChunkIdx;

		NextWriteChunkStatePtr.StartWriting(Header->NumReaders);
	}
}

const wchar_t* FComputeBufferWriter::GetName() const
{
	return Resources->Name;
}

void FComputeBufferWriter::SetAllReaderEvents()
{
	FComputeBufferHeader* Header = Resources->Header;
	for (int Idx = 0; Idx < Header->NumReaders; Idx++)
	{
		SetEvent(Resources->ReaderEvents[Idx]);
	}
}


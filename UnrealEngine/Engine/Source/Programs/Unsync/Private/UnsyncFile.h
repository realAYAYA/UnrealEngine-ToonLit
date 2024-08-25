// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncBuffer.h"
#include "UnsyncUtil.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace unsync {

extern bool GForceBufferedFiles;

enum class EFileMode : uint32 {
	None	   = 0,

	Read	   = 1 << 0,
	Write	   = 1 << 1,
	Create	   = 1 << 2,
	Unbuffered = 1 << 3,

	// Extended modes
	IgnoreDryRun = 1 << 4,	// allow write operations even in dry run mode

	// Commonly used mode combinations
	ReadOnly		   = Read,
	ReadOnlyUnbuffered = Read | Unbuffered,
	CreateReadWrite	   = Read | Write | Create,
	CreateWriteOnly	   = Write | Create,

	// Masks
	CommonModeMask	 = Create | Read | Write | Unbuffered,
	ExtendedModeMask = ~CommonModeMask,
};
UNSYNC_ENUM_CLASS_FLAGS(EFileMode, uint32)

inline bool
IsReadOnly(EFileMode Mode)
{
	switch (Mode & EFileMode::CommonModeMask)
	{
		case EFileMode::ReadOnly:
		case EFileMode::ReadOnlyUnbuffered:
			return true;
		default:
			return false;
	}
}

inline bool
IsWriteOnly(EFileMode Mode)
{
	return (Mode & EFileMode::Read) == 0;
}

inline bool
IsReadable(EFileMode Mode)
{
	return (Mode & EFileMode::Read) != 0;
}

inline bool
IsWritable(EFileMode Mode)
{
	return (Mode & EFileMode::Write) != 0;
}

struct FIOBuffer
{
	static FIOBuffer Alloc(uint64 Size, const wchar_t* DebugName);
	FIOBuffer() = default;
	FIOBuffer(FIOBuffer&& Rhs);
	FIOBuffer(const FIOBuffer& Rhs) = delete;
	FIOBuffer& operator=(const FIOBuffer& Rhs) = delete;
	FIOBuffer& operator						   =(FIOBuffer&& Rhs);
	~FIOBuffer();

	uint8* GetData() const
	{
		UNSYNC_ASSERT(Canary == CANARY);
		return DataPtr;
	}
	uint64 GetSize() const
	{
		UNSYNC_ASSERT(Canary == CANARY);
		return DataSize;
	}

	void Clear();

	void SetDataRange(uint64 Offset, uint64 Size)
	{
		UNSYNC_ASSERT(Offset + Size <= MemorySize);
		DataPtr	 = MemoryPtr + Offset;
		DataSize = Size;
	}

	void* GetMemory() const
	{
		UNSYNC_ASSERT(Canary == CANARY);
		return MemoryPtr;
	}

	FBufferView GetBufferView() const { return FBufferView{GetData(), GetSize()}; }
	FMutBufferView GetMutBufferView() { return FMutBufferView{GetData(), GetSize()}; }

private:
	static constexpr uint64 CANARY = 0x67aced0423000de5ull;

	uint64		   Canary	  = CANARY;
	uint8*		   MemoryPtr  = nullptr;
	uint64		   MemorySize = 0;
	uint8*		   DataPtr	  = nullptr;
	uint64		   DataSize	  = 0;
	const wchar_t* DebugName  = nullptr;
};

inline std::shared_ptr<FIOBuffer>
MakeShared(FIOBuffer&& Buffer)
{
	return std::make_shared<FIOBuffer>(std::forward<FIOBuffer>(Buffer));
}

using IOCallback = std::function<void(FIOBuffer Buffer, uint64 SourceOffset, uint64 ReadSize, uint64 UserData)>;

struct FIOBase
{
	virtual ~FIOBase()		  = default;
	virtual void   FlushAll() = 0;
	virtual void   FlushOne() = 0;
	virtual uint64 GetSize()  = 0;
	virtual bool   IsValid()  = 0;
	virtual void   Close()	  = 0;
	virtual int32  GetError() = 0;
};

struct FIOReader : virtual FIOBase
{
	virtual uint64 Read(void* Dest, uint64 SourceOffset, uint64 Size) = 0;
	virtual bool   ReadAsync(uint64 SourceOffset, uint64 Size, uint64 UserData, IOCallback Callback)
	{
		FIOBuffer Buffer   = FIOBuffer::Alloc(Size, L"FIOReader::ReadAsync");
		uint64	  ReadSize = Read(Buffer.GetData(), SourceOffset, Size);
		Callback(std::move(Buffer), SourceOffset, ReadSize, UserData);
		return true;
	}
};

struct FIOWriter : virtual FIOBase
{
	virtual uint64 Write(const void* Data, uint64 DestOffset, uint64 Size) = 0;
};

struct FIOReaderWriter : FIOReader, FIOWriter
{
};

#if UNSYNC_PLATFORM_WINDOWS
struct FWindowsFile : FIOReaderWriter
{
	FWindowsFile(const FPath& Filename, EFileMode Mode = EFileMode::ReadOnly, uint64 InSize = 0);
	~FWindowsFile();

	// IOBase
	virtual void   FlushAll() override;
	virtual void   FlushOne() override;
	virtual uint64 GetSize() override { return FileSize; }
	virtual bool   IsValid() override;
	virtual void   Close() override;
	virtual int32  GetError() override { return LastError; }

	// IORead
	virtual uint64 Read(void* Dest, uint64 SourceOffset, uint64 ReadSize) override;
	virtual bool   ReadAsync(uint64 SourceOffset, uint64 Size, uint64 UserData, IOCallback Callback) override;

	// IOWrite
	virtual uint64 Write(const void* InData, uint64 DestOffset, uint64 WriteSize) override;

	uint64 FileSize	  = 0;
	HANDLE FileHandle = INVALID_HANDLE_VALUE;
	int32  LastError  = 0;

	static constexpr uint32 NUM_QUEUES					 = 16;
	HANDLE					OverlappedEvents[NUM_QUEUES] = {};
	struct Command
	{
		OVERLAPPED Overlapped	= {};
		uint64	   SourceOffset = 0;
		uint64	   ReadSize		= 0;
		uint64	   UserData		= 0;
		bool	   bActive		= false;
		FIOBuffer  Buffer;
		IOCallback Callback = {};
	};
	Command Commands[NUM_QUEUES] = {};

	FPath Filename;

	static constexpr uint32 UNBUFFERED_READ_ALIGNMENT = 4096;

private:

	// All internal methods expect the Mutex to be locked
	void   InternalFlushAll();
	uint32 CompleteReadCommand(Command& Cmd);
	bool   OpenFileHandle(EFileMode InMode);

private:
	EFileMode Mode;

	std::mutex Mutex;
};
using FNativeFile = FWindowsFile;
#endif	// UNSYNC_PLATFORM_WINDOWS

#if UNSYNC_PLATFORM_UNIX
struct FUnixFile : FIOReaderWriter
{
	FUnixFile(const FPath& InFilename, EFileMode InMode = EFileMode::ReadOnly, uint64 InSize = 0);
	~FUnixFile();

	// IOBase
	virtual void   FlushAll() override;
	virtual void   FlushOne() override;
	virtual uint64 GetSize() override { return FileSize; }
	virtual bool   IsValid() override { return FileHandle != nullptr; }
	virtual void   Close() override;
	virtual int32  GetError() override { return LastError; }

	// IORead
	virtual uint64 Read(void* Dest, uint64 SourceOffset, uint64 ReadSize) override;
	virtual bool   ReadAsync(uint64 SourceOffset, uint64 ReadSize, uint64 UserData, IOCallback Callback) override;

	// IOWrite
	virtual uint64 Write(const void* Indata, uint64 DestOffset, uint64 WriteSize) override;

	uint64 FileSize	 = 0;
	int32  LastError = 0;

	FPath Filename;

	static constexpr uint32 UNBUFFERED_READ_ALIGNMENT = 4096;

private:
	bool OpenFileHandle(EFileMode InMode);

private:
	EFileMode Mode;
	FILE*	  FileHandle	 = nullptr;
	int		  FileDescriptor = 0;
};
using FNativeFile = FUnixFile;
#endif	// UNSYNC_PLATFORM_UNIX

struct FVectorStreamOut
{
	FVectorStreamOut(FBuffer& Output) : Output(Output) {}

	void Write(const void* Data, uint64 Size)
	{
		const uint8* DataBytes = reinterpret_cast<const uint8*>(Data);
		Output.Append(DataBytes, Size);
	}

	template<typename T>
	void WriteT(const T& Data)
	{
		Write(&Data, sizeof(Data));
	}

	void WriteString(const std::string& S)
	{
		uint32 Len = uint32(S.length());
		WriteT(Len);
		Write(S.c_str(), Len);
	}

	FBuffer& Output;
};

struct FMemReader : FIOReader
{
	FMemReader(const FBuffer& Buffer) : FMemReader(Buffer.Data(), Buffer.Size()) {}
	FMemReader(const uint8* InData, uint64 InDataSize);

	// IOBase
	virtual void   FlushAll() override {}
	virtual void   FlushOne() override {}
	virtual uint64 GetSize() override { return Size; }
	virtual bool   IsValid() override { return Data != nullptr; }
	virtual void   Close() override
	{
		Size = 0;
		Data = nullptr;
	}
	virtual int32 GetError() override { return 0; }

	// IORead
	virtual uint64 Read(void* Dest, uint64 SourceOffset, uint64 ReadSize) override;

	const uint8* Data = nullptr;
	uint64		 Size = 0;
};

#if UNSYNC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4250)	 // 'FMemReaderWriter': inherits 'FMemReader::FMemReader::flush_one' via dominance
#endif // UNSYNC_COMPILER_MSVC
struct FMemReaderWriter : FMemReader, FIOReaderWriter
{
	FMemReaderWriter(uint8* InData, uint64 InDataSize);
	FMemReaderWriter(FMutBufferView Buffer) : FMemReaderWriter(Buffer.Data, Buffer.Size) {}

	// IOBase
	virtual void Close() override
	{
		FMemReader::Close();
		DataRw = nullptr;
	}

	// IOWrite
	virtual uint64 Write(const void* InData, uint64 DestOffset, uint64 WriteSize) override;

	// IORead
	virtual uint64 Read(void* Dest, uint64 SourceOffset, uint64 ReadSize) override
	{
		return FMemReader::Read(Dest, SourceOffset, ReadSize);
	}

	uint8* DataRw = nullptr;
};
#if UNSYNC_COMPILER_MSVC
#pragma warning(pop)
#endif // UNSYNC_COMPILER_MSVC

struct FNullReaderWriter : FIOReaderWriter
{
	FNullReaderWriter(uint64 InDataSize) : DataSize(InDataSize) {}

	// IOBase
	virtual void   FlushAll() override{};
	virtual void   FlushOne() override{};
	virtual uint64 GetSize() override { return DataSize; }
	virtual bool   IsValid() override { return true; }
	virtual void   Close() override{};
	virtual int32  GetError() override { return 0; }

	// IORead
	virtual uint64 Read(void* Dest, uint64 SourceOffset, uint64 ReadSize) override
	{
		memset(Dest, 0, ReadSize);
		return ReadSize;
	}
	virtual bool ReadAsync(uint64 SourceOffset, uint64 Size, uint64 UserData, IOCallback Callback) override
	{
		FIOBuffer Buffer   = FIOBuffer::Alloc(Size, L"NullReaderWriter::ReadAsync");
		uint64	  ReadSize = Read(Buffer.GetData(), SourceOffset, Size);
		Callback(std::move(Buffer), SourceOffset, ReadSize, UserData);
		return true;
	}

	// IOWrite
	virtual uint64 Write(const void* InData, uint64 DestOffset, uint64 WriteSize) override { return WriteSize; }

	uint64 DataSize;
};

struct FDeferredOpenReader : FIOReader
{
	using FOpenCallback = std::function<std::unique_ptr<FIOReader>()>;

	FDeferredOpenReader(FOpenCallback InOpenCallback) : OpenCallback(InOpenCallback) {}

	// IOBase
	virtual void   FlushAll() override { GetOrOpenInner()->FlushAll(); }
	virtual void   FlushOne() override { GetOrOpenInner()->FlushOne(); }
	virtual uint64 GetSize() override { return GetOrOpenInner()->GetSize(); }
	virtual bool   IsValid() override { return GetOrOpenInner()->IsValid(); }
	virtual void   Close() override { GetOrOpenInner()->Close(); }
	virtual int32  GetError() override { return GetOrOpenInner()->GetError(); }

	// IORead
	virtual uint64 Read(void* Dest, uint64 SourceOffset, uint64 ReadSize) override
	{
		return GetOrOpenInner()->Read(Dest, SourceOffset, ReadSize);
	}
	virtual bool ReadAsync(uint64 SourceOffset, uint64 Size, uint64 UserData, IOCallback Callback) override
	{
		return GetOrOpenInner()->ReadAsync(SourceOffset, Size, UserData, Callback);
	}

	FIOReader* GetOrOpenInner()
	{
		if (!Inner)
		{
			Inner = OpenCallback();
		}
		return Inner.get();
	}

	FOpenCallback			   OpenCallback;
	std::unique_ptr<FIOReader> Inner;
};

struct FIOReaderStream
{
	FIOReaderStream(FIOReader& InInner) : Inner(InInner) {}

	uint64 Read(void* Dest, uint64 Size)
	{
		uint64 ReadBytes = Inner.Read(Dest, Offset, Size);
		Offset += ReadBytes;
		return ReadBytes;
	}

	void Seek(uint64 InOffset)
	{
		UNSYNC_ASSERT(InOffset < Inner.GetSize());
		Offset = InOffset;
	}

	uint64 Tell() const { return Offset; }

	bool IsValid() const { return Inner.IsValid(); }

	FIOReader& Inner;
	uint64	   Offset = 0;
};

FBuffer ReadFileToBuffer(const FPath& Filename);
bool	WriteBufferToFile(const FPath& Filename, const uint8* Data, uint64 Size, EFileMode FileMode = EFileMode::CreateWriteOnly);
bool	WriteBufferToFile(const FPath& Filename, const FBuffer& Buffer, EFileMode FileMode = EFileMode::CreateWriteOnly);
bool	WriteBufferToFile(const FPath& Filename, const std::string& Buffer, EFileMode FileMode = EFileMode::CreateWriteOnly);

struct FFileAttributes
{
	uint64 Mtime	  = 0;	// Windows file time (100ns ticks since 1601-01-01T00:00:00Z)
	uint64 Size		  = 0;
	bool   bDirectory = false;
	bool   bValid	  = false;
	bool   bReadOnly  = false;
};

struct FFileAttributeCache
{
	std::unordered_map<FPath::string_type, FFileAttributes> Map;

	const bool Exists(const FPath& Path) const;
};

inline bool
IsReadOnly(std::filesystem::perms Perms)
{
	return (Perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none;
}

FFileAttributes GetFileAttrib(const FPath& Path, FFileAttributeCache* AttribCache = nullptr);
FFileAttributes GetCachedFileAttrib(const FPath& Path, FFileAttributeCache& AttribCache);

bool			SetFileMtime(const FPath& Path, uint64 Mtime, bool bAllowInDryRun = false);
bool			SetFileReadOnly(const FPath& Path, bool ReadOnly);
bool			IsDirectory(const FPath& Path);
bool			PathExists(const FPath& Path);
bool			PathExists(const FPath& Path, std::error_code& OutErrorCode);
bool			CreateDirectories(const FPath& Path);
bool			FileRename(const FPath& From, const FPath& To, std::error_code& OutErrorCode);
bool			FileCopy(const FPath& From, const FPath& To, std::error_code& OutErrorCode);
bool			FileCopyOverwrite(const FPath& From, const FPath& To, std::error_code& OutErrorCode);
bool			FileRemove(const FPath& Path, std::error_code& OutErrorCode);
FPath			GetRelativePath(const FPath& Path, const FPath& Base);

// Returns number of bytes that can be written to the given path.
// Returns ~0ull if the available space could not be determined.
uint64 GetAvailableDiskSpace(const FPath& Path);

std::filesystem::recursive_directory_iterator RecursiveDirectoryScan(const FPath& Path);

uint64 ToWindowsFileTime(const std::filesystem::file_time_type& T);
std::filesystem::file_time_type FromWindowsFileTime(uint64 Ticks);

struct FSyncFilter;
FFileAttributeCache CreateFileAttributeCache(const FPath& Root, const FSyncFilter* SyncFilter = nullptr);

// Returns extended absolute path of a form \\?\D:\verylongpath or \\?\UNC\servername\verylongpath
// Expects an absolute path input. Returns original path on non-Windows.
// https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
FPath MakeExtendedAbsolutePath(const FPath& InAbsolutePath);

// Removes `\\?\` or `\\?\UNC\` prefix from a given path.
// Returns original path on non-Windows.
FPathStringView RemoveExtendedPathPrefix(const FPath& InPath);

}  // namespace unsync

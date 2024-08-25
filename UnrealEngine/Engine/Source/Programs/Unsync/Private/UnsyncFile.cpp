// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncFile.h"
#include "UnsyncCore.h"
#include "UnsyncMemory.h"
#include "UnsyncThread.h"
#include "UnsyncThread.h"

#include <mutex>

#if UNSYNC_PLATFORM_UNIX
#	include <errno.h>
#	include <sys/stat.h>
#	include <sys/types.h>
#	include <unistd.h>
#endif	// UNSYNC_PLATFORM_UNIX

#if UNSYNC_PLATFORM_WINDOWS
UNSYNC_THIRD_PARTY_INCLUDES_START
#include <winioctl.h>
UNSYNC_THIRD_PARTY_INCLUDES_END
#endif // UNSYNC_PLATFORM_WINDOWS

namespace unsync {

bool GForceBufferedFiles = false;

// Windows epoch : 1601-01-01T00:00:00Z
// Unix epoch    : 1970-01-01T00:00:00Z
static constexpr uint64 SECONDS_BETWEEN_WINDOWS_AND_UNIX = 11'644'473'600ull;
static constexpr uint64 NANOS_PER_WINDOWS_TICK			 = 100ull;
static constexpr uint64 WINDOWS_TICKS_PER_SECOND		 = 1'000'000'000ull / NANOS_PER_WINDOWS_TICK;  // each tick is 100ns

// Returns extended absolute path of a form \\?\D:\verylongpath or \\?\UNC\servername\verylongpath
// Expects an absolute path input. Returns original path on non-Windows.
// https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
FPath
MakeExtendedAbsolutePath(const FPath& InAbsolutePath)
{
	if (InAbsolutePath.empty())
	{
		return FPath();
	}

#if UNSYNC_PLATFORM_WINDOWS
	UNSYNC_ASSERTF(InAbsolutePath.is_absolute(), L"Input path '%ls' must be absolute", InAbsolutePath.wstring().c_str());
	const std::wstring& InFilenameString = InAbsolutePath.native();
	if (InFilenameString.starts_with(L"\\\\?\\"))
	{
		return InAbsolutePath;
	}
	else if (InFilenameString.starts_with(L"\\\\"))
	{
		return std::wstring(L"\\\\?\\UNC\\") + InFilenameString.substr(2);
	}
	else
	{
		return std::wstring(L"\\\\?\\") + InFilenameString;
	}
#else // UNSYNC_PLATFORM_WINDOWS
	return InAbsolutePath;
#endif // UNSYNC_PLATFORM_WINDOWS
}

FPathStringView
RemoveExtendedPathPrefix(const FPath& InPath)
{
	FPathStringView InPathString = InPath.native();
#if UNSYNC_PLATFORM_WINDOWS
	if (InPathString.starts_with(L"\\\\?\\UNC\\"))
	{
		return InPathString.substr(8);
	}
	else if (InPathString.starts_with(L"\\\\?\\"))
	{
		return InPathString.substr(4);
	}
	else
	{
		return InPathString;
	}
#else // UNSYNC_PLATFORM_WINDOWS
	return InPathString;
#endif // UNSYNC_PLATFORM_WINDOWS
}

std::filesystem::file_time_type FromWindowsFileTime(uint64 Ticks)
{
	using FileTimeDuration = std::filesystem::file_time_type::duration;

	uint64 RawSeconds = Ticks / WINDOWS_TICKS_PER_SECOND;
	uint64 RawSubsecondTicks = Ticks - (RawSeconds * WINDOWS_TICKS_PER_SECOND);
	uint64 RawSubsecondNanos = RawSubsecondTicks * NANOS_PER_WINDOWS_TICK;

#if UNSYNC_PLATFORM_WINDOWS
	FileTimeDuration Seconds = std::chrono::duration_cast<FileTimeDuration>(std::chrono::seconds(RawSeconds));
#else	// UNSYNC_PLATFORM_WINDOWS
	FileTimeDuration Seconds = std::chrono::seconds(RawSeconds - SECONDS_BETWEEN_WINDOWS_AND_UNIX);
#endif	// UNSYNC_PLATFORM_WINDOWS

	FileTimeDuration SubsecondNanos = std::chrono::duration_cast<FileTimeDuration>(std::chrono::nanoseconds(RawSubsecondNanos));

	FileTimeDuration DurationFromNativeEpoch = Seconds + SubsecondNanos;

	std::filesystem::file_time_type Result(DurationFromNativeEpoch);

	return Result;
}

FPath
GetRelativePath(const FPath& Path, const FPath& Base)
{
	// Try a trivial case first, without touching the filesystem
	FPathStringView PathView = RemoveExtendedPathPrefix(Path);
	FPathStringView BaseView = RemoveExtendedPathPrefix(Base);

	FPathStringView PathViewRemainder = PathView.substr(BaseView.length());

	if (PathView.starts_with(BaseView) && PathViewRemainder.starts_with(FPath::preferred_separator))
	{
		FPathStringView RelativePath = PathView.substr(BaseView.length());
		while (RelativePath.starts_with(FPath::preferred_separator))
		{
			RelativePath = RelativePath.substr(1);
		}
		return FPath(RelativePath);
	}

	return {};
}

FFileAttributes GetCachedFileAttrib(const FPath& Path, FFileAttributeCache& AttribCache)
{
	FFileAttributes Result;

	FPath ExtendedPath = MakeExtendedAbsolutePath(Path);

	auto It = AttribCache.Map.find(ExtendedPath);
	if (It != AttribCache.Map.end())
	{
		Result = It->second;
	}

	return Result;
}

#if UNSYNC_PLATFORM_WINDOWS
inline uint64
MakeU64(FILETIME Ft)
{
	return MakeU64(Ft.dwHighDateTime, Ft.dwLowDateTime);
}

struct FCreateFileInfo
{
	DWORD FileAccess  = 0;
	DWORD Share		  = 0;
	DWORD Disposition = 0;
	DWORD Protection  = 0;
	DWORD MapAccess	  = 0;
	DWORD FileFlags	  = FILE_ATTRIBUTE_NORMAL;

	FCreateFileInfo(EFileMode Mode)
	{
		switch (Mode & EFileMode::CommonModeMask)
		{
			default:
			case EFileMode::ReadOnly:
			case EFileMode::ReadOnlyUnbuffered:
				FileAccess	= GENERIC_READ;
				Share		= FILE_SHARE_READ;
				Disposition = OPEN_EXISTING;
				Protection	= PAGE_READONLY;
				MapAccess	= FILE_MAP_READ;
				break;
			case EFileMode::CreateReadWrite:
			case EFileMode::CreateWriteOnly:
				UNSYNC_ASSERT(!GDryRun || EnumHasAnyFlags(Mode, EFileMode::IgnoreDryRun));
				FileAccess	= GENERIC_READ | GENERIC_WRITE;
				Share		= FILE_SHARE_WRITE;
				Disposition = CREATE_ALWAYS;
				Protection	= PAGE_READWRITE;
				MapAccess	= FILE_MAP_ALL_ACCESS;
				break;
		}
	}
};

FWindowsFile::FWindowsFile(const FPath& InFilename, EFileMode InMode, uint64 InSize) : Mode(InMode)
{
	Filename = MakeExtendedAbsolutePath(InFilename);

	bool bOpenedOk = OpenFileHandle(InMode);

	if (bOpenedOk)
	{
		if (IsReadOnly(InMode))
		{
			LARGE_INTEGER LiFileSize = {};
			bool		  bSizeOk	 = GetFileSizeEx(FileHandle, &LiFileSize);
			if (!bSizeOk)
			{
				LastError = GetLastError();
				return;
			}

			FileSize = LiFileSize.QuadPart;
		}
		else if (IsWritable(InMode) && InSize)
		{
			DWORD BytesReturned = 0;
			BOOL  SparseFileOk	= DeviceIoControl(FileHandle, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &BytesReturned, nullptr);
			if (!SparseFileOk)
			{
				UNSYNC_WARNING(L"Failed to mark file '%ls' as sparse.", Filename.wstring().c_str());
			}

			LARGE_INTEGER LiFileSize = {};
			LiFileSize.QuadPart		 = InSize;
			BOOL SizeOk				 = SetFilePointerEx(FileHandle, LiFileSize, nullptr, FILE_BEGIN);
			if (!SizeOk)
			{
				CloseHandle(FileHandle);
				LastError = GetLastError();
				return;
			}

			BOOL EndOfFileOk = SetEndOfFile(FileHandle);
			if (!EndOfFileOk)
			{
				CloseHandle(FileHandle);
				LastError = GetLastError();
				return;
			}

			FileSize = LiFileSize.QuadPart;
		}
		else if (IsWritable(InMode) && (InSize == 0))
		{
			// nothing to do when creating an empty file
		}
		else
		{
			UNSYNC_ERROR(L"Unexpected file mode %d", (int)InMode);
		}

		for (uint32 I = 0; I < NUM_QUEUES; ++I)
		{
			OverlappedEvents[I]			  = CreateEvent(nullptr, true, true, nullptr);
			Commands[I].Overlapped.hEvent = OverlappedEvents[I];
		}
	}
}
FWindowsFile::~FWindowsFile()
{
	Close();
}

bool
FWindowsFile::OpenFileHandle(EFileMode InMode)
{
	FCreateFileInfo Info(InMode);
	Info.FileFlags |= FILE_FLAG_OVERLAPPED;
	if (EnumHasAnyFlags(InMode, EFileMode::Unbuffered) && !GForceBufferedFiles)
	{
		Info.FileFlags |= FILE_FLAG_NO_BUFFERING;
	}

	FileHandle = CreateFileW(Filename.c_str(), Info.FileAccess, Info.Share, nullptr, Info.Disposition, Info.FileFlags, nullptr);

	if (FileHandle == INVALID_HANDLE_VALUE)
	{
		LastError = GetLastError();
		return false;
	}
	else
	{
		return true;
	}
}

bool
FWindowsFile::IsValid()
{
	std::lock_guard<std::mutex> LockGuard(Mutex);
	return FileHandle != INVALID_HANDLE_VALUE;
}

void
FWindowsFile::Close()
{
	InternalFlushAll();

	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(FileHandle);
		FileHandle = INVALID_HANDLE_VALUE;
	}
	for (uint32 I = 0; I < NUM_QUEUES; ++I)
	{
		if (OverlappedEvents[I])
		{
			CloseHandle(OverlappedEvents[I]);
			OverlappedEvents[I] = 0;
		}
	}
}

uint32
FWindowsFile::CompleteReadCommand(Command& Cmd)
{
	// Expects that Mutex is locked

	UNSYNC_ASSERT(Cmd.bActive);
	const uint32 MaxAttempts = 100;

	DWORD ReadBytes = 0;

	bool bRecoveredFromError = false;

	const uint64 ExpectedReadEnd   = std::min(FileSize, Cmd.SourceOffset + Cmd.ReadSize);
	const uint64 ExpectedReadBytes = ExpectedReadEnd - Cmd.SourceOffset;

	for (uint32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		BOOL OverlappedResultOk = GetOverlappedResult(FileHandle, &Cmd.Overlapped, &ReadBytes, true);

		LastError = 0;
		if (!OverlappedResultOk)
		{
			LastError = GetLastError();
		}

		if (ReadBytes == ExpectedReadBytes || ReadBytes == Cmd.ReadSize)
		{
			break;
		}
		else
		{
			UNSYNC_WARNING(L"FNativeFile expected to read %lld bytes, but read %lld. %hs",
						   (uint64)ExpectedReadBytes,
						   (uint64)ReadBytes,
						   FormatSystemErrorMessage(LastError).c_str());

			UNSYNC_LOG(L"Trying to recover from error (attempt %d of %d)", Attempt + 1, MaxAttempts);

			SchedulerSleep(1000);

			// Cancel any active overlapped IO commands
			for (Command& It : Commands)
			{
				if (It.bActive && (&It != &Cmd))
				{
					CancelIoEx(FileHandle, &It.Overlapped);
					WaitForSingleObject(It.Overlapped.hEvent, INFINITE);
				}
			}

			// Re-open the file handle
			CloseHandle(FileHandle);
			bool bOpenedOk = OpenFileHandle(Mode);
			if (!bOpenedOk)
			{
				LastError = GetLastError();
				UNSYNC_ERROR(L"Failed to re-open the file. %hs", FormatSystemErrorMessage(LastError).c_str());
				break;
			}

			// Try reading this block again
			ResetEvent(Cmd.Overlapped.hEvent);
			ReadFile(FileHandle, Cmd.Buffer.GetMemory(), CheckedNarrow(Cmd.ReadSize), nullptr, &Cmd.Overlapped);

			bRecoveredFromError = true;
		}
	}

	const uint64 ReadBytesClamped = std::min<uint64>(Cmd.Buffer.GetSize(), ReadBytes);

	if (Cmd.Callback)
	{
		Cmd.Callback(std::move(Cmd.Buffer), Cmd.SourceOffset, ReadBytesClamped, Cmd.UserData);
	}

	Cmd.bActive = false;

	if (bRecoveredFromError)
	{
		// Re-issue the read requests using new file handle
		for (Command& It : Commands)
		{
			if (It.bActive)
			{
				ResetEvent(It.Overlapped.hEvent);
				ReadFile(FileHandle, It.Buffer.GetMemory(), CheckedNarrow(It.ReadSize), nullptr, &It.Overlapped);
			}
		}
	}

	return CheckedNarrow(ReadBytesClamped);
}

uint64
FWindowsFile::Write(const void* Data, uint64 DestOffset, uint64 TotalSize)
{
	// TODO: !!!!! fire-and-forget asynchronous writes !!!!!

	std::lock_guard<std::mutex> LockGuard(Mutex);

	UNSYNC_ASSERT(IsWritable(Mode));

	if (!IsWriteOnly(Mode))
	{
		InternalFlushAll();	 // flush any outstanding read requests before writing
	}

	LARGE_INTEGER Pos;
	Pos.QuadPart = DestOffset;

	uint64					WrittenBytes = 0;
	static constexpr uint64 ChunkSize	 = 128_MB;
	uint64					NumChunks	 = DivUp(TotalSize, ChunkSize);

	uint64 SourceOffset = 0;

	for (uint64 I = 0; I < NumChunks; ++I)
	{
		int32	   ThisChunkSize = CheckedNarrow(CalcChunkSize(I, ChunkSize, TotalSize));
		OVERLAPPED Overlapped	 = {};

		Overlapped.Offset	  = Pos.LowPart;
		Overlapped.OffsetHigh = Pos.HighPart;

		BOOL WriteOk = WriteFile(FileHandle, reinterpret_cast<const uint8*>(Data) + SourceOffset, ThisChunkSize, nullptr, &Overlapped);
		if (!WriteOk && GetLastError() != ERROR_IO_PENDING)
		{
			LastError = GetLastError();
			return 0;
		}

		DWORD ChunkWrittenBytes	 = 0;
		BOOL  OverlappedResultOk = TRUE;

		uint32 MaxAttempts = 100000;
		uint32 Attempt	   = 0;
		for (Attempt = 0; Attempt < MaxAttempts; ++Attempt)
		{
			OverlappedResultOk = GetOverlappedResult(FileHandle, &Overlapped, &ChunkWrittenBytes, true);
			if (!OverlappedResultOk || ChunkWrittenBytes != 0)
			{
				break;
			}
			if (ChunkWrittenBytes == 0)
			{
				SchedulerSleep(1);
			}
		}
		if (Attempt == MaxAttempts)
		{
			UNSYNC_ERROR(L"Overlapped file write timed out");
		}

		if (!OverlappedResultOk)
		{
			LastError = GetLastError();
			break;
		}

		WrittenBytes += ChunkWrittenBytes;
		Pos.QuadPart += ChunkWrittenBytes;
		SourceOffset += ChunkWrittenBytes;
	}

	return WrittenBytes;
}

uint64
FWindowsFile::Read(void* Dest, uint64 SourceOffset, uint64 ReadSize)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);

	UNSYNC_ASSERTF((Mode & EFileMode::Unbuffered) == 0, L"Unbuffered files only support ReadAsync");
	UNSYNC_ASSERT(IsReadable(Mode));

	LARGE_INTEGER Pos;
	Pos.QuadPart = SourceOffset;

	uint64					ReadBytes = 0;
	static constexpr uint64 ChunkSize = 128_MB;
	uint64					NumChunks = DivUp(ReadSize, ChunkSize);

	uint64 DestOffset = 0;

	for (uint64 I = 0; I < NumChunks; ++I)
	{
		uint32	   ThisChunkSize = CheckedNarrow(CalcChunkSize(I, ChunkSize, ReadSize));
		OVERLAPPED Overlapped	 = {};

		Overlapped.Offset	  = Pos.LowPart;
		Overlapped.OffsetHigh = Pos.HighPart;

		BOOL ReadOk =
			ReadFile(FileHandle, reinterpret_cast<uint8*>(Dest) + DestOffset + I * ChunkSize, ThisChunkSize, nullptr, &Overlapped);
		if (!ReadOk && GetLastError() != ERROR_IO_PENDING)
		{
			LastError = GetLastError();
			return 0;
		}

		DWORD ChunkReadBytes	 = 0;
		BOOL  OverlappedResultOk = GetOverlappedResult(FileHandle, &Overlapped, &ChunkReadBytes, true);
		if (!OverlappedResultOk)
		{
			LastError = GetLastError();
			break;
		}

		ReadBytes += ChunkReadBytes;
		Pos.QuadPart += ChunkReadBytes;
		SourceOffset += ChunkReadBytes;
	}

	return ReadBytes;
}

bool
FWindowsFile::ReadAsync(uint64 SourceOffset, uint64 Size, uint64 UserData, IOCallback Callback)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);

	UNSYNC_ASSERT(IsReadable(Mode));

	uint32 CmdIdx = ~0u;

	// find available queue or wait for one to complete
	DWORD WaitResult = WaitForMultipleObjects(NUM_QUEUES, OverlappedEvents, false, INFINITE);

	if (WaitResult >= WAIT_OBJECT_0 && WaitResult < NUM_QUEUES)
	{
		CmdIdx = WaitResult - WAIT_OBJECT_0;
	}

	UNSYNC_ASSERT(CmdIdx < NUM_QUEUES);

	FWindowsFile::Command& Cmd = Commands[CmdIdx];

	if (Cmd.bActive)
	{
		CompleteReadCommand(Cmd);
	}

	if (EnumHasAnyFlags(Mode, EFileMode::Unbuffered))
	{
		uint64 OriginalSize	 = Size;
		uint64 OriginalBegin = SourceOffset;
		uint64 OriginalEnd	 = SourceOffset + Size;

		uint64 AlignedBegin = AlignDownToMultiplePow2(OriginalBegin, UNBUFFERED_READ_ALIGNMENT);
		uint64 AlignedEnd	= AlignUpToMultiplePow2(OriginalEnd, UNBUFFERED_READ_ALIGNMENT);

		SourceOffset = AlignedBegin;
		Size		 = AlignedEnd - AlignedBegin;

		Cmd.Buffer = FIOBuffer::Alloc(Size, L"WindowsFile::ReadAsync_aligned");

		Cmd.Buffer.SetDataRange(OriginalBegin - AlignedBegin, OriginalSize);
	}
	else
	{
		Cmd.Buffer = FIOBuffer::Alloc(Size, L"WindowsFile::ReadAsync");
	}

	LARGE_INTEGER Pos;
	Pos.QuadPart = SourceOffset;

	Cmd.ReadSize			  = Size;
	Cmd.Overlapped.Offset	  = Pos.LowPart;
	Cmd.Overlapped.OffsetHigh = Pos.HighPart;
	Cmd.UserData			  = UserData;
	Cmd.SourceOffset		  = SourceOffset;
	Cmd.Callback			  = Callback;

	ResetEvent(Cmd.Overlapped.hEvent);
	DWORD ReadOk = ReadFile(FileHandle, Cmd.Buffer.GetMemory(), CheckedNarrow(Size), nullptr, &Cmd.Overlapped);
	if (!ReadOk)
	{
		LastError = GetLastError();
	}
	Cmd.bActive = true;

	return true;
}

void
FWindowsFile::InternalFlushAll()
{
	for (uint32 I = 0; I < NUM_QUEUES; ++I)
	{
		if (Commands[I].bActive)
		{
			CompleteReadCommand(Commands[I]);
		}
	}
}

void
FWindowsFile::FlushAll()
{
	std::lock_guard<std::mutex> LockGuard(Mutex);

	InternalFlushAll();
}

void
FWindowsFile::FlushOne()
{
	std::lock_guard<std::mutex> LockGuard(Mutex);

	uint32 NumValidEvents			  = 0;
	HANDLE Events[NUM_QUEUES]		  = {};
	uint32 CommandIndices[NUM_QUEUES] = {};

	// todo: maintain a list of outstanding commands in read()
	for (uint32 I = 0; I < NUM_QUEUES; ++I)
	{
		if (Commands[I].Buffer.GetMemory())
		{
			Events[NumValidEvents]		   = OverlappedEvents[I];
			CommandIndices[NumValidEvents] = I;
			NumValidEvents++;
		}
	}

	if (NumValidEvents == 0)
	{
		return;
	}

	DWORD WaitResult = WaitForMultipleObjects(NumValidEvents, OverlappedEvents, false, INFINITE);

	uint32 CmdIdx = ~0u;
	if (WaitResult >= WAIT_OBJECT_0 && WaitResult < NUM_QUEUES)
	{
		CmdIdx = CommandIndices[WaitResult - WAIT_OBJECT_0];
	}

	UNSYNC_ASSERT(CmdIdx < NUM_QUEUES);
	FWindowsFile::Command& Cmd = Commands[CmdIdx];

	uint32 ReadBytes = CompleteReadCommand(Cmd);

	if (ReadBytes != Cmd.ReadSize)
	{
		return;
	}

	UNSYNC_ASSERT(Cmd.Buffer.GetMemory());
	UNSYNC_ASSERT(ReadBytes);

	Cmd.ReadSize = 0;
	Cmd.UserData = 0;
	Cmd.Buffer	 = {};
	Cmd.Callback = {};
}

FFileAttributes
GetFileAttrib(const FPath& Path, FFileAttributeCache* AttribCache)
{
	FFileAttributes Result;

	FPath ExtendedPath = MakeExtendedAbsolutePath(Path);

	if (AttribCache)
	{
		auto It = AttribCache->Map.find(ExtendedPath);
		if (It != AttribCache->Map.end())
		{
			Result = It->second;
			return Result;
		}
	}

	WIN32_FILE_ATTRIBUTE_DATA AttributeData;
	BOOL					  Ok = GetFileAttributesExW(ExtendedPath.c_str(), GetFileExInfoStandard, &AttributeData);
	if (Ok)
	{
		Result.bDirectory = !!(AttributeData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		Result.Size		  = MakeU64(AttributeData.nFileSizeHigh, AttributeData.nFileSizeLow);
		Result.Mtime	  = MakeU64(AttributeData.ftLastWriteTime);
		Result.bReadOnly  = (AttributeData.dwFileAttributes & FILE_ATTRIBUTE_READONLY);
		Result.bValid	  = true;
	}

	return Result;
}

uint64
ToWindowsFileTime(const std::filesystem::file_time_type& T)
{
	return T.time_since_epoch().count();
}

uint64
GetAvailableDiskSpace(const FPath& Path)
{
	ULARGE_INTEGER AvailableBytes = {};
	ULARGE_INTEGER TotalBytes	  = {};
	ULARGE_INTEGER FreeBytes	  = {};

	BOOL bOk = GetDiskFreeSpaceExW(Path.native().c_str(), &AvailableBytes, &TotalBytes, &FreeBytes);

	if (bOk)
	{
		return AvailableBytes.QuadPart;
	}
	else
	{
		return ~0ull;
	}
}

#endif	// UNSYNC_PLATFORM_WINDOWS

#if UNSYNC_PLATFORM_UNIX
FUnixFile::FUnixFile(const FPath& InFilename, EFileMode InMode, uint64 in_size) : Filename(InFilename), Mode(InMode)
{
	FileHandle = fopen(InFilename.native().c_str(), IsReadOnly(Mode) ? "rb" : "w+b");
	if (FileHandle == nullptr)
	{
		return;
	}

	FileDescriptor = fileno(FileHandle);

	if (IsReadOnly(Mode))
	{
		struct stat stat_buf = {};
		LastError			 = fstat(FileDescriptor, &stat_buf);
		UNSYNC_ASSERT(LastError == 0);
		FileSize = stat_buf.st_size;
	}
	else
	{
		LastError = ftruncate(FileDescriptor, in_size);
		UNSYNC_ASSERT(LastError == 0);
		FileSize = in_size;
	}
}

FUnixFile::~FUnixFile()
{
	Close();
}

void
FUnixFile::FlushAll()
{
	// nothing to do, all file IO is synchronous
}

void
FUnixFile::FlushOne()
{
	// nothing to do, all file IO is synchronous
}

void
FUnixFile::Close()
{
	if (FileHandle)
	{
		fclose(FileHandle);
		FileHandle = nullptr;
	}
}

uint64
FUnixFile::Read(void* dest, uint64 SourceOffset, uint64 ReadSize)
{
	// TODO: handle partial reads (pread returning 0 < x < ReadSize)
	uint64 read_bytes = pread(FileDescriptor, dest, ReadSize, SourceOffset);
	if (read_bytes != ReadSize)
	{
		LastError = errno;
	}
	return read_bytes;
}

bool
FUnixFile::ReadAsync(uint64 SourceOffset, uint64 size, uint64 UserData, IOCallback callback)
{
	return FIOReader::ReadAsync(SourceOffset, size, UserData, callback);
}

uint64
FUnixFile::Write(const void* data, uint64 DestOffset, uint64 WriteSize)
{
	UNSYNC_ASSERT(IsWritable(Mode));

	if (!IsWriteOnly(Mode))
	{
		FlushAll();	 // flush any outstanding read requests before writing
	}

	// TODO: handle partial writes (pwrite returning 0 < x < WriteSize)
	uint64 wrote_bytes = pwrite(FileDescriptor, data, WriteSize, DestOffset);
	if (wrote_bytes != WriteSize)
	{
		LastError = errno;
	}
	return wrote_bytes;
}

FFileAttributes
GetFileAttrib(const FPath& Path, FFileAttributeCache* AttribCache)
{
	FFileAttributes Result;

	if (AttribCache)
	{
		auto it = AttribCache->Map.find(Path);
		if (it != AttribCache->Map.end())
		{
			Result = it->second;
			return Result;
		}
	}

	// TODO: could potentially use std::filesystem::directory_entry for this on all platforms

	std::error_code ErrorCode	  = {};
	auto			Entry	  = std::filesystem::directory_entry(Path, ErrorCode);

	if (!ErrorCode)
	{
		std::filesystem::file_status Status = Entry.status(ErrorCode);
		if (ErrorCode)
		{
			return Result;
		}

		Result.bDirectory = Entry.is_directory();
		Result.Size		  = Result.bDirectory ? 0 : Entry.file_size();
		Result.Mtime	  = ToWindowsFileTime(Entry.last_write_time());
		Result.bReadOnly  = IsReadOnly(Status.permissions());
		Result.bValid	  = true;
	}

	return Result;
}

uint64
ToWindowsFileTime(const std::filesystem::file_time_type& FileTime)
{
	std::chrono::duration FullDuration = FileTime.time_since_epoch();

	uint64 FullSeconds = std::chrono::floor<std::chrono::seconds>(FullDuration).count();

	auto SubsecondDuration = FullDuration - std::chrono::seconds(FullSeconds);
	auto SubsecondNanos	   = std::chrono::duration_cast<std::chrono::nanoseconds>(SubsecondDuration).count();

	uint64 Ticks = (FullSeconds + SECONDS_BETWEEN_WINDOWS_AND_UNIX) * WINDOWS_TICKS_PER_SECOND + (SubsecondNanos / NANOS_PER_WINDOWS_TICK);

	return Ticks;
}

uint64
GetAvailableDiskSpace(const FPath& Path)
{
	return ~0ull; // TODO: query available space via statvfs()
}

#endif	// UNSYNC_PLATFORM_UNIX

bool
SetFileMtime(const FPath& Path, uint64 Mtime, bool bAllowInDryRun)
{
	UNSYNC_ASSERT(!GDryRun || bAllowInDryRun);

	FPath ExtendedPath = MakeExtendedAbsolutePath(Path);

	std::filesystem::file_time_type FileTime = FromWindowsFileTime(Mtime);

	std::error_code ErrorCode;
	std::filesystem::last_write_time(ExtendedPath, FileTime, ErrorCode);

	return !ErrorCode;
}

bool
SetFileReadOnly(const FPath& Path, bool bReadOnly)
{
	UNSYNC_ASSERT(!GDryRun);

	FPath ExtendedPath = MakeExtendedAbsolutePath(Path);

	std::error_code ErrorCode;

	if (bReadOnly)
	{
		std::filesystem::permissions(
			ExtendedPath,
			std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write,
			std::filesystem::perm_options::remove,
			ErrorCode);
	}
	else
	{
		std::filesystem::permissions(ExtendedPath, std::filesystem::perms::owner_write, std::filesystem::perm_options::add, ErrorCode);
	}

	return !ErrorCode;
}

FBuffer
ReadFileToBuffer(const FPath& Filename)
{
	FBuffer	   Result;
	FNativeFile File(Filename, EFileMode::ReadOnly);
	if (File.IsValid())
	{
		Result.Resize(File.GetSize());
		uint64 ReadBytes = File.Read(Result.Data(), 0, Result.Size());
		Result.Resize(ReadBytes);
	}
	return Result;
}

bool
WriteBufferToFile(const FPath& Filename, const uint8* Data, uint64 Size, EFileMode FileMode)
{
	UNSYNC_LOG_INDENT;

	if (Data == nullptr)
	{
		UNSYNC_ERROR(L"WriteBufferToFile called with null buffer");
		return false;
	}
	if (Size == 0)
	{
		UNSYNC_ERROR(L"WriteBufferToFile called with zero size buffer");
		return false;
	}
	if (GDryRun && !EnumHasAnyFlags(FileMode, EFileMode::IgnoreDryRun))
	{
		UNSYNC_ERROR(L"WriteBufferToFile called in dry run mode");
		return false;
	}

	FNativeFile File(Filename, FileMode, Size);

	if (File.IsValid())
	{
		uint64 WroteBytes = File.Write(Data, 0, Size);
		return WroteBytes == Size;
	}
	else
	{
		UNSYNC_ERROR(L"Failed to open file '%ls' for writing. %hs",
					 Filename.wstring().c_str(),
					 FormatSystemErrorMessage(File.GetError()).c_str());
		return false;
	}
}

bool
WriteBufferToFile(const FPath& Filename, const FBuffer& Buffer, EFileMode FileMode)
{
	return WriteBufferToFile(Filename, Buffer.Data(), Buffer.Size(), FileMode);
}

bool
WriteBufferToFile(const FPath& Filename, const std::string& Buffer, EFileMode FileMode)
{
	return WriteBufferToFile(Filename, (const uint8*)Buffer.data(), Buffer.length(), FileMode);
}

struct FIOBufferCache
{
	std::mutex Mutex;

	struct FAllocation
	{
		const wchar_t* DebugName = nullptr;
		uint8*		   Memory;
		uint64		   Size;
	};

	std::vector<FAllocation> AllocatedBlocks;  // todo: hash table (though current numbers are very low, so...)
	std::vector<FAllocation> AvailableBlocks;
	uint64					 CurrentCacheSize	  = 0;
	uint64					 CurrentAllocatedSize = 0;

	static constexpr uint64 MAX_CACHED_ALLOC_SIZE = 32_MB;
	static constexpr uint64 MAX_TOTAL_CACHE_SIZE  = 4_GB;

	~FIOBufferCache()
	{
		for (FAllocation& X : AllocatedBlocks)
		{
			UnsyncFree(X.Memory);
		}
		for (FAllocation& X : AvailableBlocks)
		{
			UnsyncFree(X.Memory);
		}
	}

	uint8* Alloc(uint64 Size, const wchar_t* DebugName)
	{
		std::lock_guard<std::mutex> LockGuard(Mutex);

		uint64 BestBlockIndex = ~0ull;
		uint64 BestBlockSize  = ~0ull;

		if (Size <= MAX_CACHED_ALLOC_SIZE)
		{
			Size = NextPow2(CheckedNarrow(Size));
			for (uint64 I = 0; I < AvailableBlocks.size(); ++I)
			{
				FAllocation& Candidate = AvailableBlocks[I];
				if (Candidate.Size < BestBlockSize && Candidate.Size >= Size)
				{
					BestBlockSize  = Candidate.Size;
					BestBlockIndex = I;
				}
			}
		}

		FAllocation Allocation = {};

		if (BestBlockSize != ~0ull)
		{
			FAllocation Candidate = AvailableBlocks[BestBlockIndex];
			AllocatedBlocks.push_back(Candidate);
			AvailableBlocks[BestBlockIndex] = AvailableBlocks.back();
			AvailableBlocks.pop_back();

			Allocation = Candidate;
		}
		else
		{
			CurrentAllocatedSize += Size;

			Allocation.Memory = (uint8*)UnsyncMalloc(Size);
			UNSYNC_ASSERT(Allocation.Memory);

			Allocation.Size = Size;
			AllocatedBlocks.push_back(Allocation);

			if (Size <= MAX_CACHED_ALLOC_SIZE)
			{
				CurrentCacheSize += Allocation.Size;
			}
		}

		return Allocation.Memory;
	}

	void Free(uint8* Ptr)
	{
		std::lock_guard<std::mutex> LockGuard(Mutex);

		uint64 AllocationIndex = ~0u;
		for (uint64 I = 0; I < AllocatedBlocks.size(); ++I)
		{
			if (AllocatedBlocks[I].Memory == Ptr)
			{
				AllocationIndex = I;
				break;
			}
		}

		UNSYNC_ASSERTF(AllocationIndex != ~0u, L"Trying to free an unknown IOBuffer.");

		FAllocation FreedBlock = AllocatedBlocks[AllocationIndex];

		if (FreedBlock.Size <= MAX_CACHED_ALLOC_SIZE)
		{
			AvailableBlocks.push_back(FreedBlock);
		}
		else
		{
			UnsyncFree(FreedBlock.Memory);
			CurrentAllocatedSize -= FreedBlock.Size;
		}

		while (CurrentCacheSize > MAX_TOTAL_CACHE_SIZE && !AvailableBlocks.empty())
		{
			FAllocation& LastBlock = AvailableBlocks.back();
			UnsyncFree(LastBlock.Memory);

			UNSYNC_ASSERT(CurrentCacheSize >= LastBlock.Size);
			CurrentCacheSize -= LastBlock.Size;

			CurrentAllocatedSize -= LastBlock.Size;
			AvailableBlocks.pop_back();
		}

		AllocatedBlocks[AllocationIndex] = AllocatedBlocks.back();
		AllocatedBlocks.pop_back();
	}

	uint64 GetCurrentSize()
	{
		std::lock_guard<std::mutex> LockGuard(Mutex);
		return CurrentCacheSize;
	}
};

static FIOBufferCache GIoBufferCache;

uint8*
AllocIoBuffer(uint64 Size, const wchar_t* DebugName)
{
	return GIoBufferCache.Alloc(Size, DebugName);
}

void
FreeIoBuffer(uint8* Ptr)
{
	GIoBufferCache.Free(Ptr);
}

uint64
GetCurrentIoCacheSize()
{
	return GIoBufferCache.GetCurrentSize();
}

FFileAttributeCache
CreateFileAttributeCache(const FPath& Root, const FSyncFilter* SyncFilter)
{
	FFileAttributeCache Result;

	FTimePoint NextProgressLogTime = TimePointNow() + std::chrono::seconds(1);

	auto ReportProgress = [&NextProgressLogTime, &Result]() {
		FTimePoint TimeNow = TimePointNow();
		if (TimeNow >= NextProgressLogTime)
		{
			LogPrintf(ELogLevel::Debug, L"Found files: %d\r", (int)Result.Map.size());
			NextProgressLogTime = TimeNow + std::chrono::seconds(1);
		}
	};

	FPath ResolvedRoot = SyncFilter ? SyncFilter->Resolve(Root) : Root;

	for (const std::filesystem::directory_entry& Dir : RecursiveDirectoryScan(ResolvedRoot))
	{
		if (Dir.is_directory())
		{
			continue;
		}

		if (SyncFilter && !SyncFilter->ShouldSync(Dir.path().native()))
		{
			continue;
		}

		FFileAttributes Attr = {};

		Attr.Mtime	= ToWindowsFileTime(Dir.last_write_time());
		Attr.Size	= Dir.file_size();
		Attr.bValid = true;
		Attr.bReadOnly = IsReadOnly(Dir.status().permissions());

		Result.Map[Dir.path().native()] = Attr;

		ReportProgress();
	}

	ReportProgress();

	return Result;
}

bool
IsDirectory(const FPath& Path)
{
	FFileAttributes Attr = GetFileAttrib(Path);
	return Attr.bValid && Attr.bDirectory;
}

bool
PathExists(const FPath& Path)
{
	FPath ExtendedPath = MakeExtendedAbsolutePath(Path);
	return std::filesystem::exists(ExtendedPath);
}

bool
PathExists(const FPath& Path, std::error_code& OutErrorCode)
{
	FPath ExtendedPath = MakeExtendedAbsolutePath(Path);
	return std::filesystem::exists(ExtendedPath, OutErrorCode);
}

bool
CreateDirectories(const FPath& Path)
{
	FPath ExtendedPath = MakeExtendedAbsolutePath(Path);
	return std::filesystem::create_directories(ExtendedPath);
}

bool
FileRename(const FPath& From, const FPath& To, std::error_code& OutErrorCode)
{
	FPath ExtendedFrom = MakeExtendedAbsolutePath(From);
	FPath ExtendedTo   = MakeExtendedAbsolutePath(To);
	std::filesystem::rename(ExtendedFrom, ExtendedTo, OutErrorCode);
	return OutErrorCode.value() == 0;
}

bool
FileCopy(const FPath& From, const FPath& To, std::error_code& OutErrorCode)
{
	FPath ExtendedFrom = MakeExtendedAbsolutePath(From);
	FPath ExtendedTo   = MakeExtendedAbsolutePath(To);
	return std::filesystem::copy_file(ExtendedFrom, ExtendedTo, OutErrorCode);
}

bool
FileCopyOverwrite(const FPath& From, const FPath& To, std::error_code& OutErrorCode)
{
	FPath ExtendedFrom = MakeExtendedAbsolutePath(From);
	FPath ExtendedTo   = MakeExtendedAbsolutePath(To);
	return std::filesystem::copy_file(ExtendedFrom, ExtendedTo, std::filesystem::copy_options::overwrite_existing, OutErrorCode);
}

bool
FileRemove(const FPath& Path, std::error_code& OutErrorCode)
{
	FPath ExtendedPath = MakeExtendedAbsolutePath(Path);
	return std::filesystem::remove(ExtendedPath, OutErrorCode);
}

std::filesystem::recursive_directory_iterator RecursiveDirectoryScan(const FPath& Path)
{
	FPath ExtendedPath = MakeExtendedAbsolutePath(Path);
	return std::filesystem::recursive_directory_iterator(ExtendedPath);
}

FMemReader::FMemReader(const uint8* InData, uint64 InDataSize) : Data(InData), Size(InDataSize)
{
}

uint64
FMemReader::Read(void* Dest, uint64 SourceOffset, uint64 ReadSize)
{
	uint64 ReadEndOffset   = std::min(SourceOffset + ReadSize, Size);
	uint64 ClampedReadSize = ReadEndOffset - SourceOffset;
	memcpy(Dest, Data + SourceOffset, ClampedReadSize);
	return ClampedReadSize;
}

FMemReaderWriter::FMemReaderWriter(uint8* InData, uint64 InDataSize) : FMemReader(InData, InDataSize), DataRw(InData)
{
}

uint64
FMemReaderWriter::Write(const void* InData, uint64 DestOffset, uint64 WriteSize)
{
	uint64 WriteEndOffset	= std::min(DestOffset + WriteSize, Size);
	uint64 ClampedWriteSize = WriteEndOffset - DestOffset;
	if (ClampedWriteSize && DataRw)
	{
		memcpy(DataRw + DestOffset, InData, ClampedWriteSize);
	}
	return ClampedWriteSize;
}

FIOBuffer
FIOBuffer::Alloc(uint64 Size, const wchar_t* DebugName)
{
	UNSYNC_ASSERT(Size);

	FIOBuffer Result;

	Result.MemoryPtr  = AllocIoBuffer(Size, DebugName);
	Result.MemorySize = Size;

	Result.DataPtr	= Result.MemoryPtr;
	Result.DataSize = Size;

	Result.DebugName = DebugName;

	return Result;
}

FIOBuffer::~FIOBuffer()
{
	Clear();
	UNSYNC_CLOBBER(Canary);
}

FIOBuffer::FIOBuffer(FIOBuffer&& Rhs)
{
	UNSYNC_ASSERT(Rhs.Canary == CANARY);

	std::swap(MemoryPtr, Rhs.MemoryPtr);
	std::swap(MemorySize, Rhs.MemorySize);

	std::swap(DataPtr, Rhs.DataPtr);
	std::swap(DataSize, Rhs.DataSize);
}

void
FIOBuffer::Clear()
{
	UNSYNC_ASSERT(Canary == CANARY);
	if (MemoryPtr)
	{
		FreeIoBuffer(MemoryPtr);

		MemoryPtr  = nullptr;
		MemorySize = 0;

		DataPtr	 = nullptr;
		DataSize = 0;
	}
}

FIOBuffer&
FIOBuffer::operator=(FIOBuffer&& Rhs)
{
	UNSYNC_ASSERT(Canary == CANARY);
	UNSYNC_ASSERT(Rhs.Canary == CANARY);
	if (this != &Rhs)
	{
		std::swap(MemoryPtr, Rhs.MemoryPtr);
		std::swap(MemorySize, Rhs.MemorySize);

		std::swap(DataPtr, Rhs.DataPtr);
		std::swap(DataSize, Rhs.DataSize);

		Rhs.Clear();
	}
	return *this;
}

void
TestFileTime()
{
	UNSYNC_LOG(L"TestFileTime()");
	UNSYNC_LOG_INDENT;

	// 20231024004826Z - 2023 October 24 12:48:26
	// unix 1698108506
	// windows 133425821060000000
	const uint64 BaseExpectedWindowsTime = 133425821060000000ull;

	// Check basic conversion functionality at maximum
	{
		UNSYNC_LOG(L"File time precision estimate:");
		UNSYNC_LOG_INDENT;

		uint64 ExpectedWindowsTime = BaseExpectedWindowsTime + 9999999;

		std::filesystem::file_time_type FileTime = FromWindowsFileTime(ExpectedWindowsTime);

		uint64 RoundTripWindowsTime = ToWindowsFileTime(FileTime);
		uint64 NativeCount			= FileTime.time_since_epoch().count();

		uint64 Delta = ExpectedWindowsTime > RoundTripWindowsTime ? ExpectedWindowsTime - RoundTripWindowsTime
																  : RoundTripWindowsTime - ExpectedWindowsTime;

		UNSYNC_LOG(L"ExpectedWindowsTime  = %llu", llu(ExpectedWindowsTime));
		UNSYNC_LOG(L"RoundTripWindowsTime = %llu", llu(RoundTripWindowsTime));
		UNSYNC_LOG(L"NativeCount = %llu, Delta = %llu", llu(NativeCount), llu(Delta));
	}

	// Check basic conversion functionality at 1 second precision
	{
		uint64 ExpectedWindowsTime = BaseExpectedWindowsTime;

		std::filesystem::file_time_type FileTime = FromWindowsFileTime(ExpectedWindowsTime);

		uint64 RoundTripWindowsTime = ToWindowsFileTime(FileTime);
		uint64 NativeCount			= FileTime.time_since_epoch().count();

		UNSYNC_ASSERTF(RoundTripWindowsTime == ExpectedWindowsTime,
					   L"RoundTripWindowsTime is %llu, but expected to be %llu. Native count: %llu",
					   llu(RoundTripWindowsTime),
					   llu(ExpectedWindowsTime),
					   llu(NativeCount));
	}
}

void
TestFileAttrib()
{
	UNSYNC_LOG(L"TestFileAttrib()");
	UNSYNC_LOG_INDENT;

	FPath TempDirPath = std::filesystem::temp_directory_path() / "unsync_test";
	CreateDirectories(TempDirPath);

	const bool bDirectoryExists = PathExists(TempDirPath) && IsDirectory(TempDirPath);
	UNSYNC_ASSERT(bDirectoryExists);

	const FPath TestFilename = TempDirPath / "attrib.txt";
	UNSYNC_LOG(L"Test file name: %ls", TestFilename.wstring().c_str());

	if (PathExists(TestFilename))
	{
		SetFileReadOnly(TestFilename, false);
	}

	const bool	bFileWritten = WriteBufferToFile(TestFilename, "unsync test file");
	UNSYNC_ASSERT(bFileWritten);

	const uint64 ExpectedFileTime = 133425821060000000ull;

	const bool bMtimeSet = SetFileMtime(TestFilename, ExpectedFileTime);
	UNSYNC_ASSERT(bMtimeSet);

	const FFileAttributes FileAttrib = GetFileAttrib(TestFilename);
	UNSYNC_ASSERT(!FileAttrib.bReadOnly);

	UNSYNC_ASSERT(FileAttrib.Mtime == ExpectedFileTime);

	const bool bReadOnlySet = SetFileReadOnly(TestFilename, true);
	UNSYNC_ASSERT(bReadOnlySet);

	const FFileAttributes FileAttribReadOnly = GetFileAttrib(TestFilename);
	UNSYNC_ASSERT(FileAttribReadOnly.bReadOnly);

	const bool bReadOnlyReset = SetFileReadOnly(TestFilename, false);
	UNSYNC_ASSERT(bReadOnlyReset);

	const FFileAttributes FileAttribNonReadOnly = GetFileAttrib(TestFilename);
	UNSYNC_ASSERT(!FileAttribNonReadOnly.bReadOnly);

	std::error_code ErrorCode;
	const bool		bFileDeleted = FileRemove(TestFilename, ErrorCode);
	UNSYNC_ASSERT(bFileDeleted);
}

}  // namespace unsync

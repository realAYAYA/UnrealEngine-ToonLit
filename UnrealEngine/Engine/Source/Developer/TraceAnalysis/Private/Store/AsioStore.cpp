// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsioStore.h"
#include "HAL/PlatformFile.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "Misc/CString.h"
#include "Misc/DateTime.h"

#if PLATFORM_WINDOWS
#	include "Windows/AllowWindowsPlatformTypes.h"
#	include <Windows.h>
#	include "Windows/HideWindowsPlatformTypes.h"
#elif PLATFORM_LINUX
	#include <sys/inotify.h>
#elif PLATFORM_MAC
	#include "Misc/Paths.h"
	#include "Mac/CocoaThread.h"
#endif

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
#if PLATFORM_WINDOWS
class FAsioStore::FDirWatcher
	: public asio::windows::object_handle
{
public:
	using asio::windows::object_handle::object_handle;
};
#elif PLATFORM_LINUX
class FAsioStore::FDirWatcher
	: public asio::posix::stream_descriptor
{
	typedef TFunction<void(asio::error_code error)> HandlerType;
public:
	using asio::posix::stream_descriptor::stream_descriptor;
	void async_wait(HandlerType InHandler)
	{
		asio::posix::stream_descriptor::async_wait(asio::posix::stream_descriptor::wait_read, InHandler);
	}
};
#elif PLATFORM_MAC
class FAsioStore::FDirWatcher
{
	typedef TFunction<void(asio::error_code error)> HandlerType;
public:
	FDirWatcher(const TCHAR* InStoreDir)
		: StoreDir(InStoreDir)
	{

	}
	void async_wait(HandlerType InHandler);
	void cancel()
	{
		close();
	}
	void close()
	{
		if (bIsRunning)
		{
			FSEventStreamStop(EventStream);
			FSEventStreamInvalidate(EventStream);
			FSEventStreamRelease(EventStream);
			bIsRunning = false;
		}
	}
	bool is_open() { return bIsRunning; }

	void ProcessChanges(size_t EventCount, void* EventPaths, const FSEventStreamEventFlags EventFlags[])
	{
		Handler(asio::error_code());
	}

	FSEventStreamRef	EventStream;
    HandlerType Handler;
private:
	bool bIsRunning = false;
	const TCHAR* StoreDir = nullptr;
};

void MacCallback(ConstFSEventStreamRef StreamRef,
					void* InDirWatcherPtr,
					size_t EventCount,
					void* EventPaths,
					const FSEventStreamEventFlags EventFlags[],
					const FSEventStreamEventId EventIDs[])
{
	FAsioStore::FDirWatcher* DirWatcherPtr = (FAsioStore::FDirWatcher*)InDirWatcherPtr;
	check(DirWatcherPtr);
	check(DirWatcherPtr->EventStream == StreamRef);

	GameThreadCall(^{
		DirWatcherPtr->ProcessChanges(EventCount, EventPaths, EventFlags);
	});
}

void FAsioStore::FDirWatcher::async_wait(HandlerType InHandler)
{
    if (bIsRunning)
    {
        return;
    }

	CFAbsoluteTime Latency = 0.2;	// seconds

	FSEventStreamContext Context;
	Context.version = 0;
	Context.info = this;
	Context.retain = NULL;
	Context.release = NULL;
	Context.copyDescription = NULL;

	// Make sure the path is absolute
	const FString FullPath = FPaths::ConvertRelativePathToFull(StoreDir);

	// Set up streaming and turn it on
	CFStringRef FullPathMac = FPlatformString::TCHARToCFString(*FullPath);
	CFArrayRef PathsToWatch = CFArrayCreate(NULL, (const void**)&FullPathMac, 1, NULL);

	EventStream = FSEventStreamCreate(NULL,
		&MacCallback,
		&Context,
		PathsToWatch,
		kFSEventStreamEventIdSinceNow,
		Latency,
		kFSEventStreamCreateFlagUseCFTypes | kFSEventStreamCreateFlagNoDefer
	);

	FSEventStreamSetDispatchQueue(EventStream, dispatch_get_main_queue());
	FSEventStreamStart(EventStream);
	bIsRunning = true;

	Handler = InHandler;
}
#else
class FAsioStore::FDirWatcher
{
public:
	void async_wait(...) {}
	void cancel() {}
	void close() {}
	bool is_open() { return false; }
};
#endif



////////////////////////////////////////////////////////////////////////////////
FAsioStore::FTrace::FTrace(const TCHAR* InPath)
: Path(InPath)
{
	// Extract the trace's name
	const TCHAR* Dot = FCString::Strrchr(*Path, '.');
	if (Dot == nullptr)
	{
		Dot = *Path;
	}

	for (const TCHAR* c = Dot; c > *Path; --c)
	{
		if (c[-1] == '\\' || c[-1] == '/')
		{
			Name = FStringView(c, int32(Dot - c));
			break;
		}
	}

	Id = QuickStoreHash(Name);

	// Calculate that trace's timestamp
	uint64 InTimestamp = 0;
#if PLATFORM_WINDOWS
	HANDLE Handle = CreateFileW(InPath, 0, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, 0, nullptr);
	if (Handle != INVALID_HANDLE_VALUE)
	{
		FILETIME Time;
		if (GetFileTime(Handle, &Time, nullptr, nullptr))
		{
			// Windows FILETIME is a 64-bit value that represents the number of 100-nanosecond intervals that have elapsed since 12:00 A.M. January 1, 1601 UTC.
			// We adjust it to be compatible with the FDateTime ticks number of 100-nanosecond intervals that have elapsed since 12:00 A.M. January 1, 0001 UTC.
			InTimestamp = (static_cast<uint64>(Time.dwHighDateTime) << 32ull) | static_cast<uint64>(Time.dwLowDateTime); // [100-nanosecond ticks since 1601]
			InTimestamp += 0x0701ce1722770000ull; // FDateTime(1601, 1, 1).GetTicks()
		}

		CloseHandle(Handle);
	}
#elif PLATFORM_MAC
	struct stat FileStat;
	if (stat(TCHAR_TO_UTF8(InPath), &FileStat) == 0)
	{
		// The tv_sec field is an integer value that represents the number of seconds that have elapsed since the Unix epoch 12:00 A.M. January 1, 1970 UTC.
		// We adjust it to be compatible with the FDateTime ticks number of 100-nanosecond intervals that have elapsed since 12:00 A.M. January 1, 0001 UTC.
		InTimestamp = (uint64(FileStat.st_mtimespec.tv_sec) * 1000 * 1000 * 1000) + FileStat.st_mtimespec.tv_nsec; // [nanoseconds since 1970]
		InTimestamp /= 100; // [100-nanosecond ticks since 1970]
		InTimestamp += 0x089f7ff5f7b58000ull; // FDateTime(1970, 1, 1).GetTicks()
	}
#else
	struct stat FileStat;
	if (stat(TCHAR_TO_UTF8(InPath), &FileStat) == 0)
	{
		// The tv_sec field is an integer value that represents the number of seconds that have elapsed since the Unix epoch 12:00 A.M. January 1, 1970 UTC.
		// We adjust it to be compatible with the FDateTime ticks number of 100-nanosecond intervals that have elapsed since 12:00 A.M. January 1, 0001 UTC.
		InTimestamp = (uint64(FileStat.st_mtim.tv_sec) * 1000 * 1000 * 1000) + FileStat.st_mtim.tv_nsec; // [nanoseconds since 1970]
		InTimestamp /= 100; // [100-nanosecond ticks since 1970]
		InTimestamp += 0x089f7ff5f7b58000ull; // FDateTime(1970, 1, 1).GetTicks()
	}
#endif
	Timestamp = InTimestamp;
}

////////////////////////////////////////////////////////////////////////////////
const FStringView& FAsioStore::FTrace::GetName() const
{
	return Name;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioStore::FTrace::GetId() const
{
	return Id;
}

////////////////////////////////////////////////////////////////////////////////
uint64 FAsioStore::FTrace::GetSize() const
{
#if PLATFORM_WINDOWS
	LARGE_INTEGER FileSize = {};
	HANDLE Handle = CreateFileW(*Path, 0, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, 0, nullptr);
	if (Handle != INVALID_HANDLE_VALUE)
	{
		GetFileSizeEx(Handle, &FileSize);
		CloseHandle(Handle);
	}
	return FileSize.QuadPart;
#else
	struct stat FileStat;
	if (stat(TCHAR_TO_UTF8(*Path), &FileStat) == 0)
	{
		return uint64(FileStat.st_size);
	}
	return 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////
uint64 FAsioStore::FTrace::GetTimestamp() const
{
	return Timestamp;
}



////////////////////////////////////////////////////////////////////////////////
FAsioStore::FAsioStore(asio::io_context& InIoContext, const TCHAR* InStoreDir)
: IoContext(InIoContext)
, StoreDir(InStoreDir)
{
	Refresh();

#if PLATFORM_WINDOWS
	HANDLE DirWatchHandle = FindFirstChangeNotificationW(InStoreDir, false, FILE_NOTIFY_CHANGE_FILE_NAME);
	if (DirWatchHandle == INVALID_HANDLE_VALUE)
	{
		DirWatchHandle = 0;
	}
	DirWatcher = new FDirWatcher(IoContext, DirWatchHandle);
#elif PLATFORM_LINUX
	int inotfd = inotify_init();
	int watch_desc = inotify_add_watch(inotfd, TCHAR_TO_UTF8(InStoreDir), IN_CREATE | IN_DELETE);
	DirWatcher = new FDirWatcher(IoContext, inotfd);
#elif PLATFORM_MAC
	DirWatcher = new FDirWatcher(*StoreDir);
#endif

	WatchDir();
}

////////////////////////////////////////////////////////////////////////////////
FAsioStore::~FAsioStore()
{
	if (DirWatcher != nullptr)
	{
		check(!DirWatcher->is_open());
		delete DirWatcher;
	}
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStore::Close()
{
	if (DirWatcher != nullptr)
	{
		DirWatcher->cancel();
		DirWatcher->close();
	}

	ClearTraces();
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStore::ClearTraces()
{
	for (FTrace* Trace : Traces)
	{
		delete Trace;
	}

	Traces.Empty();
	ChangeSerial = 0;
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStore::WatchDir()
{
	if (DirWatcher == nullptr)
	{
		return;
	}
	
	DirWatcher->async_wait([this] (asio::error_code ErrorCode)
	{
		if (ErrorCode)
		{
			return;
		}

#if PLATFORM_WINDOWS
		FindNextChangeNotification(DirWatcher->native_handle());
#endif
		Refresh();
		WatchDir();
	});
}

////////////////////////////////////////////////////////////////////////////////
const TCHAR* FAsioStore::GetStoreDir() const
{
	return *StoreDir;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioStore::GetChangeSerial() const
{
	return ChangeSerial;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioStore::GetTraceCount() const
{
	return Traces.Num();
}

////////////////////////////////////////////////////////////////////////////////
const FAsioStore::FTrace* FAsioStore::GetTraceInfo(uint32 Index) const
{
	if (Index >= uint32(Traces.Num()))
	{
		return nullptr;
	}

	return Traces[Index];
}

////////////////////////////////////////////////////////////////////////////////
FAsioStore::FTrace* FAsioStore::GetTrace(uint32 Id) const
{
	for (FTrace* Trace : Traces)
	{
		if (Trace->GetId() == Id)
		{
			return Trace;
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
FAsioStore::FTrace* FAsioStore::AddTrace(const TCHAR* Path)
{
	FTrace NewTrace(Path);

	uint32 Id = NewTrace.GetId();
	if (FTrace* Existing = GetTrace(Id))
	{
		return Existing;
	}

	ChangeSerial += Id;

	FTrace* Trace = new FTrace(MoveTemp(NewTrace));
	Traces.Add(Trace);
	return Trace;
}

////////////////////////////////////////////////////////////////////////////////
FAsioStore::FNewTrace FAsioStore::CreateTrace()
{
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();

	FString TracePath;
#if 0
	bool bOk = false;
	for (int i = 0; i < 256; ++i)
	{
		uint32 TraceId = ++LastTraceId;

		TracePath = StoreDir;
		TracePath.Appendf(TEXT("/%05d"), TraceId);

		if (!PlatformFile.DirectoryExists(*TracePath) && PlatformFile.CreateDirectory(*TracePath))
		{
			bOk = true;
			break;
		}
	}

	if (!bOk)
	{
		return {};
	}

	TracePath += TEXT("/data.utrace");
#else
	FString Prefix = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));

	TracePath = StoreDir;
	TracePath.Appendf(TEXT("/%s.utrace"), *Prefix);
	for (uint64 Index = 0; PlatformFile.FileExists(*TracePath); ++Index)
	{
		TracePath = StoreDir;
		TracePath.Appendf(TEXT("/%s_%d.utrace"), *Prefix, Index);
	}
#endif // 0

	FAsioWriteable* File = FAsioFile::WriteFile(IoContext, *TracePath);
	if (File == nullptr)
	{
		return {};
	}

	FTrace* Trace = AddTrace(*TracePath);
	if (Trace == nullptr)
	{
		delete File;
		return {};
	}

	return { Trace->GetId(), File };
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioStore::HasTrace(uint32 Id) const
{
	return GetTrace(Id) != nullptr;
}

////////////////////////////////////////////////////////////////////////////////
FAsioReadable* FAsioStore::OpenTrace(uint32 Id)
{
	FTrace* Trace = GetTrace(Id);
	if (Trace == nullptr)
	{
		return nullptr;
	}

	FString TracePath;
	TracePath = StoreDir;
#if 0
	TracePath.Appendf(TEXT("/%05d/data.utrace"), Id);
#else
	TracePath += "/";
	TracePath += Trace->GetName();
	TracePath += ".utrace";
#endif // 0

	return FAsioFile::ReadFile(IoContext, *TracePath);
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStore::Refresh()
{
	ClearTraces();

	IPlatformFile& FileSystem = IPlatformFile::GetPlatformPhysical();
	FileSystem.IterateDirectory(*StoreDir, [this] (const TCHAR* Path, bool IsDirectory)
	{
#if 0
		if (!IsDirectory)
		{
			return true;
		}

		int32 Id = FCString::Atoi(Path);
		LastTraceId = (Id < LastTraceId) ? Id : LastTraceId;
#else
		if (IsDirectory)
		{
			return true;
		}

		const TCHAR* Dot = FCString::Strrchr(Path, '.');
		if (Dot == nullptr || FCString::Strcmp(Dot, TEXT(".utrace")))
		{
			return true;
		}

		AddTrace(Path);
#endif // 0

		return true;
	});
}

} // namespace Trace
} // namespace UE

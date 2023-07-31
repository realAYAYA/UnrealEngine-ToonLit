// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pch.h"
#include "Store.h"

#if TS_USING(TS_PLATFORM_LINUX)
#   include <sys/inotify.h>
#endif
#if TS_USING(TS_PLATFORM_MAC)
#	include <CoreServices/CoreServices.h>
#endif


////////////////////////////////////////////////////////////////////////////////
// Pre-C++20 there is now way to convert between clocks. C++20 onwards it is
// possible to create a clock with an Unreal epoch and convert file times to it.
// But at time of writing, C++20 isn't complete enough across the board.
static const uint64	UnrealEpochYear		= 1;
#if TS_USING(TS_PLATFORM_WINDOWS)
static const uint64	FsEpochYear			= 1601;
#else
static const uint64	FsEpochYear			= 1970;
#endif
static int64 FsToUnrealEpochBiasSeconds	= uint64(double(FsEpochYear - UnrealEpochYear) * 365.2425) * 86400;

////////////////////////////////////////////////////////////////////////////////
#if TS_USING(TS_PLATFORM_WINDOWS)
class FStore::FDirWatcher
	: public asio::windows::object_handle
{
public:
	using asio::windows::object_handle::object_handle;
};
#elif TS_USING(TS_PLATFORM_LINUX)
class FStore::FDirWatcher
	: public asio::posix::stream_descriptor
{
	typedef std::function<void(asio::error_code error)> HandlerType;
public:
	using asio::posix::stream_descriptor::stream_descriptor;
	void async_wait(HandlerType InHandler)
	{
		asio::posix::stream_descriptor::async_wait(asio::posix::stream_descriptor::wait_read, InHandler);
	}
};
#elif TS_USING(TS_PLATFORM_MAC)
class FStore::FDirWatcher
{
	typedef std::function<void(asio::error_code error)> HandlerType;
public:
	FDirWatcher(const char* InStoreDir)
	{
		std::error_code ErrorCode;
		StoreDir = std::filesystem::absolute(InStoreDir, ErrorCode);
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
			FSEventStreamUnscheduleFromRunLoop(EventStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
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
	FPath StoreDir;
};

void MacCallback(ConstFSEventStreamRef StreamRef,
					void* InDirWatcherPtr,
					size_t EventCount,
					void* EventPaths,
					const FSEventStreamEventFlags EventFlags[],
					const FSEventStreamEventId EventIDs[])
{
	FStore::FDirWatcher* DirWatcherPtr = (FStore::FDirWatcher*)InDirWatcherPtr;
	check(DirWatcherPtr);
	check(DirWatcherPtr->EventStream == StreamRef);

	DirWatcherPtr->ProcessChanges(EventCount, EventPaths, EventFlags);
}

void FStore::FDirWatcher::async_wait(HandlerType InHandler)
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

	// Set up streaming and turn it on
	std::string Path = StoreDir;
	CFStringRef FullPathMac = CFStringCreateWithBytes(
		kCFAllocatorDefault,
		(const uint8*)Path.data(),
		Path.size(),
		kCFStringEncodingUnicode,
		false);

	CFArrayRef PathsToWatch = CFArrayCreate(NULL, (const void**)&FullPathMac, 1, NULL);

	EventStream = FSEventStreamCreate(NULL,
		&MacCallback,
		&Context,
		PathsToWatch,
		kFSEventStreamEventIdSinceNow,
		Latency,
		kFSEventStreamCreateFlagUseCFTypes | kFSEventStreamCreateFlagNoDefer
	);

	FSEventStreamScheduleWithRunLoop(EventStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	FSEventStreamStart(EventStream);
	bIsRunning = true;

	Handler = InHandler;
}
#endif



////////////////////////////////////////////////////////////////////////////////
FStore::FTrace::FTrace(const FPath& InPath)
: Path(InPath)
{
	const FString Name = GetName();
	Id = QuickStoreHash(*Name);

	std::error_code Ec;
	// Calculate that trace's timestamp. Bias in seconds then convert to 0.1us.
	std::filesystem::file_time_type LastWriteTime = std::filesystem::last_write_time(Path, Ec);
	auto LastWriteDuration = LastWriteTime.time_since_epoch();
	Timestamp = std::chrono::duration_cast<std::chrono::seconds>(LastWriteDuration).count();
	Timestamp += FsToUnrealEpochBiasSeconds;
	Timestamp *= 10'000'000;
}

////////////////////////////////////////////////////////////////////////////////
FString FStore::FTrace::GetName() const
{
	return FString((const char*)Path.stem().u8string().c_str());
}

////////////////////////////////////////////////////////////////////////////////
const FPath& FStore::FTrace::GetPath() const
{
	return Path;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStore::FTrace::GetId() const
{
	return Id;
}

////////////////////////////////////////////////////////////////////////////////
uint64 FStore::FTrace::GetSize() const
{
	std::error_code Ec;
	return std::filesystem::file_size(Path, Ec);
}

////////////////////////////////////////////////////////////////////////////////
uint64 FStore::FTrace::GetTimestamp() const
{
	return Timestamp;
}



////////////////////////////////////////////////////////////////////////////////
FStore::FMount::FMount(const FPath& InDir)
: Id(QuickStoreHash(InDir.c_str()))
{
	std::error_code ErrorCode;
	Dir = fs::absolute(InDir, ErrorCode);
	fs::create_directories(Dir, ErrorCode);
}

////////////////////////////////////////////////////////////////////////////////
FString FStore::FMount::GetDir() const
{
	return fs::ToFString(Dir);
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStore::FMount::GetId() const
{
	return Id;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStore::FMount::GetTraceCount() const
{
	return Traces.Num();
}

////////////////////////////////////////////////////////////////////////////////
const FStore::FTrace* FStore::FMount::GetTraceInfo(uint32 Index) const
{
	if (Index >= uint32(Traces.Num()))
	{
		return nullptr;
	}

	return Traces[Index];
}

////////////////////////////////////////////////////////////////////////////////
FStore::FTrace* FStore::FMount::GetTrace(uint32 Id) const
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
FStore::FTrace* FStore::FMount::AddTrace(const FPath& Path)
{
	FTrace NewTrace(Path);

	uint32 Id = NewTrace.GetId();
	if (FTrace* Existing = GetTrace(Id))
	{
		return Existing;
	}

	FTrace* Trace = new FTrace(MoveTemp(NewTrace));
	Traces.Add(Trace);
	return Trace;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStore::FMount::Refresh()
{
	Traces.Empty();

	uint32 ChangeSerial = 0;
	for (auto& DirItem : std::filesystem::directory_iterator(Dir))
	{
		if (DirItem.is_directory())
		{
			continue;
		}

		FPath Extension = DirItem.path().extension();
		if (Extension != ".utrace")
		{
			continue;
		}

		FTrace* Trace = AddTrace(DirItem.path());

		if (Trace != nullptr)
		{
			ChangeSerial += Trace->GetId();
		}
	}
	return ChangeSerial;
}



////////////////////////////////////////////////////////////////////////////////
FStore::FStore(asio::io_context& InIoContext, const FPath& InStoreDir)
: IoContext(InIoContext)
{
	FPath StoreDir = InStoreDir / "001";
	AddMount(StoreDir);

	Refresh();

#if TS_USING(TS_PLATFORM_WINDOWS)
	std::wstring StoreDirW = StoreDir;
	HANDLE DirWatchHandle = FindFirstChangeNotificationW(StoreDirW.c_str(), false, FILE_NOTIFY_CHANGE_FILE_NAME);
	if (DirWatchHandle == INVALID_HANDLE_VALUE)
	{
		DirWatchHandle = 0;
	}
	DirWatcher = new FDirWatcher(IoContext, DirWatchHandle);
#elif TS_USING(TS_PLATFORM_LINUX)
	int inotfd = inotify_init();
	int watch_desc = inotify_add_watch(inotfd, StoreDir.c_str(), IN_CREATE|IN_DELETE);
	DirWatcher = new FDirWatcher(IoContext, inotfd);
#elif PLATFORM_MAC
	DirWatcher = new FDirWatcher(*StoreDir);
#endif

	WatchDir();
}

////////////////////////////////////////////////////////////////////////////////
FStore::~FStore()
{
	if (DirWatcher != nullptr)
	{
		check(!DirWatcher->is_open());
		delete DirWatcher;
	}
}

////////////////////////////////////////////////////////////////////////////////
void FStore::Close()
{
	if (DirWatcher != nullptr)
	{
		DirWatcher->cancel();
		DirWatcher->close();
	}

	for (FMount* Mount : Mounts)
	{
		delete Mount;
	}
}

////////////////////////////////////////////////////////////////////////////////
bool FStore::AddMount(const FPath& Dir)
{
	FMount* Mount = new FMount(Dir);
	Mounts.Add(Mount);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FStore::RemoveMount(uint32 Id)
{
	for (uint32 i = 1, n = Mounts.Num() - 1; i <= n; ++i) // 1 because 0th must always exist
	{
		if (Mounts[i]->GetId() == Id)
		{
			std::swap(Mounts[n], Mounts[i]);
			Mounts.SetNum(n);
			Refresh();
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
const FStore::FMount* FStore::GetMount(uint32 Id) const
{
	for (FMount* Mount : Mounts)
	{
		if (Mount->GetId() == Id)
		{
			return Mount;
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStore::GetMountCount() const
{
	return uint32(Mounts.Num());
}

////////////////////////////////////////////////////////////////////////////////
const FStore::FMount* FStore::GetMountInfo(uint32 Index) const
{
	if (Index >= uint32(Mounts.Num()))
	{
		return nullptr;
	}

	return Mounts[Index];
}

////////////////////////////////////////////////////////////////////////////////
void FStore::WatchDir()
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

#if TS_USING(TS_PLATFORM_WINDOWS)
		// Windows doesn't update modified timestamps in a timely fashion when
		// copying files (or it could be Explorer that doesn't update it until
		// later). This is a not-so-pretty "wait for a little bit" workaround.
		auto* DelayTimer = new asio::steady_timer(IoContext);
		DelayTimer->expires_after(std::chrono::seconds(2));
		DelayTimer->async_wait([this, DelayTimer] (const asio::error_code& ErrorCode)
		{
			delete DelayTimer;

			Refresh();

			FindNextChangeNotification(DirWatcher->native_handle());
			WatchDir();
		});
#else
		Refresh();
		WatchDir();
#endif
	});
}

////////////////////////////////////////////////////////////////////////////////
FString FStore::GetStoreDir() const
{
	FMount* Mount = Mounts[0];
	return Mount->GetDir();
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStore::GetChangeSerial() const
{
	return ChangeSerial;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStore::GetTraceCount() const
{
	uint32 Count = 0;
	for (const FMount* Mount : Mounts)
	{
		Count += Mount->GetTraceCount();
	}
	return Count;
}

////////////////////////////////////////////////////////////////////////////////
const FStore::FTrace* FStore::GetTraceInfo(uint32 Index) const
{
	for (const FMount* Mount : Mounts)
	{
		uint32 Count = Mount->GetTraceCount();
		if (Index < Count)
		{
			return Mount->GetTraceInfo(Index);
		}
		Index -= Count;
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
FStore::FTrace* FStore::GetTrace(uint32 Id, FMount** OutMount) const
{
	for (FMount* Mount : Mounts)
	{
		if (FTrace* Trace = Mount->GetTrace(Id))
		{
			if (OutMount != nullptr)
			{
				*OutMount = Mount;
			}
			return Trace;
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
FStore::FNewTrace FStore::CreateTrace()
{
	// N.B. Not thread safe!?
	char Prefix[24];
	std::time_t Now = std::time(nullptr);
	std::tm* LocalNow = std::localtime(&Now);
	std::strftime(Prefix, TS_ARRAY_COUNT(Prefix), "%Y%m%d_%H%M%S", LocalNow);

	FMount* DefaultMount = Mounts[0];

	FPath TracePath(*DefaultMount->GetDir());
	TracePath /= Prefix;
	TracePath += ".utrace";
	
	for (uint32 Index = 0; std::filesystem::is_regular_file(TracePath); ++Index)
	{
		char FilenameIndexed[64];
		std::sprintf(FilenameIndexed, "%s_%02d.utrace", Prefix, Index);
		TracePath.replace_filename(FPath(FilenameIndexed));
	}

	FAsioWriteable* File = FAsioFile::WriteFile(IoContext, TracePath);
	if (File == nullptr)
	{
		return {};
	}

	FTrace* Trace = DefaultMount->AddTrace(TracePath);
	if (Trace == nullptr)
	{
		delete File;
		return {};
	}

	return { Trace->GetId(), File };
}

////////////////////////////////////////////////////////////////////////////////
bool FStore::HasTrace(uint32 Id) const
{
	return GetTrace(Id) != nullptr;
}

////////////////////////////////////////////////////////////////////////////////
FAsioReadable* FStore::OpenTrace(uint32 Id)
{
	FMount* Mount;
	FTrace* Trace = GetTrace(Id, &Mount);
	if (Trace == nullptr)
	{
		return nullptr;
	}

	return FAsioFile::ReadFile(IoContext, Trace->GetPath());
}

////////////////////////////////////////////////////////////////////////////////
void FStore::Refresh()
{
	ChangeSerial = 0;
	for (FMount* Mount : Mounts)
	{
		ChangeSerial += Mount->Refresh();
	}
}

/* vim: set noexpandtab : */

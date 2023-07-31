// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pch.h"
#include "StoreService.h"
#include "Utils.h"
#include "Version.h"

#include <cxxopts.hpp>

#if TS_USING(TS_BUILD_DEBUG)
#	include <thread>
#endif

#if TS_USING(TS_PLATFORM_LINUX) || TS_USING(TS_PLATFORM_MAC)
#	include <pwd.h>
#	include <semaphore.h>
#	include <sched.h>
#	include <signal.h>
#	include <cstdarg>
#	include <sys/file.h>
#	include <sys/mman.h>
#	include <sys/stat.h>
#	include <sys/wait.h>
#	include <unistd.h>
#endif

// Debug builds act as both the forker and the daemon. Set to TS_OFF to disable
// this behaviour.
#if TS_USING(TS_BUILD_DEBUG) //&&0
#	define TS_DAEMON_THREAD TS_ON
#else
#	define TS_DAEMON_THREAD TS_OFF
#endif

// {{{1 misc -------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FMmapScope
{
public:
								FMmapScope(void* InPtr, uint32 InLength=0);
								~FMmapScope();
	template <typename T> T*	As() const;

private:
	void*						Ptr;
	int32						Length;

private:
								FMmapScope() = delete;
								FMmapScope(const FMmapScope&) = delete;
								FMmapScope(const FMmapScope&&) = delete;
	FMmapScope&					operator = (const FMmapScope&) = delete;
	FMmapScope&					operator = (const FMmapScope&&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
FMmapScope::FMmapScope(void* InPtr, uint32 InLength)
: Ptr(InPtr)
, Length(InLength)
{
}

////////////////////////////////////////////////////////////////////////////////
FMmapScope::~FMmapScope()
{
#if TS_USING(TS_PLATFORM_WINDOWS)
	UnmapViewOfFile(Ptr);
#elif TS_USING(TS_PLATFORM_LINUX) || TS_USING(TS_PLATFORM_MAC)
	munmap(Ptr, Length);
#endif
}

////////////////////////////////////////////////////////////////////////////////
template <typename T>
T* FMmapScope::As() const
{
	return (T*)Ptr;
}



////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct TOnScopeExit
{
		TOnScopeExit()	= default;
		~TOnScopeExit()	{ (*Lambda)(); }
	T*	Lambda;
};

#define OnScopeExit(x) \
	auto TS_CONCAT(OnScopeExitFunc, __LINE__) = x; \
	TOnScopeExit<decltype(TS_CONCAT(OnScopeExitFunc, __LINE__))> TS_CONCAT(OnScopeExitInstance, __LINE__); \
	do { TS_CONCAT(OnScopeExitInstance, __LINE__).Lambda = &TS_CONCAT(OnScopeExitFunc, __LINE__); } while (0)



////////////////////////////////////////////////////////////////////////////////
static void GetUnrealTraceHome(FPath& Out, bool Make=false)
{
#if TS_USING(TS_PLATFORM_WINDOWS)
	wchar_t Buffer[MAX_PATH];
	auto Ok = SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, Buffer);
	if (Ok != S_OK)
	{
		uint32 Ret = GetEnvironmentVariableW(L"USERPROFILE", Buffer, TS_ARRAY_COUNT(Buffer));
		if (Ret == 0 || Ret >= TS_ARRAY_COUNT(Buffer))
		{
			return;
		}
	}
	Out = Buffer;
	Out /= "UnrealEngine/Common/UnrealTrace";
#else
	int UserId = getuid();
	const passwd* Passwd = getpwuid(UserId);
	Out = Passwd->pw_dir;
	Out /= "UnrealEngine/UnrealTrace";
#endif

	if (Make)
	{
		std::error_code ErrorCode;
		std::filesystem::create_directories(Out, ErrorCode);
	}
}




// {{{1 logging ----------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
#define TS_LOG(Format, ...) \
	do { FLogging::Log(Format "\n", ##__VA_ARGS__); } while (false)

////////////////////////////////////////////////////////////////////////////////
class FLogging
{
public:
	static void			Initialize();
	static void			Shutdown();
	static void			Log(const char* Format, ...);

private:
						FLogging();
						~FLogging();
						FLogging(const FLogging&) = delete;
						FLogging(FLogging&&) = default;
	void				LogImpl(const char* String) const;
	static FLogging*	Instance;
	FILE*				File = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
FLogging* FLogging::Instance = nullptr;

////////////////////////////////////////////////////////////////////////////////
FLogging::FLogging()
{
	// Find where the logs should be written to. Make sure it exists.
	FPath LogDir;
	GetUnrealTraceHome(LogDir, true);

	// Fetch all existing logs.
	struct FExistingLog
	{
		FPath	Path;
		uint32					Index;

		int32 operator < (const FExistingLog& Rhs) const
		{
			return Index < Rhs.Index;
		}
	};
	std::vector<FExistingLog> ExistingLogs;
	if (std::filesystem::is_directory(LogDir))
	{
		for (const auto& DirItem : std::filesystem::directory_iterator(LogDir))
		{
			int32 Index = -1;
			std::string StemUtf8 = DirItem.path().stem().string();
			sscanf(StemUtf8.c_str(), "Server_%d", &Index);
			if (Index >= 0)
			{
				ExistingLogs.push_back({DirItem.path(), uint32(Index)});
			}
		}
	}

	// Sort and try and tidy up old logs.
	static int32 MaxLogs = 12; // plus one new one
	std::sort(ExistingLogs.begin(), ExistingLogs.end());
	for (int32 i = 0, n = int32(ExistingLogs.size() - MaxLogs); i < n; ++i)
	{
		std::error_code ErrorCode;
		std::filesystem::remove(ExistingLogs[i].Path, ErrorCode);
	}


	// Open the log file (note; can race other instances)
	uint32 LogIndex = ExistingLogs.empty() ? 0 : ExistingLogs.back().Index;
	for (uint32 n = LogIndex + 10; File == nullptr && LogIndex < n;)
	{
		++LogIndex;
		char LogName[128];
		snprintf(LogName, TS_ARRAY_COUNT(LogName), "Server_%d.log", LogIndex);
		FPath LogPath = LogDir / LogName;

#if TS_USING(TS_PLATFORM_WINDOWS)
		File = _wfopen(LogPath.c_str(), L"wbxN");
#else
		File = fopen(LogPath.c_str(), "wbx");
#endif
	}
}

////////////////////////////////////////////////////////////////////////////////
FLogging::~FLogging()
{
	if (File != nullptr)
	{
		fclose(File);
	}
}

////////////////////////////////////////////////////////////////////////////////
void FLogging::Initialize()
{
	if (Instance != nullptr)
	{
		return;
	}

	Instance = new FLogging();
}

////////////////////////////////////////////////////////////////////////////////
void FLogging::Shutdown()
{
	if (Instance == nullptr)
	{
		return;
	}

	delete Instance;
	Instance = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
void FLogging::LogImpl(const char* String) const
{
	if (File != nullptr)
	{
		fputs(String, File);
		fflush(File);
	}

	fputs(String, stdout);
}

////////////////////////////////////////////////////////////////////////////////
void FLogging::Log(const char* Format, ...)
{
	va_list VaList;
	va_start(VaList, Format);

	char Buffer[320];
	vsnprintf(Buffer, TS_ARRAY_COUNT(Buffer), Format, VaList);
	Buffer[TS_ARRAY_COUNT(Buffer) - 1] = '\0';

	Instance->LogImpl(Buffer);

	va_end(VaList);
}



////////////////////////////////////////////////////////////////////////////////
struct FLoggingScope
{
	FLoggingScope()		{ FLogging::Initialize(); }
	~FLoggingScope()	{ FLogging::Shutdown(); }
};



// {{{1 store ------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
struct FStoreOptions
{
	FPath		Dir;
	int			Port			= 1989;
	int			RecorderPort	= 1981;
};

////////////////////////////////////////////////////////////////////////////////
static FStoreOptions ParseOptions(int ArgC, char** ArgV)
{
	cxxopts::Options Options("UnrealTraceServer", "Unreal Trace Server");
	Options.add_options()
		("storedir", "Default store directory", cxxopts::value<std::string>()->default_value(""))
		("port",	"TCP port to serve the store on",			cxxopts::value<int>()->default_value("0"))
		("recport",	"TCP port for the recorder to listen on",	cxxopts::value<int>()->default_value("0"))
		;

	auto Parsed = Options.parse(ArgC, ArgV);

	FStoreOptions Ret;
	if (int Value = Parsed["port"].as<int>())
	{
		Ret.Port = Value;
	}

	if (int Value = Parsed["recport"].as<int>())
	{
		Ret.RecorderPort = Value;
	}

	std::string Value = Parsed["storedir"].as<std::string>();
	if (!Value.empty())
	{
		std::error_code ErrorCode;
		Ret.Dir = fs::absolute(Value, ErrorCode);
	}

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
static FStoreService* StartStore(const FStoreOptions& Options)
{
	FStoreService::FDesc Desc;
	Desc.StoreDir = Options.Dir;
	Desc.StorePort = Options.Port;
	Desc.RecorderPort = Options.RecorderPort;
	return FStoreService::Create(Desc);
}



// {{{1 instance-info ----------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
struct FInstanceInfo
{
public:
	static const uint32	CurrentVersion =
#if TS_USING(TS_BUILD_DEBUG)
		0x8000'0000 |
#endif
		((TS_VERSION_PROTOCOL & 0xffff) << 16) | (TS_VERSION_MINOR & 0xffff);

	void				Set();
	void				WaitForReady() const;
	bool				IsOlder() const;
	std::atomic<uint32> Published;
	uint32				Version;
	uint32				Pid;
};

////////////////////////////////////////////////////////////////////////////////
void FInstanceInfo::Set()
{
	Version = CurrentVersion;
#if TS_USING(TS_PLATFORM_WINDOWS)
	Pid = GetCurrentProcessId();
#elif TS_USING(TS_PLATFORM_LINUX) || TS_USING(TS_PLATFORM_MAC)
	Pid = getpid();
#endif
	Published.fetch_add(1, std::memory_order_release);
}

////////////////////////////////////////////////////////////////////////////////
void FInstanceInfo::WaitForReady() const
{
	// Spin until this instance info is published (by another process)
#if TS_USING(TS_PLATFORM_WINDOWS)
	for (;; Sleep(0))
#else
	for (;; sched_yield())
#endif
	{
		if (Published.load(std::memory_order_acquire))
		{
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
bool FInstanceInfo::IsOlder() const
{
	// Decide which is older; this compiled code or the instance we have a
	// pointer to.
	bool bIsOlder = false;
	bIsOlder |= (Version < FInstanceInfo::CurrentVersion);
	return bIsOlder;
}



// {{{1 return codes -----------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
enum : int
{
	Result_Ok				= 0,
	Result_BegunCreateFail,
	Result_BegunExists,
	Result_BegunTimeout,
	Result_CopyFail,
	Result_ForkFail,
	Result_LaunchFail,
	Result_NoQuitEvent,
	Result_ProcessOpenFail,
	Result_QuitExists,
	Result_RenameFail,
	Result_SharedMemFail,
	Result_SharedMemTruncFail,
	Result_OpenFailPid,
	Result_ReadFailPid,
	Result_LockClaimFail,
	Result_UnexpectedError,
};



// {{{1 windows ----------------------------------------------------------------

#if TS_USING(TS_PLATFORM_WINDOWS)
////////////////////////////////////////////////////////////////////////////////
class FWinHandle
{
public:
	FWinHandle(HANDLE InHandle)
	: Handle(InHandle)
	{
		if (Handle == INVALID_HANDLE_VALUE)
		{
			Handle = nullptr;
		}
	}

				~FWinHandle()				{ if (Handle) CloseHandle(Handle); }
				operator HANDLE () const	{ return Handle; }
	bool		IsValid() const				{ return Handle != nullptr; }

private:
	HANDLE		Handle;
	FWinHandle&	operator = (const FWinHandle&) = delete;
				FWinHandle(const FWinHandle&) = delete;
				FWinHandle(const FWinHandle&&) = delete;
};



////////////////////////////////////////////////////////////////////////////////
static bool LaunchUnfancy(const wchar_t* Binary, wchar_t* CommandLine)
{
	/* No frills spawning of a child process */

	uint32 CreateProcFlags = CREATE_BREAKAWAY_FROM_JOB;
	STARTUPINFOW StartupInfo = { sizeof(STARTUPINFOW) };
	PROCESS_INFORMATION ProcessInfo = {};

	BOOL bOk = CreateProcessW(Binary, CommandLine, nullptr, nullptr, FALSE,
		CreateProcFlags, nullptr, nullptr, &StartupInfo, &ProcessInfo);

	if (bOk == FALSE)
	{
		return false;
	}

	CloseHandle(ProcessInfo.hProcess);
	CloseHandle(ProcessInfo.hThread);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
static bool LaunchUnprivileged(const wchar_t* Binary, wchar_t* CommandLine)
{
	/* Ordinary child process without inheriting any administrator rights */

	uint32 CreateProcFlags = CREATE_BREAKAWAY_FROM_JOB;
	STARTUPINFOW StartupInfo = { sizeof(STARTUPINFOW) };
	PROCESS_INFORMATION ProcessInfo = {};

	SAFER_LEVEL_HANDLE SafeLevel = nullptr;
	BOOL bOk = SaferCreateLevel(SAFER_SCOPEID_USER,
		SAFER_LEVELID_NORMALUSER, SAFER_LEVEL_OPEN, &SafeLevel, nullptr);
	if (bOk == FALSE)
	{
		return false;
	}

	HANDLE AccessToken;
	if (SaferComputeTokenFromLevel(SafeLevel, nullptr, &AccessToken, 0, nullptr))
	{
		bOk = CreateProcessAsUserW(AccessToken, Binary, CommandLine,
			nullptr, nullptr, FALSE, CreateProcFlags, nullptr, nullptr,
			&StartupInfo, &ProcessInfo);

		CloseHandle(AccessToken);
	}

	SaferCloseLevel(SafeLevel);
	return (bOk == TRUE);
}

////////////////////////////////////////////////////////////////////////////////
static bool LaunchUnelevated(const wchar_t* Binary, wchar_t* CommandLine)
{
	/* Launches a binary with the shell as its parent. The shell (such as
	   Explorer) should be an unelevated process. */

	// No sense in using this route if we are not elevated in the first place
	if (IsUserAnAdmin() == FALSE)
	{
		return LaunchUnprivileged(Binary, CommandLine);
	}

	// Get the users' shell process and open it for process creation
	HWND ShellWnd = GetShellWindow();
	if (ShellWnd == nullptr)
	{
		return false;
	}

	DWORD ShellPid;
	GetWindowThreadProcessId(ShellWnd, &ShellPid);

	FWinHandle Process = OpenProcess(PROCESS_CREATE_PROCESS, FALSE, ShellPid);
	if (!Process.IsValid())
	{
		return false;
	}

	// Creating a process as a child of another process is done by setting a
	// thread-attribute list on the startup info passed to CreateProcess()
	SIZE_T AttrListSize;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &AttrListSize);

	auto AttrList = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(AttrListSize);
	OnScopeExit([&] () { free(AttrList); });

	if (!InitializeProcThreadAttributeList(AttrList, 1, 0, &AttrListSize))
	{
		return false;
	}

	BOOL bOk = UpdateProcThreadAttribute(AttrList, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
		(HANDLE*)&Process, sizeof(Process), nullptr, nullptr);
	if (!bOk)
	{
		return false;
	}

	// By this point we know we are an elevated process. It is not allowed to
	// create a process as a child of another unelevated process that share our
	// elevated console window if we have one. So we'll need to create a new one.
	uint32 CreateProcFlags = CREATE_BREAKAWAY_FROM_JOB|EXTENDED_STARTUPINFO_PRESENT;
	if (GetConsoleWindow() != nullptr)
	{
		CreateProcFlags |= CREATE_NEW_CONSOLE;
	}
	else
	{
		CreateProcFlags |= DETACHED_PROCESS;
	}

	// Everything is set up now so we can proceed and launch the process
	STARTUPINFOEXW StartupInfo = { sizeof(STARTUPINFOEXW) };
	StartupInfo.lpAttributeList = AttrList;
	PROCESS_INFORMATION ProcessInfo = {};

	bOk = CreateProcessW(Binary, CommandLine, nullptr, nullptr, FALSE,
		CreateProcFlags, nullptr, nullptr, &StartupInfo.StartupInfo, &ProcessInfo);
	if (bOk == FALSE)
	{
		return false;
	}

	CloseHandle(ProcessInfo.hProcess);
	CloseHandle(ProcessInfo.hThread);
	return true;
}



////////////////////////////////////////////////////////////////////////////////
static const wchar_t*	GIpcName					= L"Local\\UnrealTraceInstance";
static const int32		GIpcSize					= 4 << 10;
static const wchar_t*	GQuitEventName				= L"Local\\UnrealTraceEvent";
static const wchar_t*	GBegunEventName				= L"Local\\UnrealTraceEventBegun";
static int				MainDaemon(int, char**);
void					AddToSystemTray(FStoreService&);
void					RemoveFromSystemTray();

////////////////////////////////////////////////////////////////////////////////
static int CreateExitCode(uint32 Id)
{
	return (GetLastError() & 0xfff) | (Id << 12);
}

////////////////////////////////////////////////////////////////////////////////
static int MainKillImpl(int ArgC, char** ArgV, const FInstanceInfo* InstanceInfo)
{
	// Signal to the existing instance to shutdown or forcefully do it if it
	// does not respond in time.

	TS_LOG("Opening quit event");
	FWinHandle QuitEvent = CreateEventW(nullptr, TRUE, FALSE, GQuitEventName);
	if (GetLastError() != ERROR_ALREADY_EXISTS)
	{
		TS_LOG("Not found (gle=%d)", GetLastError());
		return Result_NoQuitEvent;
	}

	TS_LOG("Open the process %d", InstanceInfo->Pid);
	DWORD OpenProcessFlags = PROCESS_TERMINATE | SYNCHRONIZE;
	FWinHandle ProcHandle = OpenProcess(OpenProcessFlags, FALSE, InstanceInfo->Pid);
	if (!ProcHandle.IsValid())
	{
		TS_LOG("Unsuccessful (gle=%d)", GetLastError());
		return CreateExitCode(Result_ProcessOpenFail);
	}

	TS_LOG("Firing quit event and waiting for process");
	SetEvent(QuitEvent);

	if (WaitForSingleObject(ProcHandle, 5000) == WAIT_TIMEOUT)
	{
		TS_LOG("Timeout. Force terminating");
		TerminateProcess(ProcHandle, 10);
	}

	return Result_Ok;
}

////////////////////////////////////////////////////////////////////////////////
static int MainKill(int ArgC, char** ArgV)
{
	// Find if an existing instance is already running.
	FWinHandle IpcHandle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, false, GIpcName);
	if (!IpcHandle.IsValid())
	{
		TS_LOG("All good. There was no active UTS process");
		return Result_Ok;
	}

	// There is an instance running so we can get its info block
	void* IpcPtr = MapViewOfFile(IpcHandle, FILE_MAP_ALL_ACCESS, 0, 0, GIpcSize);
	FMmapScope MmapScope(IpcPtr);
	const auto* InstanceInfo = MmapScope.As<const FInstanceInfo>();
	InstanceInfo->WaitForReady();

	return MainKillImpl(ArgC, ArgV, InstanceInfo);
}

////////////////////////////////////////////////////////////////////////////////
static int MainFork(int ArgC, char** ArgV)
{
	// Check for an existing instance that is already running.
	TS_LOG("Opening exist instance's shared memory");
	FWinHandle IpcHandle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, false, GIpcName);
	if (IpcHandle.IsValid())
	{
		void* IpcPtr = MapViewOfFile(IpcHandle, FILE_MAP_ALL_ACCESS, 0, 0, GIpcSize);
		FMmapScope MmapScope(IpcPtr);

		const auto* InstanceInfo = MmapScope.As<const FInstanceInfo>();
		InstanceInfo->WaitForReady();
#if TS_USING(TS_BUILD_DEBUG)
		if (false)
#else
		if (!InstanceInfo->IsOlder())
#endif
		{
			TS_LOG("Existing instance is the same age or newer");
			return Result_Ok;
		}

		// Kill the other instance.
		int KillRet = MainKillImpl(0, nullptr, InstanceInfo);
		if (KillRet == Result_NoQuitEvent)
		{
			// If no quit event was found then we shall assume that another new
			// store instance beat us to it.
			TS_LOG("Looks like someone else has already taken care of the upgrade");
			return Result_Ok;
		}

		if (KillRet != Result_Ok)
		{
			TS_LOG("Kill attempt failed (ret=%d)", KillRet);
			return KillRet;
		}
	}
	else
	{
		TS_LOG("No existing process/shared memory found");
	}

	// Get this binary's path
	TS_LOG("Getting binary path");
	wchar_t BinPath[MAX_PATH];
	uint32 BinPathLen = GetModuleFileNameW(nullptr, BinPath, TS_ARRAY_COUNT(BinPath));
	if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		// This should never really happen...
		TS_LOG("MAX_PATH is not enough");
		return CreateExitCode(Result_UnexpectedError);
	}
	TS_LOG("Binary located at '%ls'", BinPath);

	// Calculate where to store the binaries.
	FPath DestPath;
	GetUnrealTraceHome(DestPath);
	{
		wchar_t Buffer[64];
		_snwprintf(Buffer, TS_ARRAY_COUNT(Buffer), L"Bin/%08x/UnrealTraceServer.exe", FInstanceInfo::CurrentVersion);
		DestPath /= Buffer;
	}
	TS_LOG("Run path '%ls'", DestPath.c_str());

#if TS_USING(TS_BUILD_DEBUG)
	// Debug builds will always do the copy.
	{
		std::error_code ErrorCode;
		std::filesystem::remove(DestPath, ErrorCode);
	}
#endif

	// Copy the binary out to a location where it doesn't matter if the file
	// gets locked by the OS.
	if (!std::filesystem::is_regular_file(DestPath))
	{
		TS_LOG("Copying to run path");

		std::error_code ErrorCode;
		std::filesystem::create_directories(DestPath.parent_path(), ErrorCode);

		// Tag the destination with our PID and copy
		DWORD OurPid = GetCurrentProcessId();
		wchar_t Buffer[16];
		_snwprintf(Buffer, TS_ARRAY_COUNT(Buffer), L"_%08x", OurPid);
		FPath TempPath = DestPath;
		TempPath += Buffer;
		if (!std::filesystem::copy_file(BinPath, TempPath))
		{
			TS_LOG("File copy failed (gle=%d)", GetLastError());
			return CreateExitCode(Result_CopyFail);
		}

		// Move the file into place. If this fails because the file exists then
		// another instance has beaten us to the punch.
		std::filesystem::rename(TempPath, DestPath, ErrorCode);
		if (ErrorCode)
		{
			bool bRaceLost = (ErrorCode == std::errc::file_exists);
			TS_LOG("Rename to destination failed (bRaceLost=%c)", bRaceLost);
			return bRaceLost ? Result_Ok : CreateExitCode(Result_RenameFail);
		}
	}
	else
	{
		TS_LOG("Already exists");
	}

	// Launch a new instance as a daemon and wait until we know it has started
	TS_LOG("Creating begun event");
	FWinHandle BegunEvent = CreateEventW(nullptr, TRUE, FALSE, GBegunEventName);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		TS_LOG("Did not work (gle=%d)", GetLastError());
		return CreateExitCode(Result_BegunExists);
	}

	// For debugging ease and consistency we will daemonize in this process
	// instead of spawning a second one.
#if TS_USING(TS_DAEMON_THREAD)
	std::thread DaemonThread([=] () { MainDaemon(ArgC, ArgV); });
#else
	std::wstring CommandLine = L"UnrealTraceServer.exe daemon";
	for (int i = 1; i < ArgC; ++i)
	{
		CommandLine += L"\"";
		CommandLine += FWinApiStr(ArgV[i]);
		CommandLine += L"\"";
	}
	if (!LaunchUnelevated(DestPath.c_str(), CommandLine.data()))
	{
		TS_LOG("Unelevated launch failed (gle=%d)", GetLastError());
		if (!LaunchUnprivileged(DestPath.c_str(), CommandLine.data()))
		{
			TS_LOG("Unprivileged launch failed (gle=%d)", GetLastError());
			if (!LaunchUnfancy(DestPath.c_str(), CommandLine.data()))
			{
				TS_LOG("Launch failed (gle=%d)", GetLastError());
				return CreateExitCode(Result_LaunchFail);
			}
		}
	}
#endif // !TS_BUILD_DEBUG

	TS_LOG("Waiting on begun event");
	int Ret = Result_Ok;
	if (WaitForSingleObject(BegunEvent, 5000) == WAIT_TIMEOUT)
	{
		TS_LOG("Wait timed out (gle=%d)", GetLastError());
		Ret = CreateExitCode(Result_BegunTimeout);
	}

#if TS_USING(TS_DAEMON_THREAD)
	static bool volatile bShouldExit = false;
	while (true)
	{
		Sleep(500);

		if (bShouldExit)
		{
			FWinHandle QuitEvent = CreateEventW(nullptr, TRUE, FALSE, GQuitEventName);
			SetEvent(QuitEvent);
			break;
		}
	}

	DaemonThread.join();
#endif

	TS_LOG("Complete (ret=%d)", Ret);
	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
static int MainDaemon(int ArgC, char** ArgV)
{
	// Move the working directory to be where this binary is located.
	TS_LOG("Setting working directory");
	wchar_t BinPath[MAX_PATH];
	uint32 BinPathLen = GetModuleFileNameW(nullptr, BinPath, TS_ARRAY_COUNT(BinPath));
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER && BinPathLen > 0)
	{
		std::error_code ErrorCode;
		FPath BinDir(BinPath);
		BinDir = BinDir.parent_path();
		std::filesystem::current_path(BinDir, ErrorCode);
		const char* Result = ErrorCode ? "Failed" : "Succeeded";
		TS_LOG("%s setting '%ls' (gle=%d)", Result, BinDir.c_str(), GetLastError());
	}
	else
	{
		TS_LOG("Something went wrong (gle=%d)", GetLastError());
	}

	// Create a piece of shared memory so all store instances can communicate.
	TS_LOG("Creating some shared memory");
	FWinHandle IpcHandle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
		PAGE_READWRITE, 0, GIpcSize, GIpcName);
	if (!IpcHandle.IsValid())
	{
		TS_LOG("Creation unsuccessful (gle=%d)", GetLastError());
		return CreateExitCode(Result_SharedMemFail);
	}

	// Create a named event so others can tell us to quit.
	TS_LOG("Creating a quit event");
	FWinHandle QuitEvent = CreateEventW(nullptr, TRUE, FALSE, GQuitEventName);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		// This really should not happen. It is expected that only one process
		// will get this far (gated by the shared-memory object creation).
		TS_LOG("It unexpectedly exists already");
		return CreateExitCode(Result_QuitExists);
	}

	// Fill out the Ipc details and publish.
	void* IpcPtr = MapViewOfFile(IpcHandle, FILE_MAP_ALL_ACCESS, 0, 0, GIpcSize);
	{
		TS_LOG("Writing shared instance info");
		FMmapScope MmapScope(IpcPtr);
		auto* InstanceInfo = MmapScope.As<FInstanceInfo>();
		InstanceInfo->Set();
	}

	// Fire up the store
	TS_LOG("Starting the store");
	FStoreService* StoreService;
	{
		FStoreOptions Options = ParseOptions(ArgC, ArgV);

		if (Options.Dir.empty())
		{
			FPath StoreDir;
			GetUnrealTraceHome(StoreDir);
			StoreDir /= "Store";
			Options.Dir = StoreDir;
		}

		StoreService = StartStore(Options);
	}

	// Let every one know we've started.
	{
		FWinHandle BegunEvent = CreateEventW(nullptr, TRUE, FALSE, GBegunEventName);
		if (BegunEvent.IsValid())
		{
			SetEvent(BegunEvent);
		}
	}

	// To clearly indicate to users that we are around we'll add an icon to the
	// system tray.
	AddToSystemTray(*StoreService);

	// Wait to be told to resign.
	WaitForSingleObject(QuitEvent, INFINITE);

	// Clean up. We are done here.
	RemoveFromSystemTray();
	delete StoreService;
	return Result_Ok;
}

////////////////////////////////////////////////////////////////////////////////
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// Get command line arguments and convert to UTF8
	const wchar_t* CommandLine = GetCommandLineW();
	int ArgC;
	wchar_t** WideArgV = CommandLineToArgvW(CommandLine, &ArgC);

	TInlineBuffer<320> ArgBuffer;
	char** ArgV = (char**)(ArgBuffer->Append(ArgC * sizeof(char*)));
	for (int i = 0; i < ArgC; ++i)
	{
		const wchar_t* WideArg = WideArgV[i];
		int32 ArgSize = WideCharToMultiByte(CP_UTF8, 0, WideArg, -1, nullptr, 0, nullptr, nullptr);

		char* Arg = (char*)(ArgBuffer->Append(ArgSize));
		WideCharToMultiByte(CP_UTF8, 0, WideArg, -1, Arg, ArgSize, nullptr, nullptr);

		ArgV[i] = Arg;
	}

	// The proper entry point
	extern int main(int, char**);
	int Ret = main(ArgC, ArgV);

	// Clean up
	LocalFree(ArgV);
	return Ret;
}

#endif // TS_PLATFORM_WINDOWS



// {{{1 linux/mac --------------------------------------------------------------

#if TS_USING(TS_PLATFORM_LINUX) || TS_USING(TS_PLATFORM_MAC)

////////////////////////////////////////////////////////////////////////////////
static int MainDaemon(int, char**, pid_t);

////////////////////////////////////////////////////////////////////////////////
static FPath GetLockFilePath()
{
	return "/tmp/UnrealTraceServer.pid";
}

////////////////////////////////////////////////////////////////////////////////
static int MainKillImpl(int ArgC, char** ArgV, pid_t DaemonPid)
{
	// Issue the terminate signal
	TS_LOG("Sending SIGTERM to %d", DaemonPid);
	if (kill(DaemonPid, SIGTERM) < 0)
	{
		TS_LOG("Failed to send SIGTERM");
		return Result_SharedMemFail;
	}

	// Wait for the process to end. If it takes too long, kill it.
	TS_LOG("Waiting for pid %d", DaemonPid);
	const uint32 SleepMs = 47;
	timespec SleepTime = { 0, SleepMs * 1000 * 1000 };
	nanosleep(&SleepTime, nullptr);
	for (uint32 i = 0; ; i += SleepMs)
	{
		if (i >= 5000) // "5000" for grep-ability
		{
			kill(DaemonPid, SIGKILL);
			TS_LOG("Timed out. Sent SIGKILL instead (errno=%d)", errno);
			break;
		}

		if (kill(DaemonPid, 0) < 0)
		{
			if (errno == ESRCH)
			{
				TS_LOG("Process no longer exists");
				break;
			}
		}

		nanosleep(&SleepTime, nullptr);
	}

	return Result_Ok;
}

////////////////////////////////////////////////////////////////////////////////
static int MainKill(int ArgC, char** ArgV)
{
	// Open the pid file to detect an existing instance
	FPath DotPidPath = GetLockFilePath();
	TS_LOG("Checking for a '%s' lock file", DotPidPath.c_str());
	int DotPidFd = open(DotPidPath.c_str(), O_RDONLY);
	if (DotPidFd < 0)
	{
		if (errno == ENOENT)
		{
			TS_LOG("All good. Ain't nuffin' running me ol' mucker.");
			return Result_Ok;
		}

		TS_LOG("Unable to open lock file (%s, errno=%d)", DotPidPath.c_str(), errno);
		return Result_OpenFailPid;
	}
	OnScopeExit([DotPidFd] { close(DotPidFd); });

	// If we can claim the write lock then lock file is orphaned.
	struct flock FileLock = { .l_type = F_WRLCK };
	int Result = fcntl(DotPidFd, F_GETLK, &FileLock);
	if (Result != 0 || FileLock.l_type == F_UNLCK)
	{
		TS_LOG("Lock file appears to be orphaned. Removing");
		std::error_code ErrorCode;
		std::filesystem::remove(DotPidPath, ErrorCode);
		return Result_Ok;
	}

	return MainKillImpl(ArgC, ArgV, FileLock.l_pid);
}

////////////////////////////////////////////////////////////////////////////////
static int MainFork(int ArgC, char** ArgV)
{
	// Open the pid file to detect an existing instance
	FPath DotPidPath = GetLockFilePath();
	TS_LOG("Checking for a '%s' lock file", DotPidPath.c_str());
	for (int DotPidFd = open(DotPidPath.c_str(), O_RDONLY); DotPidFd >= 0; )
	{
		OnScopeExit([DotPidFd] () { close(DotPidFd); });

		// If we can claim a write lock then the lock file is orphaned
		struct flock FileLock = { .l_type = F_WRLCK };
		int Result = fcntl(DotPidFd, F_GETLK, &FileLock);
		if (Result != 0 || FileLock.l_type == F_UNLCK)
		{
			TS_LOG("Lock file appears to be orphaned. Unlinking it");
			std::error_code ErrorCode;
			std::filesystem::remove(DotPidPath, ErrorCode);
			break;
		}

		// Get the instance info from the buffer
		char DotPidBuffer[sizeof(FInstanceInfo)];
		Result = read(DotPidFd, DotPidBuffer, sizeof(DotPidBuffer));
		if (Result < sizeof(FInstanceInfo))
		{
			TS_LOG("Failed to read the .pid lock file (errno=%d)", errno);
			return Result_ReadFailPid;
		}
		const auto* InstanceInfo = (const FInstanceInfo*)DotPidBuffer;

		// Old enough for this fine establishment?
		if (!InstanceInfo->IsOlder())
		{
			TS_LOG("Existing instance is the same age or newer");
			return Result_Ok;
		}

		// If we've got this far then there's an instance running that is old
		TS_LOG("Killing an older instance that is already running");
		int KillRet = MainKillImpl(0, nullptr, FileLock.l_pid);
		if (KillRet == Result_NoQuitEvent)
		{
			// If no quit event was found then we shall assume that another new
			// store instance beat us to it.
			TS_LOG("Looks like someone else has already taken care of the upgrade");
			return Result_Ok;
		}

		if (KillRet != Result_Ok)
		{
			TS_LOG("Kill attempt failed (ret=%d)", KillRet);
			return KillRet;
		}
	}

	// Launch a daemonized version of ourselves. For debugging ease and
	// consistency we will daemonize in this process instead of spawning
	// a second one.
	pid_t DaemonPid = -1;
#if TS_USING(TS_BUILD_DEBUG)
	std::thread DaemonThread([=] () { MainDaemon(ArgC, ArgV, 0); });
#else
	TS_LOG("Forking process");
	pid_t ParentPid = getpid();
	DaemonPid = fork();
	if (DaemonPid < 0)
	{
		TS_LOG("Failed (errno=%d)", errno);
		return Result_ForkFail;
	}
	else if (DaemonPid == 0)
	{
		return MainDaemon(ArgC, ArgV, ParentPid);
	}
#endif // TS_BUILD_DEBUG

	// Wait for the daemon to indicate that it has started the store.
	int Ret = Result_Ok;
#if TS_USING(TS_BUILD_DEBUG)
	DaemonThread.join();
#else
	TS_LOG("Wait until we know the daemon has started.");

	// Create another fork to act as a timeout.
	pid_t TimeoutPid = fork();
	if (TimeoutPid == 0)
	{
		timespec TimeoutTime = { 5, 0 };
		nanosleep(&TimeoutTime, nullptr);
		exit(0);
	}

	struct sigaction SigAction = {};
	SigAction.sa_flags = int(SA_RESETHAND);
	SigAction.sa_handler = [] (int Signal) {
		TS_LOG("Parent received signal %d", Signal);
	};
	sigaction(SIGUSR1, &SigAction, nullptr);

	// Now we have two children (daemon and timeout) and a signal to wait for
	int WaitResult = -1;
	pid_t WaitedPid = waitpid(0, &WaitResult, 0);
	if (WaitedPid == TimeoutPid)
	{
		TS_LOG("Timed out");
		Ret = Result_BegunTimeout;
	}
	else if (WaitedPid == DaemonPid)
	{
		TS_LOG("Daemon exited unexpectedly (wait_result=%d)", WaitResult);
		Ret = WEXITSTATUS(WaitResult);
	}
	else
	{
		TS_LOG("Daemon signalled successful start");
	}
	kill(TimeoutPid, SIGKILL);
#endif // debug

	TS_LOG("Forked complete (ret=%d)", Ret);
	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
static int MainDaemon(int ArgC, char** ArgV, pid_t ParentPid)
{
	// We expect that there is no lock file on disk if we've got this far.
	FPath DotPidPath = GetLockFilePath();
	TS_LOG("Opening the lock file '%s'", DotPidPath.c_str());
	int DotPidFd = open(DotPidPath.c_str(), O_CREAT|O_WRONLY, 0666);
	if (DotPidFd < 0)
	{
		TS_LOG("Failed to open lock file (errno=%d)", errno);
		return Result_OpenFailPid;
	}
	fchmod(DotPidFd, 0666);
	OnScopeExit([DotPidFd] () { close(DotPidFd); });

	// Lock the lock file
	struct flock FileLock = { .l_type = F_WRLCK };
	int Result = fcntl(DotPidFd, F_SETLK, &FileLock);
	if (Result < 0)
	{
		TS_LOG("Unable to claim lock file (errno=%d)", errno);
		return Result_LockClaimFail;
	}

	// Fire up the store
	TS_LOG("Starting the store");
	FStoreService* StoreService;
	{
		FStoreOptions Options = ParseOptions(ArgC, ArgV);

		if (Options.Dir.empty())
		{
			FPath StoreDir;
			GetUnrealTraceHome(StoreDir);
			StoreDir /= "Store";
			Options.Dir = StoreDir;
		}

		StoreService = StartStore(Options);
	}
	OnScopeExit([StoreService] () { delete StoreService; });

	// Fill out the lock file with details about this instance
	FInstanceInfo InstanceInfo;
	InstanceInfo.Set();
	if (write(DotPidFd, &InstanceInfo, sizeof(InstanceInfo)) != sizeof(InstanceInfo))
	{
		TS_LOG("Unable to write instance info to lock file (errno=%d)", errno);
		return Result_UnexpectedError;
	}
	fsync(DotPidFd);

	// Detach from our parent and from any controlling terminal
	for (Result = setsid(); Result < 0;)
	{
		if (errno == EPERM)
		{
			break;
		}

		TS_LOG("Failed creating a new session (errno=%d)", errno);
		return Result_UnexpectedError;
	}

	// Signal to any parent that we're done here.
	if (ParentPid != 0)
	{
		TS_LOG("Signalling parent %d", ParentPid);
		kill(ParentPid, SIGUSR1);
	}

	// Wait to be told to resign.
	struct sigaction SigAction = {};
	SigAction.sa_handler = [] (int) {};
	SigAction.sa_flags = int(SA_RESETHAND),
	sigaction(SIGTERM, &SigAction, nullptr);
	sigaction(SIGINT, &SigAction, nullptr);

	TS_LOG("Entering signal wait loop...");
	sigset_t SignalSet;
	sigemptyset(&SignalSet);
	sigaddset(&SignalSet, SIGTERM);
	sigaddset(&SignalSet, SIGKILL);
	sigaddset(&SignalSet, SIGINT);
	while (true)
	{
		int Signal = -1;
		int Ret = sigwait(&SignalSet, &Signal);
		if (Ret == 0)
		{
			TS_LOG("Received signal %d", Signal);
			break;
		}
	}

	// Clean up. We are done here.
	std::error_code ErrorCode;
	std::filesystem::remove(DotPidPath, ErrorCode);

	FileLock.l_type = F_UNLCK;
	fcntl(DotPidFd, F_SETLK, &FileLock);

	return Result_Ok;
}

////////////////////////////////////////////////////////////////////////////////
static int MainDaemon(int ArgC, char** ArgV)
{
	return MainDaemon(ArgC, ArgV, 0);
}

#endif // TS_PLATFORM_LINUX/MAC



// {{{1 main -------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
int MainTest(int ArgC, char** ArgV)
{
	extern void TestCbor();
	TestCbor();
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int main(int ArgC, char** ArgV)
{
	if (ArgC < 2)
	{
		printf("UnrealTraceServer v%d.%d / Unreal Engine / Epic Games\n\n", TS_VERSION_PROTOCOL, TS_VERSION_MINOR);
		puts(  "Usage; <cmd>");
		puts(  "Commands;");
		puts(  "  fork   Starts a background server, upgrading any existing instance");
		puts(  "  daemon The mode that a background server runs in");
		puts(  "  kill   Shuts down a currently running instance");
		puts("");
		puts(  "UnrealTraceServer acts as a hub between runtimes that are tracing performance");
		puts(  "instrumentation and tools like Unreal Insights that consume and present that");
		puts(  "data for analysis. TCP ports 1981 and 1989 are used, where the former receives");
		puts(  "trace data, and the latter is used by tools to query the server's store.");

		FPath HomeDir;
		GetUnrealTraceHome(HomeDir);
		HomeDir.make_preferred();
		std::string HomeDirU8 = HomeDir.string();
		printf("\nStore path; %s\n", HomeDirU8.c_str());

		return 127;
	}

	struct {
		const char*	Verb;
		int			(*Entry)(int, char**);
	} Dispatches[] = {
		"fork",		MainFork,
		"daemon",	MainDaemon,
		"test",		MainTest,
		"kill",		MainKill,
	};

	for (const auto& Dispatch : Dispatches)
	{
		if (strcmp(ArgV[1], Dispatch.Verb) == 0)
		{
			FLoggingScope LoggingScope;
			return (Dispatch.Entry)(ArgC - 1, ArgV + 1);
		}
	}

	printf("Unknown command '%s'\n", ArgV[1]);
	return 126;
}

/* vim: set noexpandtab foldlevel=1 : */

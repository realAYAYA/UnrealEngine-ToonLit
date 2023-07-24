// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncLog.h"
#include "UnsyncCore.h"
#include "UnsyncError.h"
#include "UnsyncFile.h"
#include "UnsyncUtil.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <atomic>
#include <mutex>
#include <vector>

#if UNSYNC_PLATFORM_WINDOWS
#	include <Dbghelp.h>
#	include <Windows.h>
#	pragma comment(lib, "Dbghelp.lib")
#endif	// UNSYNC_PLATFORM_WINDOWS

namespace unsync {

#if UNSYNC_PLATFORM_WINDOWS
bool
IsDebuggerPresent()
{
	return ::IsDebuggerPresent();
}
#else
bool
IsDebuggerPresent()
{
	return false;  // TODO
}
#endif

thread_local bool	GLogVerbose		= false;
bool				GLogVeryVerbose = false;
bool				GBreakOnError	= IsDebuggerPresent();
bool				GBreakOnWarning = false;
thread_local uint32 GLogIndent		= 0;
bool				GLogProgress	= false;

std::mutex GLogMutex;

std::atomic<uint32> GLogThreadIndexCounter;
thread_local uint32 GLogThreadIndex = ~0u;

FTimePoint GNextFlushTime = TimePointNow();
static void
LogConditionalFlush(FILE* Stream)
{
	FTimePoint NextFlushTime = GNextFlushTime;
	FTimePoint CurrentTime	 = TimePointNow();
	if (CurrentTime > NextFlushTime || GLogProgress)
	{
		GNextFlushTime = CurrentTime + std::chrono::milliseconds(1000);
		fflush(Stream);
	}
}

static uint32
GetLogThreadIndex()
{
	if (GLogThreadIndex == ~0u)
	{
		GLogThreadIndex = GLogThreadIndexCounter++;
	}
	return GLogThreadIndex;
}

struct FLogFile
{
	FLogFile(const wchar_t* InFilename) : Filename(InFilename)
	{
#if UNSYNC_PLATFORM_WINDOWS
		Handle = _wfopen(InFilename, L"wt, ccs=UNICODE");
#else
		// TODO
#endif
	}

	~FLogFile()
	{
		if (Handle)
		{
			fclose(Handle);
		}
	}

	std::wstring Filename;
	FILE*		 Handle = nullptr;
};

std::unique_ptr<FLogFile> GLogFile;

void
LogSetFileInternal(const wchar_t* Filename)
{
	GLogFile = {};

	if (Filename)
	{
		GLogFile = std::make_unique<FLogFile>(Filename);
	}
}

std::vector<std::unique_ptr<FLogFile>> GLogFileStack;

FLogFileScope::FLogFileScope(const wchar_t* Filename)
{
	{
		std::lock_guard<std::mutex> LockGuard(GLogMutex);
		GLogFileStack.push_back(std::move(GLogFile));
		LogSetFileInternal(Filename);
	}

	UNSYNC_VERBOSE2(L"UNSYNC %hs started logging to file '%ls'", GetVersionString().c_str(), Filename);
}

FLogFileScope::~FLogFileScope()
{
	UNSYNC_VERBOSE2(L"Finished logging to file");

	std::lock_guard<std::mutex> LockGuard(GLogMutex);
	std::swap(GLogFile, GLogFileStack.back());
	GLogFileStack.pop_back();
}

void
LogProgress(const wchar_t* ItemName, uint64 Current, uint64 Total)
{
	if (!GLogProgress)
	{
		return;
	}

	std::lock_guard<std::mutex> LockGuard(GLogMutex);
	if (ItemName == nullptr)
	{
		ItemName = L"*";
	}
	wprintf(L"@progress [%ls] %llu / %llu\n", ItemName, Current, Total);

	LogConditionalFlush(stdout);
}

void
LogStatus(const wchar_t* InItemName, const wchar_t* Status)
{
	if (!GLogProgress)
	{
		return;
	}

	std::lock_guard<std::mutex> LockGuard(GLogMutex);

	const wchar_t* ItemName = InItemName ? InItemName : L"*";
	wprintf(L"@status [%ls] %ls\n", ItemName, Status);

	if (InItemName)
	{
		LogConditionalFlush(stdout);
	}
	else
	{
		fflush(stdout);
	}
}

void
LogFlush()
{
	std::lock_guard<std::mutex> LockGuard(GLogMutex);
	fflush(stdout);
	if (GLogFile && GLogFile->Handle)
	{
		FILE* F = GLogFile->Handle;
		fflush(F);
	}
}

void
LogPrintf(ELogLevel Level, const wchar_t* Str, ...)
{
	std::lock_guard<std::mutex> LockGuard(GLogMutex);

	bool		   bShouldIndent = false;
	const wchar_t* Prefix		 = nullptr;

	if (Level == ELogLevel::Error)
	{
		Prefix = L"ERROR: ";
	}
	else if (Level == ELogLevel::Warning)
	{
		Prefix = L"WARNING: ";
	}
	else if (GLogIndent)
	{
		// Indent the log only for info/debug/trace levels
		bShouldIndent = true;
	}

	ELogLevel MaxDisplayLevel = ELogLevel::Info;

	if (GLogVerbose)
	{
		if (GLogVeryVerbose)
		{
			MaxDisplayLevel = ELogLevel::Trace;
		}
		else
		{
			MaxDisplayLevel = ELogLevel::Debug;
		}
	}

	if (Level <= MaxDisplayLevel)
	{
		if (Prefix)
		{
			wprintf(Prefix);
		}

		if (bShouldIndent)
		{
			wprintf(L"%*c", GLogIndent, L' ');
		}

		va_list Va;
		va_start(Va, Str);
		vwprintf(Str, Va);
		va_end(Va);

		LogConditionalFlush(stdout);
	}

	if (GLogFile && GLogFile->Handle)
	{
		FILE* F = GLogFile->Handle;

		uint32 ThreadIndex = GetLogThreadIndex();
		fwprintf(F, L"[%3d] ", ThreadIndex);

		switch (Level)
		{
			case ELogLevel::Error:
				fwprintf(F, L"[ERROR] ");
				break;
			case ELogLevel::Warning:
				fwprintf(F, L"[WARN] ");
				break;
			case ELogLevel::Info:
				fwprintf(F, L"[INFO] ");
				break;
			case ELogLevel::Debug:
				fwprintf(F, L"[DEBUG] ");
				break;
			case ELogLevel::Trace:
				fwprintf(F, L"[TRACE] ");
				break;
			default:
				break;
		}

		va_list Va;
		va_start(Va, Str);
		vfwprintf(F, Str, Va);
		va_end(Va);

		if (Level == ELogLevel::Error || Level == ELogLevel::Warning)
		{
			fflush(F);
		}
	}
}

extern const char* HttpStatusToString(int32 Code);
void
LogError(const FError& E)
{
	const char* ErrorKindStr = nullptr;
	const char* ErrorDescStr = nullptr;
	switch (E.Kind)
	{
		default:
		case EErrorKind::Unknown:
			ErrorKindStr = "Unknown";
			break;
		case EErrorKind::Http:
			ErrorDescStr = HttpStatusToString(E.Code);
			ErrorKindStr = "HTTP";
			break;
		case EErrorKind::System:
			ErrorKindStr = "System";
			break;
		case EErrorKind::App:
			ErrorKindStr = "Application";
			break;
	}

	const wchar_t* ContextStr = E.Context.empty() ? nullptr : E.Context.c_str();

	if (ErrorDescStr)
	{
		LogPrintf(ELogLevel::Error,
				  L"%hs code: %d (%hs).%ls%ls\n",
				  ErrorKindStr,
				  E.Code,
				  ErrorDescStr,
				  ContextStr ? L" Context: " : L"",
				  E.Context.empty() ? L"" : E.Context.c_str());
	}
	else
	{
		LogPrintf(ELogLevel::Error,
				  L"%hs code: %d.%ls%ls\n",
				  ErrorKindStr,
				  E.Code,
				  ContextStr ? L" Context: " : L"",
				  E.Context.empty() ? L"" : E.Context.c_str());
	}
}

static FPath GCrashDumpPath;
void
SetCrashDumpPath(const FPath& Path)
{
	std::lock_guard<std::mutex> LockGuard(GLogMutex);
	GCrashDumpPath = Path;
}

bool
get_crash_dump_path(FPath& OutPath)
{
	std::lock_guard<std::mutex> LockGuard(GLogMutex);
	if (GCrashDumpPath.empty())
	{
		return false;
	}
	else
	{
		OutPath = GCrashDumpPath;
		return true;
	}
}

bool
LogWriteCrashDump(void* InExceptionPointers)
{
#if UNSYNC_PLATFORM_WINDOWS

	_EXCEPTION_POINTERS* ExceptionPointers = (_EXCEPTION_POINTERS*)InExceptionPointers;

	FPath CrashDumpPath;
	if (!get_crash_dump_path(CrashDumpPath))
	{
		return false;
	}

	wchar_t DumpFilename[4096] = {};
	uint64	Timestamp		   = TimePointNow().time_since_epoch().count();
	swprintf_s(DumpFilename, L"unsync-%016llX.dmp", Timestamp);
	FPath CrashDumpFilename = CrashDumpPath / FPath(DumpFilename);

	LogPrintf(ELogLevel::Info, L"!!! Writing crash dump to '%ls'\n", CrashDumpFilename.wstring().c_str());

	MINIDUMP_EXCEPTION_INFORMATION ExceptionInfo;
	ExceptionInfo.ThreadId			= GetCurrentThreadId();
	ExceptionInfo.ExceptionPointers = ExceptionPointers;
	ExceptionInfo.ClientPointers	= true;

	HANDLE DumpFile =
		CreateFileW(CrashDumpFilename.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	if (DumpFile != INVALID_HANDLE_VALUE)
	{
		BOOL Ok =
			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), DumpFile, MiniDumpWithDataSegs, &ExceptionInfo, nullptr, nullptr);
		if (Ok)
		{
			LogPrintf(ELogLevel::Info, L"!!! Crash dump file written.\n");
			return true;
		}
		else
		{
			LogPrintf(ELogLevel::Error, L"!!! Failed to generate crash dump. Error code: %d.\n", GetLastError());
		}
	}
	else
	{
		LogPrintf(ELogLevel::Error, L"!!! Failed to open output file. Error code: %d.\n", GetLastError());
	}

	LogPrintf(ELogLevel::Error, L"!!! Failed to write dump file.\n");
#endif	// UNSYNC_PLATFORM_WINDOWS

	return false;
}

FLogFlushScope::~FLogFlushScope()
{
	LogFlush();
}

}  // namespace unsync

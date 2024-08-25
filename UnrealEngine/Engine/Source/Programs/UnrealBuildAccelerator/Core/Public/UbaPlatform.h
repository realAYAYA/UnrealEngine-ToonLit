// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"

#if PLATFORM_WINDOWS
	#define NOMINMAX
	#include <ntstatus.h>
	#define WIN32_NO_STATUS
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#include <ws2tcpip.h>
	#include <windows.h>

	#if defined( __clang_analyzer__ )
	#define ANALYSIS_NORETURN __attribute__((analyzer_noreturn))
	#else
	#define ANALYSIS_NORETURN __analysis_noreturn
	#endif
#else
	#include <fcntl.h>
	#include <wchar.h>
	#include <stdio.h>
	#include <stdarg.h>
	#include <math.h>
	#include <string.h>
	#include <sys/mman.h>
	#include <sys/syscall.h>
	#include <sys/socket.h>
	#include <sys/stat.h>
	#include <sys/wait.h>
	#include <unistd.h>
	#include <spawn.h>
	#include <signal.h>

	#if PLATFORM_MAC
	#include <sys/random.h>
    #include <sys/posix_shm.h>
	#include  <mach-o/dyld.h>
	#else
	#include <linux/random.h>
	#include <time.h>
	#include <termios.h>
	#include <stropts.h>
	#include <sys/select.h>
	#include <sys/ioctl.h>
	#endif

	#define ANALYSIS_NORETURN // __attribute__((analyzer_noreturn))
    #define ERROR_SUCCESS 0
#endif

#define UBA_EXPERIMENTAL 0

namespace uba
{
	#ifndef sizeof_array
	#define	sizeof_array(array) int(sizeof(array)/sizeof(array[0]))
	#endif

	#if UBA_DEBUG
	#define UBA_ASSERT(x) do { if (x) break; uba::UbaAssert(TC(""), __FILE__, __LINE__, #x, 543221); } while(false)
	#define UBA_ASSERTF(x, ...) do { if (x) break; uba::StringBuffer<> _buf; _buf.Appendf(__VA_ARGS__); uba::UbaAssert(_buf.data, __FILE__, __LINE__, #x, 543221); } while(false)
	#define UBA_ASSERT_MESSAGEBOX 0
	#else
	#define UBA_ASSERT(x) do { } while(false)
	#define UBA_ASSERTF(x, ...) do { } while(false)
	#define UBA_ASSERT_MESSAGEBOX 0
	#endif

	#define UBA_NOT_IMPLEMENTED(x) UBA_ASSERTF(false, TC("%s not implemented!"), #x);

	void WriteAssertInfo(class StringBufferBase& out, const tchar* text, const char* file, u32 line, const char* expr, u32 skipCallstack);
	ANALYSIS_NORETURN void UbaAssert(const tchar* text, const char* file, u32 line, const char* expr, u32 terminateCode);
	using CustomAssertHandler = void(const tchar* text);
	void SetCustomAssertHandler(CustomAssertHandler* handler);
	bool CreateGuid(Guid& out);
	bool IsRunningWine();
	bool IsEscapePressed();
	void Sleep(u32 milliseconds);
	u32 GetUserDefaultUILanguage();
	u32 GetLastError();
	void SetLastError(u32 error);
	bool GetComputerNameW(tchar* buffer, u32 bufferLen);
	u32 GetCurrentProcessId();

	enum FileHandle : u64 {};
	inline constexpr FileHandle InvalidFileHandle = (FileHandle)-1;

	enum MutexHandle : u64 {};
	inline constexpr MutexHandle InvalidMutexHandle = MutexHandle(0);
	MutexHandle CreateMutexW(bool bInitialOwner, const tchar* name);

	enum ProcHandle : u64 {};
	inline constexpr ProcHandle InvalidProcHandle = ((ProcHandle)(u64)-1);
	ProcHandle GetCurrentProcessHandle();

	u32 GetEnvironmentVariableW(const tchar* name, tchar* buffer, u32 nSize);
	u32 ExpandEnvironmentStringsW(const tchar* lpSrc, tchar* lpDst, u32 nSize);
	u32 GetLogicalProcessorCount();
	u32 GetProcessorGroupCount();

	void ElevateCurrentThreadPriority();
	void PrefetchVirtualMemory(const void* mem, u64 size);

#if PLATFORM_WINDOWS
	inline constexpr bool CaseInsensitiveFs = true;
	inline constexpr tchar PathSeparator = '\\';
	inline constexpr tchar NonPathSeparator = '/';
	inline constexpr u32 MaxPath = 512;
	inline DWORD ToLow(u64 v) { LARGE_INTEGER li; li.QuadPart = (LONGLONG)v; return li.LowPart; }
	inline LONG ToHigh(u64 v) { LARGE_INTEGER li; li.QuadPart = (LONGLONG)v; return li.HighPart; }
	inline LARGE_INTEGER ToLargeInteger(u64 v) { LARGE_INTEGER li; li.QuadPart = (LONGLONG)v; return li; }
	inline LARGE_INTEGER ToLargeInteger(u32 high, u32 low) { LARGE_INTEGER li; li.HighPart = (LONG)high; li.LowPart = low; return li; }
	#define TStrlen(s) u32(wcslen(s))
	#define TStrchr(a, b) wcschr(a, b)
	#define TStrrchr(a, b) wcsrchr(a, b)
	#define TStrstr(a, b) wcsstr(a, b)
	#define TStrcmp(a, b) wcscmp(a, b)
	#define TSprintf_s swprintf_s
	#define Tvsprintf_s(...) vswprintf_s(__VA_ARGS__)
	#define TStrcpy_s wcscpy_s
	#define TStrcat_s wcscat_s
	#define TStrdup _wcsdup
#else
	inline constexpr tchar PathSeparator = '/';
	inline constexpr tchar NonPathSeparator = '\\';
	inline constexpr u32 MaxPath = 512;
	#define TStrlen(s) u32(strlen(s))
	#define TStrchr(a, b) strchr(a, b)
	#define TStrrchr(a, b) strrchr(a, b)
	#define TStrstr(a, b) strstr(a, b)
	#define TStrcmp(a, b) strcmp(a, b)
	#define TSprintf_s(...) snprintf(__VA_ARGS__)
	#define Tvsprintf_s(a, b, c, d) vsnprintf(a, b, c, d)
	#define TStrcpy_s(a, b, c) strcpy(a, c)
	#define TStrcat_s(a, b, c) strcat(a, c)
	#define TStrdup strdup
	#define localtime_s(a,b) localtime_r(b,a)
	void GetMappingHandleName(StringBufferBase& out, u64 uid);
	inline u64 FromTimeSpec(const timespec& ts) { return u64(ts.tv_sec) * 10'000'000ull + u64(ts.tv_nsec/100); }
	inline timespec ToTimeSpec(u64 time) { timespec ts; ts.tv_sec = time / 10'000'000ull; ts.tv_nsec = (time - (u64(ts.tv_sec) * 10'000'000ull)) * 100; return ts; }
	#if PLATFORM_LINUX
	inline constexpr bool CaseInsensitiveFs = false;
	#define st_mtimespec st_mtim
	#else
	inline constexpr bool CaseInsensitiveFs = true;
#endif
#endif // PLATFORM_WINDOWS
}

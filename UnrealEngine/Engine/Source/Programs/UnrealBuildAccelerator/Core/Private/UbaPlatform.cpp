// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaPlatform.h"
#include "UbaProcessStats.h"

#if !PLATFORM_WINDOWS
#include <execinfo.h>
#include <limits.h>
extern const char* __progname;
#else
#include <conio.h>
#include <psapi.h>
#endif

namespace uba
{
	SystemStats g_systemStats;
	thread_local SystemStats* t_systemStats;

	SystemStats& SystemStats::GetCurrent()
	{
		SystemStats* stats = t_systemStats;
		return stats ? *stats : g_systemStats;
	}

	SystemStats& SystemStats::GetGlobal()
	{
		return g_systemStats;
	}

	SystemStatsScope::SystemStatsScope(SystemStats& s) : stats(s)
	{
		t_systemStats = &stats;
	}
	SystemStatsScope::~SystemStatsScope()
	{
		t_systemStats = nullptr;
	}

	bool CreateGuid(Guid& out)
	{
		#if PLATFORM_WINDOWS
		return ::CoCreateGuid((GUID*)&out) == S_OK;
		#elif PLATFORM_MAC
		arc4random_buf(&out, 16);
		return true;
		#else
		const int f = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
		if (f == -1)
			return false;
		size_t bytesRead = read(f, &out, 16);
		close(f);
		return bytesRead == 16;
		//syscall(SYS_getrandom, &out, sizeof(Guid), 0x0001);
		#endif
	}

	bool IsRunningWine()
	{
		#if PLATFORM_WINDOWS
		static bool isRunningInWine = []()
			{
				if (HMODULE kernelBaseModule = GetModuleHandleW(L"kernel32.dll"))
					return GetProcAddress(kernelBaseModule, "wine_get_unix_file_name") != nullptr;
				return false;
			}();
		return isRunningInWine;
		#else
		return false;
		#endif
	}

	void Sleep(u32 milliseconds)
	{
		#if PLATFORM_WINDOWS
		::Sleep(milliseconds);
		#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
		struct timespec ts;
		ts.tv_sec = milliseconds / 1000;
		ts.tv_nsec = (milliseconds % 1000) * 1000000;
		nanosleep(&ts, NULL);
		#else
		if (milliseconds >= 1000)
		{
			sleep(milliseconds / 1000);
		}
		usleep((milliseconds % 1000) * 1000);
		#endif
	}

	u32 GetUserDefaultUILanguage()
	{
		#if PLATFORM_WINDOWS
		return ::GetUserDefaultUILanguage();
		#else
		return 1;
		#endif
	}

	thread_local u32 t_lastError;

	u32 GetLastError()
	{
		#if PLATFORM_WINDOWS
		return ::GetLastError();
		#else
		return t_lastError;
		#endif
	}

	void SetLastError(u32 error)
	{
		#if PLATFORM_WINDOWS
		::SetLastError(error);
		#else
		t_lastError = error;
		#endif
	}

	bool GetComputerNameW(tchar* buffer, u32 bufferLen)
	{
		#if PLATFORM_WINDOWS
		DWORD nSize = bufferLen;
		return ::GetComputerNameW(buffer, &nSize);
		#else
		gethostname(buffer, bufferLen);
		return true;
		#endif
	}

	void WriteAssertInfo(StringBufferBase& out, const tchar* text, const char* file, u32 line, const char* expr, u32 skipCallstack = 0)
	{
#if PLATFORM_WINDOWS
		if (text)
			out.Append(L"ASSERT: ").Append(text).Append(L"\r\n");
		if (file)
			out.Appendf(L"%hs:%u (%hs)", file, line, expr);

		typedef USHORT(WINAPI* CaptureStackBackTraceType)(__in ULONG, __in ULONG, __out PVOID*, __out_opt PULONG);
		static CaptureStackBackTraceType func = (CaptureStackBackTraceType)(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlCaptureStackBackTrace"));
		if (func != NULL)
		{
			struct ModuleRec { u64 start; HMODULE handle; };
			Map<u64, ModuleRec> moduleEndAddresses;

			HMODULE loadedModules[512];
			DWORD needed = 0;
			if (EnumProcessModules(GetCurrentProcess(), loadedModules, sizeof(loadedModules), &needed))
			{
				for (u32 i = 0, e = needed / sizeof(HMODULE); i != e; ++i)
				{
					MODULEINFO mi;
					GetModuleInformation(GetCurrentProcess(), loadedModules[i], &mi, sizeof(mi));
					ModuleRec& rec = moduleEndAddresses[u64(mi.lpBaseOfDll) + mi.SizeOfImage];
					rec.handle = loadedModules[i];
					rec.start = u64(mi.lpBaseOfDll);
				}
			}

			const int kMaxCallers = 16;// 62;
			void* callers[kMaxCallers];
			if (u32 count = (func)(0, kMaxCallers, callers, NULL))
			{
				if (file)
					out.Appendf(L"\n\n");
				out.Appendf(L"  Callstack:");
				for (u32 i = 0; i < count; i++)
				{
					tchar str[1024];
					auto addr = u64(callers[i]);

					auto findIt = moduleEndAddresses.lower_bound(addr);
					if (findIt != moduleEndAddresses.end() && addr >= findIt->second.start)
					{
						if (GetModuleFileNameW(findIt->second.handle, str, sizeof_array(str)))
						{
							const tchar* moduleName = str;
							if (const tchar* lastSeparator = TStrrchr(str, PathSeparator))
								moduleName = lastSeparator + 1;
							out.Appendf(L"\n   %s: +0x%llx", moduleName, addr - findIt->second.start);
						}
					}
					else
						out.Appendf(L"\n   <Unknown>: 0x%llx", callers[i]);
				}
			}
		}
#else
		out.Append(TC("ASSERT: ")).Append(*text ? text : TC("Unknown")).Append('\n');
		if (expr && strcmp(expr, "false") != 0)
			out.Appendf(TC(" EXPR: %s\n"), expr);
		if (file && *file)
			out.Appendf(TC("LOCATION: %s:%u"), file, line);

		constexpr u32 maxCallers = 100;
		void* buffer[maxCallers];
		int nptrs = backtrace(buffer, maxCallers);

		if (true)
		{
			char progName[256];
			int res = readlink("/proc/self/exe", progName, sizeof_array(progName));
			if (res != -1)
			{
				progName[res] = 0;
				fflush(stdout);
				char str[4096];
				strcpy(str, "addr2line");
				for (int i = 0; i < nptrs; ++i)
				{
					char v[32];
					snprintf(v, sizeof_array(v), " %llx", u64(buffer[i]));
					strcat(str, v);
				}
				strcat(str, " -e ");

				strcat(str, progName);
				fflush(stdout);
				if (FILE* fp = popen(str, "r"))
				{
					out.Append('\n');
					char path[1024] = { 0 };
					u32 index = 0;
					u32 countBeforeCallstack = out.count;
					while (fgets(path, sizeof(path), fp) != NULL)
					{
						if (path[0] == '?')
							break;
						if (index++ <= skipCallstack)
							continue;
						char* fileName = path;
						char* lastSlash = strrchr(path, '/');
						if (lastSlash)
							fileName = lastSlash + 1;
						if (strncmp(fileName, "function.h", 10) == 0 || strncmp(fileName, "invoke.h:", 9) == 0)
							continue;
						out.Appendf(TC("  %s"), fileName);
					}
					fclose(fp);
					if (countBeforeCallstack != out.count)
						return;
				}
			}
		}

		char** strings = backtrace_symbols(buffer, nptrs);
		if (strings == NULL)
		{
			out.Append(TC("Failed to get symbols"));
			return;
		}
		for (int i = 0; i < nptrs; ++i)
			out.Appendf(TC("%s\n"), strings[i]);
		free(strings);
#endif
	}

	bool IsEscapePressed()
	{
		#if PLATFORM_WINDOWS
		return _kbhit() && _getch() == 27;
		#else
		return false;
		#endif
	}

	u32 GetCurrentProcessId()
	{
		#if PLATFORM_WINDOWS
		return ::GetCurrentProcessId();
		#else
		return getpid();
		#endif
	}

	MutexHandle CreateMutexW(bool bInitialOwner, const tchar* lpName)
	{
		#if PLATFORM_WINDOWS
		return (MutexHandle)(u64)::CreateMutexW(NULL, bInitialOwner, lpName);
		#else
		// TODO: This is used to check for exclusivity and also for trace streams (only created by host and read by visualizer)
		SetLastError(ERROR_SUCCESS);
		return ((MutexHandle)(u64)1337); // Just some random value
		#endif
	}

	u32 GetEnvironmentVariableW(const tchar* name, tchar* buffer, u32 nSize)
	{
		#if PLATFORM_WINDOWS
		return ::GetEnvironmentVariableW(name, buffer, nSize);
		#else
		const char* env = getenv(name);
		if (!env)
		{
			SetLastError(203); // ERROR_ENVVAR_NOT_FOUND
			return 0;
		}

		auto envLen = strlen(env);
		if (nSize <= envLen)
			return envLen + 1;
		memcpy(buffer, env, envLen + 1);
		return envLen;
		#endif
	}

	u32 ExpandEnvironmentStringsW(const tchar* lpSrc, tchar* lpDst, u32 nSize)
	{
		#if PLATFORM_WINDOWS
		return ::ExpandEnvironmentStringsW(lpSrc, lpDst, nSize);
		#else
		UBA_ASSERTF(false, TC("ExpandEnvironmentStringsW not implemented (%s)"), lpSrc);
		return 0;
		#endif
	}

	ProcHandle GetCurrentProcessHandle()
	{
		#if PLATFORM_WINDOWS
		return (ProcHandle)(u64)GetCurrentProcess();
		#else
		UBA_ASSERTF(false, TC("GetCurrentProcessHandle not implemented"));
		return (ProcHandle)0;
		#endif
	}

	u32 GetLogicalProcessorCount()
	{
		#if PLATFORM_WINDOWS
		return ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
		#else
		return (u32)sysconf(_SC_NPROCESSORS_ONLN);
		#endif
	}

	u32 GetProcessorGroupCount()
	{
		#if PLATFORM_WINDOWS
		static u32 s_processorGroupCount = u32(GetActiveProcessorGroupCount());
		if (s_processorGroupCount)
			return s_processorGroupCount;
		#endif
		return 1u;
	}

	void ElevateCurrentThreadPriority()
	{
		#if PLATFORM_WINDOWS
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		#endif
	}

	void PrefetchVirtualMemory(const void* mem, u64 size)
	{
		#if PLATFORM_WINDOWS
		WIN32_MEMORY_RANGE_ENTRY entry;
		entry.VirtualAddress = const_cast<void*>(mem);
		entry.NumberOfBytes = size;
		PrefetchVirtualMemory(GetCurrentProcess(), 1, &entry, 0);
		#endif
	}


#if !PLATFORM_WINDOWS
	void GetMappingHandleName(StringBufferBase& out, u64 uid)
	{
		#if PLATFORM_MAC
		out.Append("/tmp/uba_").AppendHex(uid);
		#else
		out.Append("/uba_").AppendHex(uid);
		#endif
	}
#endif
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaApplicationRules.h"
#include "UbaStringBuffer.h"
#include "UbaPlatform.h"
#include "UbaProcessStats.h"
#include "UbaProtocol.h"

namespace uba
{
	class DirectoryTable;
	class MappedFileTable;

	#if PLATFORM_WINDOWS
	DWORD Local_GetLongPathNameW(LPCWSTR lpszShortPath, LPWSTR lpszLongPath, DWORD cchBuffer);
	#endif

	ANALYSIS_NORETURN void FatalError(u32 code, const tchar* format, ...);

	void Rpc_WriteLog(const tchar* text, u64 textCharLength, bool printInSession, bool isError);
	void Rpc_WriteLogf(const tchar* format, ...);

	const tchar* GetApplicationShortName();
	bool FixPath(StringBufferBase& out, const tchar* path);

	#if UBA_DEBUG_LOG_ENABLED
		#define DEBUG_LOG_PREFIX(Prefix, Command, ...) \
			LogScope STRING_JOIN(ls, __LINE__); \
			if (isLogging()) \
			{ \
				GetLogTlsBuffer().Clear().Append(Command).Append(' ').Appendf(__VA_ARGS__).Append(TC("\n")); \
				WriteDebugLogWithPrefix(#Prefix, STRING_JOIN(ls, __LINE__)); \
			}

		//#define DEBUG_LOG_DETOURED(Command, ...) 
		#define DEBUG_LOG_DETOURED(Command, ...) DEBUG_LOG_PREFIX(D, Command, __VA_ARGS__)
		//#define DEBUG_LOG_TRUE(...)
		#define DEBUG_LOG_TRUE(Command, ...) DEBUG_LOG_PREFIX(T, Command, __VA_ARGS__)
		//#define DEBUG_LOG_PIPE(Command, ...) ts.leave(); DEBUG_LOG_PREFIX(P, Command, __VA_ARGS__)
		#define DEBUG_LOG_PIPE(Command, ...) ts.Leave();
		//#define DEBUG_LOG(...)
		#define DEBUG_LOG(...) { if (isLogging()) { GetLogTlsBuffer().Clear().Appendf(__VA_ARGS__).Append(TC("\n")); WriteDebugLog(); }}
	#else
		#define DEBUG_LOG(...)
		#define DEBUG_LOG_DETOURED(Command, ...)
		#define DEBUG_LOG_TRUE(Command, ...)
		#define DEBUG_LOG_PIPE(...) ts.Leave();
	#endif

	extern StringBuffer<512>& g_virtualApplication;
	extern StringBuffer<512>& g_virtualApplicationDir;
	extern StringBuffer<256>& g_logName;

	extern StringBuffer<512>& g_virtualWorkingDir;
	extern StringBuffer<128>& g_systemTemp;
	extern StringBuffer<128>& g_systemRoot;

	extern ProcessStats& g_stats;
	extern bool g_echoOn;
	extern ReaderWriterLock& g_communicationLock;
	extern MemoryBlock& g_memoryBlock;
	extern DirectoryTable& g_directoryTable;
	extern MappedFileTable& g_mappedFileTable;

	struct SuppressDetourScope
	{
		SuppressDetourScope();
		~SuppressDetourScope();
	};
	extern thread_local u32 t_disallowDetour;

	#if UBA_DEBUG_LOG_ENABLED
	inline constexpr u32 LogBufSize = 16 * 1024;
	extern FileHandle g_debugFile;
	inline bool isLogging() { return g_debugFile != InvalidFileHandle; }
	struct LogScope { LogScope(); ~LogScope(); void Flush(); };
	StringBufferBase& GetLogTlsBuffer();
	void WriteDebugLogWithPrefix(const char* prefix, LogScope& scope);
	void WriteDebugLog();
	void FlushDebugLog();
	#endif
	#if UBA_DEBUG_VALIDATE
	extern bool g_validateFileAccess;
	#endif

	inline constexpr bool g_allowDirectoryCache = true;
	inline constexpr bool g_allowFileMappingDetour = true;
	inline constexpr bool g_allowFindFileDetour = true;
	inline constexpr bool g_allowListDirectoryHandle = true;
	inline constexpr bool g_allowKeepFilesInMemory = IsWindows;

	extern u32 g_rulesIndex;
	extern ApplicationRules* g_rules;
	extern bool g_runningRemote;
	extern bool g_isChild;

	#if PLATFORM_WINDOWS
	constexpr u32 ErrorSuccess = 0;
	constexpr u32 ErrorFileNotFound = ERROR_FILE_NOT_FOUND;
	using FileAttributesData = WIN32_FILE_ATTRIBUTE_DATA;
	#else
	using FileAttributesData = struct stat;
	constexpr u32 ErrorSuccess = 0;
	constexpr u32 ErrorFileNotFound = ENOENT;
	#endif


	struct FileAttributes
	{
		FileAttributesData data;
		u64 fileIndex;
		u32 volumeSerial;
		u8 exists;
		u8 useCache;
		u32 lastError;
	};

	inline bool CanDetour(const tchar* file) { return !t_disallowDetour && g_rules->CanDetour(file); }
	inline bool KeepInMemory(const tchar* fileName, u32 fileNameLen) { return g_allowKeepFilesInMemory && g_rules->KeepInMemory(fileName, fileNameLen, g_systemTemp.data); }

	void Shared_WriteConsole(const char* chars, u32 charCount, bool isError);
	void Shared_WriteConsole(const wchar_t* chars, u32 charCount, bool isError);

	const tchar* Shared_GetFileAttributes(FileAttributes& outAttr, const tchar* fileName, bool checkIfDir = false);

	void InitSharedVariables();

	template<typename T> struct VariableMem { template<typename... Args> void Create(Args&&... args) { new (data) T(args...); }; u64 data[AlignUp(sizeof(T), sizeof(u64)) / sizeof(u64)]; };
	#define VARIABLE_MEM(type, name) VariableMem<type> name##Mem; type& name = (type&)name##Mem.data;
}

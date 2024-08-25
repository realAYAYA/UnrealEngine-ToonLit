// Copyright Epic Games, Inc. All Rights Reserved.

#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES 0
#define UBA_IS_DETOURED_INCLUDE 1

#include "UbaDetoursFunctionsWin.h"
#include "UbaDetoursFileMappingTable.h"
#include "UbaDetoursApi.h"

#if !defined(UBA_USE_MIMALLOC)
#define True_malloc malloc
#else
// Taken from mimalloc/types.h
#define MI_ZU(x)  x##ULL
#define MI_SEGMENT_MASK                   ((uintptr_t)(MI_SEGMENT_ALIGN - 1))
#define MI_SEGMENT_ALIGN                  MI_SEGMENT_SIZE
#define MI_SEGMENT_SIZE                   (MI_ZU(1)<<MI_SEGMENT_SHIFT)
#define MI_SEGMENT_SHIFT                  ( 9 + MI_SEGMENT_SLICE_SHIFT)  // 32MiB
#define MI_SEGMENT_SLICE_SHIFT            (13 + MI_INTPTR_SHIFT)
#define MI_INTPTR_SHIFT (3)
#endif

#include <ntstatus.h>
#define WIN32_NO_STATUS

#include <malloc.h>

NTSTATUS NtCopyFileChunk(HANDLE Source, HANDLE Dest, HANDLE Event, PIO_STATUS_BLOCK IoStatusBlock, ULONG Length, PULONG SourceOffset, PULONG DestOffset, PULONG SourceKey, PULONG DestKey, ULONG Flags) { return 0; }
#define DETOURED_FUNCTION(Func) decltype(Func)* True_##Func = ::Func; 
DETOURED_FUNCTIONS
DETOURED_FUNCTIONS_MEMORY
#undef DETOURED_FUNCTION

#define DETOURED_FUNCTION(Func) void* Local_##Func = ::Func;
DETOURED_FUNCTIONS_MEMORY
#undef DETOURED_FUNCTION

#include "UbaDirectoryTable.h"
#include "UbaProcessStats.h"
#include "UbaProtocol.h"
#include "UbaWinBinDependencyParser.h"
#include "UbaDetoursPayload.h"
#include "UbaApplicationRules.h"

#include "UbaDetoursShared.h"

#include "Shlwapi.h"
#include <detours/detours.h>
#include <stdio.h>

namespace uba
{

bool g_useMiMalloc;
constexpr u64  g_pageSize = 64*1024;

thread_local u32 t_disallowCreateFileDetour	 = 0; // Set this to 1 to disallow file detour.. note that this will prevent directory cache from properly being updated

// Beautiful! cl.exe needs an exact address in that range to be able to map in pch file
// So we'll reserve a bigger range than will be requested and give it back when needed.
constexpr uintptr_t g_clExeBaseAddress = 0x6bb00000000;
constexpr u64 g_clExeBaseAddressSize = 0x400000000;
void* g_clExeBaseReservedMemory = 0;

HANDLE PseudoHandle = (HANDLE)0xfffffffffffffffe;
constexpr int StdOutFd = -2;

#if UBA_DEBUG_LOG_ENABLED
bool g_debugFileFlushOnWrite = false;

void WriteDebug(const char* s, u32 strLen)
{
	DWORD toWrite = (DWORD)strLen;
	DWORD lastError = GetLastError();
	while (true)
	{
		DWORD written;
		if (True_WriteFile((HANDLE)g_debugFile, s, toWrite, &written, NULL))
			break;
		if (GetLastError() != ERROR_IO_PENDING)
			break;// ExitProcess(1340); // During shutdown this might actually cause error and we just ignore that and break out
		s += written;
		toWrite -= written;
	}
	if (g_debugFileFlushOnWrite)
		True_FlushFileBuffers((HANDLE)g_debugFile);
	SetLastError(lastError);
}
void FlushDebugLog()
{
	if (isLogging())
		True_FlushFileBuffers((HANDLE)g_debugFile);
}
#endif

//#define UBA_PROFILE_DETOURED_CALLS

#if defined(UBA_PROFILE_DETOURED_CALLS)
#define DETOURED_FUNCTION(name) Timer timer##name;
DETOURED_FUNCTIONS
DETOURED_FUNCTIONS_MEMORY
#undef DETOURED_FUNCTION
#define DETOURED_CALL(name) TimerScope _(timer##name)
#else
#define DETOURED_CALL(name)
#endif

bool g_isDetachedProcess;
bool g_isRunningWine;
int g_uiLanguage;

StringBuffer<256> g_exeDir;

const wchar_t* g_commandLine;
wchar_t* g_virtualCommandLine;

constexpr u32 TrackInputsMemCapacity = 512 * 1024;
u8* g_trackInputsMem;
u32 g_trackInputsBufPos;
void TrackInput(const wchar_t* file)
{
	if (g_trackInputsMem)
	{
		BinaryWriter w(g_trackInputsMem, g_trackInputsBufPos, TrackInputsMemCapacity);
		w.WriteString(file);
		g_trackInputsBufPos = u32(w.GetPosition());
	}
}

struct MemoryFile
{
	MemoryFile(u8* data = nullptr, bool localOnly = true) : baseAddress(data), isLocalOnly(localOnly) {}
	MemoryFile(bool localOnly, u64 reserveSize_) : isLocalOnly(localOnly)
	{
		Reserve(reserveSize_);
	}

	void Reserve(u64 reserveSize_)
	{
		reserveSize = reserveSize_;
		if (isLocalOnly)
		{
			baseAddress = (u8*)VirtualAlloc(NULL, reserveSize, MEM_RESERVE, PAGE_READWRITE);
			if (!baseAddress)
				FatalError(1354, L"VirtualAlloc failed trying to reserve %llu. (Error code: %u)", reserveSize, GetLastError());
			mappedSize = reserveSize;
		}
		else
		{
			mappedSize = 32 * 1024 * 1024;
			mappingHandle = True_CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_RESERVE, ToHigh(reserveSize), ToLow(reserveSize), NULL);
			if (!mappingHandle)
				FatalError(1348, L"CreateFileMappingW failed trying to reserve %llu. (Error code: %u)", reserveSize, GetLastError());
			baseAddress = (u8*)True_MapViewOfFile(mappingHandle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, mappedSize);
			if (!baseAddress)
				FatalError(1353, L"MapViewOfFile failed trying to map %llu. ReservedSize: %llu (Error code: %u)", mappedSize, reserveSize, GetLastError());
		}
	}

	void Unreserve()
	{
		if (isLocalOnly)
		{
			VirtualFree(baseAddress, 0, MEM_RELEASE);
		}
		else
		{
			True_UnmapViewOfFile(baseAddress);
			CloseHandle(mappingHandle);
			mappingHandle = nullptr;
		}
		baseAddress = nullptr;
		committedSize = 0;
	}

	void Write(struct DetouredHandle& handle, LPCVOID lpBuffer, u64 nNumberOfBytesToWrite);
	void EnsureCommited(struct DetouredHandle& handle, u64 size);

	u64 fileIndex = ~u64(0);
	u64 fileTime = ~u64(0);
	u32 volumeSerial = 0;

	HANDLE mappingHandle = nullptr;
	u8* baseAddress;
	u64 reserveSize = 0;
	u64 mappedSize = 0;
	u64 committedSize = 0;
	u64 writtenSize = 0;
	bool isLocalOnly;
	bool isReported = false;
};
u8 g_emptyMemoryFileMem;
MemoryFile& g_emptyMemoryFile = *new MemoryFile(&g_emptyMemoryFileMem, true);

struct FileObject
{
	void* operator new(size_t size);
	void operator delete(void* p);
	FileInfo* fileInfo = nullptr;
	u32 refCount = 1;
	u32 closeId = 0;
	u32 desiredAccess = 0;
	bool deleteOnClose = false;
	bool ownsFileInfo = false;
	TString newName;
};
BlockAllocator<FileObject> g_fileObjectAllocator(g_memoryBlock);
void* FileObject::operator new(size_t size) { return g_fileObjectAllocator.Allocate(); }
void FileObject::operator delete(void* p) { g_fileObjectAllocator.Free(p); }


enum HandleType
{
	HandleType_File,
	HandleType_FileMapping,
	HandleType_Process,
	HandleType_Std,
};

struct DetouredHandle
{
	void* operator new(size_t size);
	void operator delete(void* p);

	DetouredHandle(HandleType t, HANDLE th = INVALID_HANDLE_VALUE) : trueHandle(th), type(t) {}

	HANDLE trueHandle;
	u32 dirTableOffset = ~u32(0);
	HandleType type;

	// Only for files
	FileObject* fileObject = nullptr;
    u64 pos = 0;
};

constexpr u64 DetouredHandleMaxCount = 200*1024; // ~200000 handles enough?
constexpr u64 DetouredHandleStart = 300000; // Let's hope noone uses the handles starting at 300000! :)
constexpr u64 DetouredHandleEnd = DetouredHandleStart + DetouredHandleMaxCount;
constexpr u64 DetouredHandlesMemReserve = AlignUp(DetouredHandleMaxCount*sizeof(DetouredHandle), 64*1024);
constexpr u64 DetouredHandlesMemStart = 0;
MemoryBlock& g_detouredHandleMemoryBlock = *new MemoryBlock(DetouredHandlesMemReserve, (void*)DetouredHandlesMemStart);
u64 g_detouredHandlesStart = u64(g_detouredHandleMemoryBlock.memory);
u64 g_detouredHandlesEnd = g_detouredHandlesStart + g_detouredHandleMemoryBlock.reserveSize;
BlockAllocator<DetouredHandle> g_detouredHandleAllocator(g_detouredHandleMemoryBlock);
void* DetouredHandle::operator new(size_t size) { return g_detouredHandleAllocator.Allocate(); }
void DetouredHandle::operator delete(void* p) { g_detouredHandleAllocator.Free(p); }

inline bool isDetouredHandle(HANDLE h)
{
	return u64(h) >= DetouredHandleStart && u64(h) < DetouredHandleEnd;
}

inline HANDLE makeDetouredHandle(DetouredHandle* p)
{
	u64 index = (u64(p) - g_detouredHandlesStart) / sizeof(DetouredHandle);
	UBA_ASSERT(index < DetouredHandleMaxCount);
	return HANDLE(DetouredHandleStart + index);
}

inline DetouredHandle& asDetouredHandle(HANDLE h)
{
	u64 index = u64(h) - DetouredHandleStart;
	u64 p = (index * sizeof(DetouredHandle)) + g_detouredHandlesStart;
	return *(DetouredHandle*)p;
}

HANDLE g_stdHandle[3];
HANDLE g_nullFile;

struct ListDirectoryHandle
{
	void* operator new(size_t size);
	void operator delete(void* p);

	StringKey dirNameKey;
	DirectoryTable::Directory& dir;
	int it;
	Vector<u32> fileTableOffsets;
	HANDLE validateHandle;
	TString wildcard;
	const wchar_t* originalName = nullptr;
};

constexpr u64 ListDirHandlesRange = 4*1024*1024;
MemoryBlock& g_listDirHandleMemoryBlock = *new MemoryBlock(ListDirHandlesRange);
u64 g_listDirHandlesStart = u64(g_listDirHandleMemoryBlock.memory);
u64 g_listDirHandlesEnd = g_listDirHandlesStart + g_listDirHandleMemoryBlock.reserveSize;
BlockAllocator<ListDirectoryHandle> g_listDirectoryHandleAllocator(g_listDirHandleMemoryBlock);
void* ListDirectoryHandle::operator new(size_t size) { return g_listDirectoryHandleAllocator.Allocate(); }
void ListDirectoryHandle::operator delete(void* p) { g_listDirectoryHandleAllocator.Free(p); }
inline bool isListDirectoryHandle(HANDLE h) { return (u64)h >= g_listDirHandlesStart && (u64)h < g_listDirHandlesEnd; }
inline HANDLE makeListDirectoryHandle(ListDirectoryHandle* p) { return HANDLE(p);}
inline ListDirectoryHandle& asListDirectoryHandle(HANDLE h) { return *(ListDirectoryHandle*)h; }

ReaderWriterLock g_loadedModulesLock;
UnorderedMap<HMODULE, TString> g_loadedModules;
u64 g_memoryFileIndexCounter = ~u64(0) - 1000000; // I really hope this will not collide with anything

struct SuppressCreateFileDetourScope
{
	SuppressCreateFileDetourScope() { ++t_disallowCreateFileDetour; }
	~SuppressCreateFileDetourScope() { --t_disallowCreateFileDetour; }
};

const wchar_t* HandleToName(DetouredHandle& dh)
{
	if (dh.fileObject)
		if (const wchar_t* name = dh.fileObject->fileInfo->name)
			return name;
	return L"Unknown";
}


void MemoryFile::Write(DetouredHandle& handle, LPCVOID lpBuffer, u64 nNumberOfBytesToWrite)
{
	u64 newPos = handle.pos + nNumberOfBytesToWrite;
	EnsureCommited(handle, newPos);
	memcpy(baseAddress + handle.pos, lpBuffer, nNumberOfBytesToWrite);
	handle.pos += nNumberOfBytesToWrite;
	if (writtenSize < newPos)
	{
		writtenSize = newPos;
		isReported = false;
	}
}

void MemoryFile::EnsureCommited(DetouredHandle& handle, u64 size)
{
	if (committedSize >= size)
		return;
	if (size > mappedSize)
	{
		bool shouldRemap = true;
		if (size > reserveSize)
		{
			if (writtenSize == 0 && !isReported)
			{
				u64 newReserve = AlignUp(size, g_pageSize);
				Rpc_WriteLogf(L"TODO: RE-RESERVING MemoryFile. Initial reserve: %llu, New reserve: %llu. Please fix application rules", reserveSize, newReserve);
				Unreserve();
				Reserve(newReserve);
				shouldRemap = false;
			}
			else
				FatalError(1347, L"Reserved size of %ls is smaller than what is requested to be. ReserveSize: %llu Written: %llu Requested: %llu", HandleToName(handle), reserveSize, writtenSize, size);
		}

		if (shouldRemap)
		{
			True_UnmapViewOfFile(baseAddress);
			mappedSize = Min(reserveSize, AlignUp(Max(size, mappedSize * 4), g_pageSize));
			baseAddress = (u8*)True_MapViewOfFile(mappingHandle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, mappedSize);
			if (!baseAddress)
				FatalError(1347, L"MapViewOfFile failed trying to map %llu for %ls. ReservedSize: %llu (Error code: %u)", mappedSize, HandleToName(handle), reserveSize, GetLastError());
		}
	}

	u64 toCommit = Min(reserveSize, AlignUp(size - committedSize, g_pageSize));
	if (!VirtualAlloc(baseAddress + committedSize, toCommit, MEM_COMMIT, PAGE_READWRITE))
		FatalError(1347, L"Failed to ensure virtual memory for %ls. MappedSize: %llu, CommittedSize: %llu RequestedSize: %llu. (%u)", HandleToName(handle), mappedSize, committedSize, size, GetLastError());
	committedSize += toCommit;
}

void ToInvestigate(const wchar_t* format, ...)
{
#if UBA_DEBUG_LOG_ENABLED
	va_list arg;
	va_start (arg, format);
	StringBuffer<> buffer;
	buffer.Append(format, arg);
	va_end (arg);
	DEBUG_LOG(buffer.data);
	FlushDebugLog();
	Rpc_WriteLogf(L"%ls\n", buffer.data);
#endif
}

void UbaAssert(const wchar_t* text, const char* file, u32 line, const char* expr, u32 terminateCode, bool allowTerminate)
{
	SuppressDetourScope _;
	
	StringBuffer<32*1024> b;
	WriteAssertInfo(b, text, file, line, expr, 1);
	Rpc_WriteLog(b.data, b.count, true, true);
	#if UBA_DEBUG_LOG_ENABLED
	FlushDebugLog();
	#endif

	#if UBA_ASSERT_MESSAGEBOX
	StringBuffer<> title;
	title.Appendf(L"Assert %ls - pid %u", GetApplicationShortName(), GetCurrentProcessId());
	int ret = MessageBoxW(GetConsoleWindow(), b.data, title.data, MB_ABORTRETRYIGNORE|MB_SYSTEMMODAL);
	if (ret == IDABORT)
		ExitProcess(terminateCode);
	else if (ret == IDRETRY)
		DebugBreak();
	#else
	if (allowTerminate)
		ExitProcess(terminateCode);
	#endif
}

void UbaAssert(const wchar_t* text, const char* file, u32 line, const char* expr, u32 terminateCode)
{
	UbaAssert(text, file, line, expr, terminateCode, true);
}

extern HANDLE g_hostProcess;

const wchar_t* HandleToName(HANDLE handle)
{
	if (handle == INVALID_HANDLE_VALUE)
		return L"INVALID";
	if (isListDirectoryHandle(handle))
	{
		#if UBA_DEBUG
		return asListDirectoryHandle(handle).originalName;
		#else
		return L"DIRECTORY";
		#endif
	}
	if (!isDetouredHandle(handle))
		return L"UNKNOWN";
	DetouredHandle& dh = asDetouredHandle(handle);
	if (dh.fileObject)
	{
		if (const wchar_t* name = dh.fileObject->fileInfo->name)
			return name;
	}
	return L"DETOURED";
}


bool NeedsSharedMemory(const wchar_t* file) { return g_allowKeepFilesInMemory && g_rules->NeedsSharedMemory(file); }
u64 FileTypeMaxSize(const StringBufferBase& file, bool isSystemOrTempFile) { return g_rules->FileTypeMaxSize(file, isSystemOrTempFile); }
bool IsOutputFile(LPCWSTR fileName, u64 fileNameLen, DWORD desiredAccess, bool isDeleteOnClose = false) { return ((desiredAccess & GENERIC_WRITE) || isDeleteOnClose) && g_allowKeepFilesInMemory && g_rules->IsOutputFile(fileName, fileNameLen); }


bool EnsureMapped(DetouredHandle& handle, DWORD dwFileOffsetHigh = 0, DWORD dwFileOffsetLow = 0, SIZE_T numberOfBytesToMap = 0, void* baseAddress = nullptr)
{
	FileInfo& info = *handle.fileObject->fileInfo;
	
	if (info.memoryFile || info.fileMapMem)
		return true;

	u64 offset = ToLargeInteger(dwFileOffsetHigh, dwFileOffsetLow).QuadPart;
	if (!numberOfBytesToMap)
	{
		UBA_ASSERTF(info.size && info.size != InvalidValue || (info.isFileMap && info.size == 0), L"FileInfo file size is bad: %llu", info.size);
		numberOfBytesToMap = info.size;
	}

	u64 alignedOffsetStart = 0;
	if (info.trueFileMapOffset)
	{
		offset += info.trueFileMapOffset;
		u64 endOffset = offset + numberOfBytesToMap;
		alignedOffsetStart = AlignUp(offset - (g_pageSize - 1), g_pageSize);
		u64 alignedOffsetEnd = AlignUp(endOffset, g_pageSize);
		u64 mapSize = alignedOffsetEnd - alignedOffsetStart;
		info.fileMapMem = (u8*)True_MapViewOfFileEx(info.trueFileMapHandle, info.fileMapViewDesiredAccess, ToHigh(alignedOffsetStart), ToLow(alignedOffsetStart), mapSize, baseAddress);
	}
	else
	{
		info.fileMapMem = (u8*)True_MapViewOfFileEx(info.trueFileMapHandle, info.fileMapViewDesiredAccess, 0, 0, numberOfBytesToMap, baseAddress);
	}

	if (info.fileMapMem == nullptr || (baseAddress && info.fileMapMem != baseAddress))
	{
		ToInvestigate(L"MapViewOfFileEx failed trying to map %llu bytes on address 0x%llx with offset %llu, using %llu with access %u (%u)", numberOfBytesToMap, u64(baseAddress), alignedOffsetStart, u64(info.trueFileMapHandle), info.fileMapViewDesiredAccess, GetLastError());
		return false;
	}
	info.fileMapMem += (offset - alignedOffsetStart);
	info.fileMapMemEnd = info.fileMapMem + info.size;

	DEBUG_LOG_TRUE(L"INTERNAL MapViewOfFileEx", L"(%ls) (size: %llu) (%ls) -> 0x%llx", info.name, numberOfBytesToMap, info.originalName, uintptr_t(info.fileMapMem));
	return true;
}

enum : u8 { AccessFlag_Read = 1, AccessFlag_Write = 2 };
u8 GetFileAccessFlags(DWORD dwDesiredAccess)
{
	u8 access = 0;
	if (dwDesiredAccess & GENERIC_READ)
		access |= AccessFlag_Read;
	if (dwDesiredAccess & GENERIC_WRITE)
		access |= AccessFlag_Write;
	return access;
}

ReaderWriterLock g_longPathNameCacheLock;
GrowingUnorderedMap<const wchar_t*, const wchar_t*, HashString, EqualString> g_longPathNameCache(&g_memoryBlock);

void Rpc_AllocFailed(const wchar_t* allocType, u32 error)
{
	TimerScope ts(g_stats.virtualAllocFailed);
	SCOPED_WRITE_LOCK(g_communicationLock, pcs);
	BinaryWriter writer;
	writer.WriteByte(MessageType_VirtualAllocFailed);
	writer.WriteString(allocType);
	writer.WriteU32(error);
	writer.Flush();
	Sleep(5*1000);
}

StringBuffer<32 * 1024> g_stdFile;
ReaderWriterLock g_stdFileLock;

void CloseCaches()
{
	for (auto& it : g_mappedFileTable.m_lookup)
	{
		FileInfo& info = it.second;
		if (info.fileMapMem)
		{
			DEBUG_LOG_TRUE(L"INTERNAL UnmapViewOfFile", L"%llu (%ls) (%ls)", uintptr_t(info.fileMapMem), info.name, info.originalName);
			True_UnmapViewOfFile(info.fileMapMem);
		}
		if (info.trueFileMapHandle != 0)
		{
			DEBUG_LOG_TRUE(L"INTERNAL CloseHandle", L"%llu (%ls) (%ls)", uintptr_t(info.trueFileMapHandle), info.name, info.originalName);
			CloseHandle(info.trueFileMapHandle);
		}

		// Let them leak
		if (info.memoryFile && !info.memoryFile->isLocalOnly)
		{
			UnmapViewOfFile(info.memoryFile->baseAddress);
			CloseHandle(info.memoryFile->mappingHandle);
		}
		//if (info.memoryFile && info.memoryFile != &g_emptyMemoryFile)
		//	delete info.memoryFile;
	}
}

bool g_exitMessageSent;

void SendExitMessage(DWORD exitCode, u64 startTime)
{
	if (g_exitMessageSent)
		return;
	g_exitMessageSent = true;

	if (g_trackInputsMem)
	{
		u32 left = g_trackInputsBufPos;
		u32 pos = 0;
		while (left)
		{
			u32 toWrite = Min(left, u32(30 * 1024));
			SCOPED_WRITE_LOCK(g_communicationLock, pcs);
			BinaryWriter writer;
			writer.WriteByte(MessageType_InputDependencies);
			if (pos == 0)
				writer.WriteU32(left);
			writer.WriteU32(toWrite);
			writer.WriteBytes(g_trackInputsMem + pos, toWrite);
			writer.Flush();
			left -= toWrite;
			pos += toWrite;
		}
	}

	g_stats.usedMemory = u32(g_memoryBlock.writtenSize);

	SCOPED_WRITE_LOCK(g_communicationLock, pcs);
	BinaryWriter writer;
	writer.WriteByte(MessageType_Exit);
	writer.WriteU32(exitCode);
	writer.WriteString(g_logName);

	g_stats.detach.time += GetTime() - startTime;
	g_stats.detach.count = 1;

	g_stats.Write(writer);

	// We must flush here if this is a child because,
	// if there is a parent process waiting for this to finish,
	// the parent might move on before Exit message has been processed on session side
	writer.Flush(g_isChild);
}

void OnModuleLoaded(HMODULE moduleHandle, const wchar_t* name);

// Variables used to communicate state from kernelbase functions to ntdll functions
thread_local const wchar_t* t_renameFileNewName;
thread_local const wchar_t* t_createFileFileName;

#include "UbaDetoursFunctionsMiMalloc.inl"
#include "UbaDetoursFunctionsNtDll.inl"
#include "UbaDetoursFunctionsKernelBase.inl"
#include "UbaDetoursFunctionsUcrtBase.inl"
#include "UbaDetoursFunctionsImagehlp.inl"
#include "UbaDetoursFunctionsDbgHelp.inl"

void DetourAttachFunction(void** trueFunc, void* detouredFunc, const char* funcName)
{
	if (!*trueFunc)
		return;
	auto error = DetourAttach(trueFunc, detouredFunc);
	if (error == NO_ERROR)
		return;
	Rpc_WriteLogf(L"Failed to detour %hs", funcName);
	ExitProcess(error);
}

void DetourDetachFunction(void** trueFunc, void* detouredFunc, const char* funcName)
{
	if (!*trueFunc)
		return;
	auto error = DetourDetach(trueFunc, detouredFunc);
	if (error == NO_ERROR)
		return;
	Rpc_WriteLogf(L"Failed to detach detoured %hs", funcName);
}

int DetourAttachFunctions(bool runningRemote)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	#define DETOURED_FUNCTION(Func) True_##Func = (decltype(True_##Func))GetProcAddress(moduleHandle, #Func);

	if (HMODULE moduleHandle = GetModuleHandleW(L"kernelbase.dll"))
	{
		DETOURED_FUNCTIONS_KERNELBASE
	}

	if (HMODULE moduleHandle = GetModuleHandleW(L"kernel32.dll"))
	{
		DETOURED_FUNCTIONS_KERNEL32
	}

	if (HMODULE moduleHandle = GetModuleHandleW(L"ntdll.dll"))
	{
		DETOURED_FUNCTIONS_NTDLL
	}

	if (HMODULE moduleHandle = GetModuleHandleW(L"ucrtbase.dll"))
	{
		DETOURED_FUNCTIONS_UCRTBASE
		if (g_useMiMalloc)
		{
			DETOURED_FUNCTIONS_MEMORY
		}
	}

	if (HMODULE moduleHandle = GetModuleHandleW(L"shlwapi.dll"))
	{
		DETOURED_FUNCTIONS_SHLWAPI
	}

	#undef DETOURED_FUNCTION

	// Can't attach to these when running through debugger with some vs extensions (Microsoft child process debugging)
#if UBA_DEBUG
	if (IsDebuggerPresent())
	{
		True_CreateProcessW = nullptr;
		#if defined(DETOURED_INCLUDE_DEBUG)
		True_CreateProcessA = nullptr;
		True_CreateProcessAsUserW = nullptr;
		#endif
	}
#endif

	#define DETOURED_FUNCTION(Func) DetourAttachFunction((PVOID*)&True_##Func, Detoured_##Func, #Func);
	DETOURED_FUNCTIONS
	if (g_useMiMalloc)
	{
		DETOURED_FUNCTIONS_MEMORY
	}
	#undef DETOURED_FUNCTION


	LONG error = DetourTransactionCommit();
	if (error != NO_ERROR)
	{
		printf("Error detouring: %ld\n", error);
		ExitProcess(1343);
	}

	return 0;
}

void OnModuleLoaded(HMODULE moduleHandle, const wchar_t* name)
{
	UBA_ASSERT(g_isRunningWine);

	// SymLoadModuleExW do something bad that cause remote wine to fail everything after this call.. TODO: Revisit
	if (!True_SymLoadModuleExW && Contains(name, L"dbghelp.dll"))
	{
		True_SymLoadModuleExW = (SymLoadModuleExWFunc*)GetProcAddress(moduleHandle, "SymLoadModuleExW");
		UBA_ASSERT(True_SymLoadModuleExW);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttachFunction((PVOID*)&True_SymLoadModuleExW, Detoured_SymLoadModuleExW, "SymLoadModuleExW");
		LONG error = DetourTransactionCommit(); (void)error;
		UBA_ASSERT(!error);
	}

	// ImageGetDigestStream is buggy in wine so we have to detour it for ShaderCompileWorker
	if (!True_ImageGetDigestStream && Contains(name, L"imagehlp.dll"))
	{
		True_ImageGetDigestStream = (ImageGetDigestStreamFunc*)GetProcAddress(moduleHandle, "ImageGetDigestStream");
		UBA_ASSERT(True_ImageGetDigestStream);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttachFunction((PVOID*)&True_ImageGetDigestStream, Detoured_ImageGetDigestStream, "ImageGetDigestStream");
		LONG error = DetourTransactionCommit(); (void)error;
		UBA_ASSERT(!error);
	}
}

int DetourDetachFunctions()
{
	if (g_directoryTable.m_memory)
		True_UnmapViewOfFile(g_directoryTable.m_memory);

	if (g_mappedFileTable.m_mem)
		True_UnmapViewOfFile(g_mappedFileTable.m_mem);

	//assert(g_wantsOnCloseLookup.empty());
	//UBA_ASSERT(g_mappedFileTable.m_memLookup.empty());

	CloseCaches();

	#define DETOURED_FUNCTION(Func) DetourDetachFunction((PVOID*)&True_##Func, Detoured_##Func, #Func);
	if (g_useMiMalloc)
	{
		DETOURED_FUNCTIONS_MEMORY
	}
	DETOURED_FUNCTIONS
	#undef DETOURED_FUNCTION
	return 0;
}

void PreInit(const DetoursPayload& payload)
{
	#if UBA_USE_MIMALLOC
	//mi_option_enable(mi_option_large_os_pages);
	mi_option_disable(mi_option_abandoned_page_reset);
	#endif

	InitSharedVariables();

	g_rulesIndex = payload.rulesIndex;
	g_rules = GetApplicationRules()[payload.rulesIndex].rules;
	g_useMiMalloc = payload.useCustomAllocator;
	g_runningRemote = payload.runningRemote;
	g_isChild = payload.isChild;
	g_isDetachedProcess = g_rules->AllowDetach();
	g_isRunningWine = payload.isRunningWine;
	g_uiLanguage = payload.uiLanguage;

	if (g_isRunningWine) // There are crashes when running in Wine and really hard to debug
	{
		//g_useMiMalloc = false;
		//g_checkRtlHeap = false;
	}

	#if UBA_DEBUG_VALIDATE
	if (g_runningRemote)
		g_validateFileAccess = false;
	#endif

	{
		if (!payload.logFile.IsEmpty())
		{
			g_logName.Append(payload.logFile);
			HANDLE debugFile = CreateFileW(payload.logFile.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			#if UBA_DEBUG_LOG_ENABLED
			g_debugFile = (FileHandle)(u64)debugFile;
			#else
			if (debugFile != INVALID_HANDLE_VALUE)
			{
				const char str[] = "Run in debug to get this file populated";
				DWORD written = 0;
				WriteFile(debugFile, str, sizeof(str), &written, NULL);
				CloseHandle(debugFile);
			}
			#endif
		}
	}

	if (g_runningRemote)
	{
		ULONG languageCount = 1;
		wchar_t languageBuffer[6];
		swprintf_s(languageBuffer, 6, L"%04x", g_uiLanguage);
		languageBuffer[5] = 0;
		if (!SetProcessPreferredUILanguages(MUI_LANGUAGE_ID, languageBuffer, &languageCount))
		{
			DEBUG_LOG(L"Failed to set locale");
		}
	}

	{
		wchar_t exeFullName[256];
		if (!GetModuleFileNameW(NULL, exeFullName, sizeof_array(exeFullName)))
			FatalError(1350, L"GetModuleFileNameW failed (%u)", GetLastError());
		wchar_t* lastSlash = wcsrchr(exeFullName, '\\');
		*lastSlash = 0;
		FixPath(g_exeDir, exeFullName);
		g_exeDir.Append('\\');
	}

	// Special cl.exe handling..  this is needed for compiles using pch files where this address _must_ be available.
	if (payload.rulesIndex == 1) // cl.exe
	{
		g_clExeBaseReservedMemory = VirtualAlloc((void*)g_clExeBaseAddress, g_clExeBaseAddressSize, MEM_RESERVE, PAGE_READWRITE);
		DEBUG_LOG(L"Reserving %llu bytes at 0x%llx for cl.exe", g_clExeBaseAddressSize, g_clExeBaseAddress);
		if (!g_clExeBaseReservedMemory)
			FatalError(1349, L"Failed to reserve memory for cl.exe (%u)", GetLastError());
	}

	// Special link.exe handling.. it seems loading bcrypt.dll can deadlock when using mimalloc so we make sure ti load it here directly instead
	// There is a setting to disable bcrypt dll loading inside mimalloc but with that change mimalloc does not work with older versions of windows
	if (payload.rulesIndex == 2)
	{
		if (!LoadLibraryExW(L"bcrypt.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32))
			FatalError(1351, L"Failed to load bcrypt.dll (%u)", GetLastError());
		if (!LoadLibraryExW(L"bcryptprimitives.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32))
			FatalError(1352, L"Failed to load bcryptprimitives.dll (%u)", GetLastError());
	}
}

void Init(const DetoursPayload& payload, u64 startTime)
{
	if (g_rules->EnableVectoredExceptionHandler())
	{
		AddVectoredExceptionHandler(0, [](_EXCEPTION_POINTERS* ExceptionInfo) -> LONG
			{
				u32 exceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
				if (exceptionCode != EXCEPTION_STACK_OVERFLOW && exceptionCode != EXCEPTION_ACCESS_VIOLATION)
					return EXCEPTION_CONTINUE_SEARCH;
				StringBuffer<> text;
				text.Appendf(L"Unhandled Exception (Code: 0x%x)", exceptionCode);
				UbaAssert(text.data, nullptr, 0, nullptr, exceptionCode, false);
				return EXCEPTION_EXECUTE_HANDLER;
			});
	}

	DetourAttachFunctions(g_runningRemote);

	if (g_isDetachedProcess)
	{
		g_stdHandle[0] = makeDetouredHandle(new DetouredHandle(HandleType_Std)); // STD_ERR
		g_stdHandle[1] = makeDetouredHandle(new DetouredHandle(HandleType_Std)); // STD_OUT
		g_stdHandle[2] = makeDetouredHandle(new DetouredHandle(HandleType_Std)); // STD_IN
	}
	else
	{
		HANDLE stderrHandle = True_GetStdHandle(STD_ERROR_HANDLE);
		g_stdHandle[0] = GetFileType(stderrHandle) == FILE_TYPE_CHAR ? stderrHandle : 0;
		
		HANDLE stdoutHandle = True_GetStdHandle(STD_OUTPUT_HANDLE);
		g_stdHandle[1] = GetFileType(stdoutHandle) == FILE_TYPE_CHAR ? stdoutHandle : 0;
	}

	if (payload.trackInputs)
		g_trackInputsMem = (u8*)g_memoryBlock.Allocate(TrackInputsMemCapacity, 1, L"TrackInputs");
	
	g_systemRoot.count = GetEnvironmentVariableW(L"SystemRoot", g_systemRoot.data, g_systemRoot.capacity);
	g_systemRoot.MakeLower();

	wchar_t systemTemp[256];
	GetEnvironmentVariableW(L"TEMP", systemTemp, 256);
	FixPath(g_systemTemp, systemTemp);

	StringBuffer<512> applicationBuffer;
	StringBuffer<512> workingDirBuffer;

	HANDLE directoryTableHandle;
	u32 directoryTableSize;
	u32 directoryTableCount;
	HANDLE mappedFileTableHandle;
	u32 mappedFileTableSize;
	u32 mappedFileTableCount;

	{
		TimerScope ts(g_stats.init);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_Init);
		writer.Flush();
		BinaryReader reader;

		g_echoOn = reader.ReadBool();

		reader.ReadString(applicationBuffer);
		reader.ReadString(workingDirBuffer);

		directoryTableHandle = (HANDLE)reader.ReadU64();
		directoryTableSize = reader.ReadU32();
		directoryTableCount = reader.ReadU32();
		mappedFileTableHandle = (HANDLE)reader.ReadU64();
		mappedFileTableSize = reader.ReadU32();
		mappedFileTableCount = reader.ReadU32();
		DEBUG_LOG_PIPE(L"Init", L"");
	}

	TrackInput(applicationBuffer.data);

	Shared_SetCurrentDirectory(workingDirBuffer.data);

	{
		FixPath(applicationBuffer.data, g_virtualWorkingDir.data, g_virtualWorkingDir.count, g_virtualApplication);

		if (const wchar_t* lastBackslash = g_virtualApplication.Last('\\'))
			g_virtualApplicationDir.Append(g_virtualApplication.data, (lastBackslash + 1 - g_virtualApplication.data));
		else
			FatalError(4444, L"What the heck: %s", g_virtualApplication.data);
	}

	const wchar_t* cmdLine = True_GetCommandLineW();

	const wchar_t* exePos;
	if (Contains(cmdLine, g_exeDir.data, true, &exePos))
	{
		StringBuffer<> buf;
		buf.Append(cmdLine, exePos - cmdLine);
		buf.Append(g_virtualApplicationDir);
		TString realCmdLine(buf.data);
		realCmdLine += (cmdLine + g_exeDir.count);
		g_virtualCommandLine = g_memoryBlock.Strdup(realCmdLine.c_str());
	}

	#if UBA_DEBUG_LOG_ENABLED
	if (isLogging())
	{
		g_commandLine = cmdLine;
		u64 cmdLineLen = wcslen(cmdLine);
		wchar_t temp[LogBufSize - 10];
		if (cmdLineLen > sizeof_array(temp))
		{
			memcpy(temp, cmdLine, sizeof(temp));
			temp[sizeof_array(temp)-1] = 0;
			cmdLine = temp;
		}
		DEBUG_LOG(L"Cmdline: %ls", cmdLine);
		DEBUG_LOG(L"WorkingDir: %ls", g_virtualWorkingDir.data);
		DEBUG_LOG(L"ExeDir: %ls", g_virtualApplicationDir.data);
		DEBUG_LOG(L"ExeDir (true): %ls", g_exeDir.data);
	}
	#endif

	if (!True_DuplicateHandle(g_hostProcess, mappedFileTableHandle, GetCurrentProcess(), &mappedFileTableHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
		UBA_ASSERTF(false, L"Failed to duplicate filetable handle (%u)", GetLastError());

	u8* mappedFileTableMem = (u8*)True_MapViewOfFile(mappedFileTableHandle, FILE_MAP_READ, 0, 0, 0);
	UBA_ASSERT(mappedFileTableMem);
	g_mappedFileTable.Init(mappedFileTableMem, mappedFileTableCount, mappedFileTableSize);

	if (!True_DuplicateHandle(g_hostProcess, directoryTableHandle, GetCurrentProcess(), &directoryTableHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
		UBA_ASSERTF(false, L"Failed to duplicate directorytable handle (%u)", GetLastError());

	u8* directoryTableMem = (u8*)True_MapViewOfFile(directoryTableHandle, FILE_MAP_READ, 0, 0, 0);
	UBA_ASSERT(directoryTableMem);
	g_directoryTable.Init(directoryTableMem, directoryTableCount, directoryTableSize);

	g_stats.attach.time += GetTime() - startTime;
	g_stats.attach.count = 1;
}

void Deinit(u64 startTime)
{
	if (g_isRunningWine) // mt.exe etc fails if detaching is not done during shutdown
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetachFunctions();
		DetourTransactionCommit();
	}

	#if defined(UBA_PROFILE_DETOURED_CALLS)
	#define DETOURED_FUNCTION(name) if (timer##name.count != 0) { char sb[1024]; sprintf_s(sb, sizeof(sb), "%s: %u %llu\n", #name, timer##name.count.load(), TimeToMs(timer##name.time.load())); WriteDebug(sb); }
	DETOURED_FUNCTIONS
	DETOURED_FUNCTIONS_MEMORY
	#undef DETOURED_FUNCTION
	#endif

	DWORD exitCode = STILL_ACTIVE;
	if (!GetExitCodeProcess(GetCurrentProcess(), &exitCode))
		exitCode = STILL_ACTIVE;

	if (!g_exitMessageSent)
		SendExitMessage(exitCode, startTime); // This should never happen? ExitProcess is always called after main function
}

void PostDeinit()
{
	DEBUG_LOG(L"Finished");
	#if UBA_DEBUG_LOG_ENABLED
	if (isLogging())
	{
		FlushDebugLog();
		HANDLE debugFile = (HANDLE)g_debugFile;
		g_debugFile = InvalidFileHandle;
		CloseHandle(debugFile);
	}
	#endif
}

}

extern "C"
{
	using namespace uba;

	UBA_DETOURED_API u32 UbaSendCustomMessage(const void* send, u32 sendSize, void* recv, u32 recvCapacity)
	{
		//TimerScope ts(g_stats.init);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_Custom);
		writer.WriteU32(sendSize);
		writer.WriteBytes(send, sendSize);
		writer.Flush();
		BinaryReader reader;
		u32 recvSize = reader.ReadU32();
		UBA_ASSERT(recvSize < recvCapacity);
		reader.ReadBytes(recv, recvSize);
		return recvSize;
	}

	UBA_DETOURED_API bool UbaFlushWrittenFiles()
	{
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_FlushWrittenFiles);
		writer.Flush();
		BinaryReader reader;
		return reader.ReadBool();
	}

	UBA_DETOURED_API bool UbaUpdateEnvironment(const wchar_t* reason, bool resetStats)
	{
		{
			SCOPED_WRITE_LOCK(g_communicationLock, pcs);
			BinaryWriter writer;
			writer.WriteByte(MessageType_UpdateEnvironment);
			writer.WriteString(reason ? reason : L"");
			writer.WriteBool(resetStats);
			writer.Flush();
			BinaryReader reader;
			if (!reader.ReadBool())
				return false;
		}
		Rpc_UpdateTables();
		return true;
	}

	UBA_DETOURED_API bool UbaRunningRemote()
	{
		return g_runningRemote;
	}

	UBA_DETOURED_API bool UbaRequestNextProcess(u32 prevExitCode, wchar_t* outArguments, u32 outArgumentsCapacity)
	{
		#if UBA_DEBUG_LOG_ENABLED
		FlushDebugLog();
		#endif

		*outArguments = 0;
		bool newProcess;
		{
			SCOPED_WRITE_LOCK(g_communicationLock, pcs);
			BinaryWriter writer;
			writer.WriteByte(MessageType_GetNextProcess);
			writer.WriteU32(prevExitCode);
			g_stats.Write(writer);


			writer.Flush();
			BinaryReader reader;
			newProcess = reader.ReadBool();
			if (newProcess)
			{
				reader.ReadString(outArguments, outArgumentsCapacity);
				reader.SkipString(); // workingDir
				reader.SkipString(); // description
				reader.ReadString(g_logName.Clear());
			}
		}

		if (newProcess)
		{
			g_stats = {};

			#if UBA_DEBUG_LOG_ENABLED
			SuppressCreateFileDetourScope scope;
			HANDLE debugFile = (HANDLE)g_debugFile;
			g_debugFile = InvalidFileHandle;
			CloseHandle(debugFile);
			debugFile = CreateFileW(g_logName.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			g_debugFile = (FileHandle)(u64)debugFile;
			#endif
		}

		Rpc_UpdateTables();
		return newProcess;
	}
}

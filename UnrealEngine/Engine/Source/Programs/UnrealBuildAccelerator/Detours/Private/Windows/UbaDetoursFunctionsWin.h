// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaPlatform.h"
#include <winternl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <io.h>
#include <aclapi.h>
#include <mbstring.h>
#include <Shlwapi.h>
#include <wchar.h>
#include <stdio.h>


#if UBA_DEBUG
#define DETOURED_INCLUDE_DEBUG
#endif



#define DETOURED_FUNCTIONS \
	DETOURED_FUNCTIONS_KERNELBASE \
	DETOURED_FUNCTIONS_KERNEL32 \
	DETOURED_FUNCTIONS_NTDLL \
	DETOURED_FUNCTIONS_SHLWAPI \
	DETOURED_FUNCTIONS_UCRTBASE \

#define DETOURED_FUNCTIONS_KERNELBASE \
	DETOURED_FUNCTION(GetCommandLineW) \
	DETOURED_FUNCTION(GetCurrentDirectoryW) \
	DETOURED_FUNCTION(GetCurrentDirectoryA) \
	DETOURED_FUNCTION(SetCurrentDirectoryW) \
	DETOURED_FUNCTION(DuplicateHandle) \
	DETOURED_FUNCTION(CreateFileW) \
	DETOURED_FUNCTION(CreateFileA) \
	DETOURED_FUNCTION(CreateDirectoryW) \
	DETOURED_FUNCTION(RemoveDirectoryW) \
	DETOURED_FUNCTION(LockFile) \
	DETOURED_FUNCTION(LockFileEx) \
	DETOURED_FUNCTION(UnlockFile) \
	DETOURED_FUNCTION(UnlockFileEx) \
	DETOURED_FUNCTION(ReadFile) \
	DETOURED_FUNCTION(WriteFile) \
	DETOURED_FUNCTION(WriteFileEx) \
	DETOURED_FUNCTION(FlushFileBuffers) \
	DETOURED_FUNCTION(GetFileSize) \
	DETOURED_FUNCTION(GetFileSizeEx) \
	DETOURED_FUNCTION(SetFilePointer) \
	DETOURED_FUNCTION(SetFilePointerEx) \
	DETOURED_FUNCTION(SetEndOfFile) \
	DETOURED_FUNCTION(SetFileTime) \
	DETOURED_FUNCTION(GetFileTime) \
	DETOURED_FUNCTION(GetFileType) \
	DETOURED_FUNCTION(GetLongPathNameW) \
	DETOURED_FUNCTION(GetFullPathNameW) \
	DETOURED_FUNCTION(GetVolumePathNameW) \
	DETOURED_FUNCTION(GetModuleFileNameW) \
	DETOURED_FUNCTION(GetModuleFileNameExW) \
	DETOURED_FUNCTION(GetFileAttributesW) \
	DETOURED_FUNCTION(SetFileAttributesW) \
	DETOURED_FUNCTION(GetFileAttributesExW) \
	DETOURED_FUNCTION(CopyFileW) \
	DETOURED_FUNCTION(CopyFileExW) \
	DETOURED_FUNCTION(CreateHardLinkW) \
	DETOURED_FUNCTION(DeleteFileW) \
	DETOURED_FUNCTION(MoveFileWithProgressW) \
	DETOURED_FUNCTION(MoveFileExW) \
	DETOURED_FUNCTION(FindFirstFileW) \
	DETOURED_FUNCTION(FindFirstFileExW) \
	DETOURED_FUNCTION(FindNextFileW) \
	DETOURED_FUNCTION(FindClose) \
	DETOURED_FUNCTION(SetFileInformationByHandle) \
	DETOURED_FUNCTION(CreateFileMappingW) \
	DETOURED_FUNCTION(OpenFileMappingW) \
	DETOURED_FUNCTION(MapViewOfFile) \
	DETOURED_FUNCTION(MapViewOfFileEx) \
	DETOURED_FUNCTION(UnmapViewOfFile) \
	DETOURED_FUNCTION(UnmapViewOfFileEx) \
	DETOURED_FUNCTION(GetFinalPathNameByHandleW) \
	DETOURED_FUNCTION(CreateProcessW) \
	DETOURED_FUNCTION(TerminateProcess) \
	DETOURED_FUNCTION(SearchPathW) \
	DETOURED_FUNCTION(LoadLibraryExW) \
	DETOURED_FUNCTION(GetStdHandle) \
	DETOURED_FUNCTION(SetStdHandle) \
	DETOURED_FUNCTION(GetConsoleMode) \
	DETOURED_FUNCTION(SetConsoleMode) \
	DETOURED_FUNCTION(GetDriveTypeW) \
	DETOURED_FUNCTION(GetDiskFreeSpaceExW) \
	DETOURED_FUNCTION(GetFileInformationByHandleEx) \
	DETOURED_FUNCTION(GetFileInformationByHandle) \
	DETOURED_FUNCTION(GetVolumeInformationByHandleW) \
	DETOURED_FUNCTION(GetVolumeInformationW) \
	DETOURED_FUNCTION(GetUserDefaultUILanguage) \
	DETOURED_FUNCTION(GetThreadPreferredUILanguages) \
	DETOURED_FUNCTION(GetConsoleTitleW) \
	DETOURED_FUNCTION(WaitForSingleObject) \
	DETOURED_FUNCTION(WaitForSingleObjectEx) \
	DETOURED_FUNCTION(WaitForMultipleObjects) \
	DETOURED_FUNCTION(WaitForMultipleObjectsEx) \
	DETOURED_FUNCTION(WriteConsoleA) \
	DETOURED_FUNCTION(WriteConsoleW) \
	DETOURED_FUNCTION(ReadConsoleW) \
	DETOURED_FUNCTION(ExitProcess) \
	DETOURED_FUNCTION(VirtualAlloc) \
	DETOURED_FUNCTION(GetQueuedCompletionStatusEx) \
	DETOURED_FUNCTION(GetSecurityInfo) \
	DETOURED_FUNCTIONS_KERNELBASE_DEBUG \

#define DETOURED_FUNCTIONS_KERNEL32 \
	DETOURED_FUNCTION(CreateFileMappingA) \
	DETOURED_FUNCTION(GetExitCodeProcess) \
	DETOURED_FUNCTION(CreateTimerQueueTimer) \
	DETOURED_FUNCTION(DeleteTimerQueueTimer) \
	DETOURED_FUNCTIONS_KERNEL32_DEBUG \

#define DETOURED_FUNCTIONS_NTDLL \
	DETOURED_FUNCTION(NtClose) \
	DETOURED_FUNCTION(NtCreateFile) \
	DETOURED_FUNCTION(NtOpenFile) \
	DETOURED_FUNCTION(NtFsControlFile) \
	DETOURED_FUNCTION(NtCopyFileChunk) \
	DETOURED_FUNCTION(NtQueryVolumeInformationFile) \
	DETOURED_FUNCTION(NtQueryInformationFile) \
	DETOURED_FUNCTION(NtQueryDirectoryFile) \
	DETOURED_FUNCTION(NtQueryObject) \
	DETOURED_FUNCTION(NtQueryInformationProcess) \
	DETOURED_FUNCTION(NtSetInformationFile) \
	DETOURED_FUNCTION(NtCreateSection) \
	DETOURED_FUNCTION(RtlSizeHeap) \
	DETOURED_FUNCTION(RtlFreeHeap) \
	DETOURED_FUNCTION(RtlAnsiStringToUnicodeString) \
	DETOURED_FUNCTION(RtlUnicodeStringToAnsiString) \
	DETOURED_FUNCTIONS_NTDLL_DEBUG \

#define DETOURED_FUNCTIONS_SHLWAPI \
	DETOURED_FUNCTIONS_SHLWAPI_DEBUG

#define DETOURED_FUNCTIONS_UCRTBASE \
	DETOURED_FUNCTION(_wgetcwd) \
	DETOURED_FUNCTION(_wfullpath) \
	DETOURED_FUNCTION(_fullpath) \
	DETOURED_FUNCTION(_get_wpgmptr) \
	DETOURED_FUNCTION(_waccess_s) \
	DETOURED_FUNCTION(_wspawnl) \
	DETOURED_FUNCTION(_get_osfhandle) \
	DETOURED_FUNCTION(_isatty) \
	DETOURED_FUNCTION(_write) \
	DETOURED_FUNCTION(fputs) \
	DETOURED_FUNCTION(_wsplitpath_s) \
	DETOURED_FUNCTIONS_UCRTBASE_DEBUG \

#if UBA_USE_MIMALLOC
#define DETOURED_FUNCTIONS_MEMORY \
	DETOURED_FUNCTION(malloc) \
	DETOURED_FUNCTION(calloc) \
	DETOURED_FUNCTION(_recalloc) \
	DETOURED_FUNCTION(realloc) \
	DETOURED_FUNCTION(_expand) \
	DETOURED_FUNCTION(_msize) \
	DETOURED_FUNCTION(free) \
	DETOURED_FUNCTION(_strdup) \
	DETOURED_FUNCTION(_wcsdup) \
	DETOURED_FUNCTION(_mbsdup) \
	DETOURED_FUNCTION(_aligned_malloc) \
	DETOURED_FUNCTION(_aligned_realloc) \
	DETOURED_FUNCTION(_aligned_recalloc) \
	DETOURED_FUNCTION(_aligned_free) \
	DETOURED_FUNCTION(_aligned_offset_malloc) \
	DETOURED_FUNCTION(_aligned_offset_realloc) \
	DETOURED_FUNCTION(_aligned_offset_recalloc) \
	DETOURED_FUNCTION(_dupenv_s) \
	DETOURED_FUNCTION(_wdupenv_s) \
	DETOURED_FUNCTION(_malloc_base) \
	DETOURED_FUNCTION(_calloc_base) \
	DETOURED_FUNCTION(_realloc_base) \
	DETOURED_FUNCTION(_free_base) \
	DETOURED_FUNCTION(_expand_base) \
	DETOURED_FUNCTION(_msize_base) \
	DETOURED_FUNCTION(_recalloc_base) \
	DETOURED_FUNCTIONS_MEMORY_DEBUG \

#else
#define DETOURED_FUNCTIONS_MEMORY
#endif

#if defined(DETOURED_INCLUDE_DEBUG)

#define DETOURED_FUNCTIONS_KERNELBASE_DEBUG \
	DETOURED_FUNCTION(FreeLibrary) \
	DETOURED_FUNCTION(RegOpenKeyExW) \
	DETOURED_FUNCTION(SetLastError) \
	DETOURED_FUNCTION(GetLastError) \
	DETOURED_FUNCTION(RegOpenKeyExA) \
	DETOURED_FUNCTION(RegCloseKey) \
	DETOURED_FUNCTION(IsValidCodePage) \
	DETOURED_FUNCTION(GetACP) \
	DETOURED_FUNCTION(GetConsoleWindow) \
	DETOURED_FUNCTION(SetConsoleCursorPosition) \
	DETOURED_FUNCTION(GetConsoleScreenBufferInfo) \
	DETOURED_FUNCTION(ScrollConsoleScreenBufferW) \
	DETOURED_FUNCTION(FillConsoleOutputAttribute) \
	DETOURED_FUNCTION(FillConsoleOutputCharacterW) \
	DETOURED_FUNCTION(FlushConsoleInputBuffer) \
	DETOURED_FUNCTION(SetConsoleTextAttribute) \
	DETOURED_FUNCTION(SetConsoleTitleW) \
	DETOURED_FUNCTION(CreateConsoleScreenBuffer) \
	DETOURED_FUNCTION(CreatePseudoConsole) \
	DETOURED_FUNCTION(CreateProcessAsUserW) \
	DETOURED_FUNCTION(SetConsoleCtrlHandler) \
	DETOURED_FUNCTION(GetConsoleOutputCP) \
	DETOURED_FUNCTION(ReadConsoleInputA) \
	DETOURED_FUNCTION(GetLocaleInfoEx) \
	DETOURED_FUNCTION(GetUserDefaultLocaleName) \
	DETOURED_FUNCTION(GetDiskFreeSpaceExA) \
	DETOURED_FUNCTION(GetLongPathNameA) \
	DETOURED_FUNCTION(GetFullPathNameA) \
	DETOURED_FUNCTION(GetFileAttributesA) \
	DETOURED_FUNCTION(GetFileAttributesExA) \
	DETOURED_FUNCTION(GetModuleFileNameA) \
	DETOURED_FUNCTION(GetModuleFileNameExA) \
	DETOURED_FUNCTION(GetModuleBaseNameA) \
	DETOURED_FUNCTION(GetModuleBaseNameW) \
	DETOURED_FUNCTION(SetUnhandledExceptionFilter) \
	DETOURED_FUNCTION(CreateProcessA) \
	DETOURED_FUNCTION(FlushInstructionCache) \
	DETOURED_FUNCTION(CreateFile2) \
	DETOURED_FUNCTION(CreateFileTransactedW) \
	DETOURED_FUNCTION(OpenFile) \
	DETOURED_FUNCTION(ReOpenFile) \
	DETOURED_FUNCTION(ReadFileEx) \
	DETOURED_FUNCTION(ReadFileScatter) \
	DETOURED_FUNCTION(SetFileValidData) \
	DETOURED_FUNCTION(ReplaceFileW) \
	DETOURED_FUNCTION(CreateHardLinkA) \
	DETOURED_FUNCTION(DeleteFileA) \
	DETOURED_FUNCTION(CreateSymbolicLinkW) \
	DETOURED_FUNCTION(CreateSymbolicLinkA) \
	DETOURED_FUNCTION(SetEnvironmentVariableW) \
	DETOURED_FUNCTION(GetEnvironmentVariableW) \
	DETOURED_FUNCTION(GetEnvironmentVariableA) \
	DETOURED_FUNCTION(GetEnvironmentStringsW) \
	DETOURED_FUNCTION(ExpandEnvironmentStringsW) \
	DETOURED_FUNCTION(GetTempFileNameW) \
	DETOURED_FUNCTION(CreateDirectoryExW) \
	DETOURED_FUNCTION(CreateEventW) \
	DETOURED_FUNCTION(CreateEventExW) \
	DETOURED_FUNCTION(CreateMutexExW) \
	DETOURED_FUNCTION(CreateWaitableTimerExW) \
	DETOURED_FUNCTION(CreateIoCompletionPort) \
	DETOURED_FUNCTION(CreatePipe) \
	DETOURED_FUNCTION(CreateNamedPipeW) \
	DETOURED_FUNCTION(PeekNamedPipe) \
	DETOURED_FUNCTION(GetKernelObjectSecurity) \
	DETOURED_FUNCTION(ImpersonateNamedPipeClient) \
	DETOURED_FUNCTION(TransactNamedPipe) \
	DETOURED_FUNCTION(SetNamedPipeHandleState) \
	DETOURED_FUNCTION(GetNamedPipeInfo) \
	DETOURED_FUNCTION(GetNamedPipeHandleStateW) \
	DETOURED_FUNCTION(GetNamedPipeServerProcessId) \
	DETOURED_FUNCTION(GetNamedPipeServerSessionId) \
	DETOURED_FUNCTION(DecryptFileW) \
	DETOURED_FUNCTION(DecryptFileA) \
	DETOURED_FUNCTION(EncryptFileW) \
	DETOURED_FUNCTION(EncryptFileA) \
	DETOURED_FUNCTION(OpenEncryptedFileRawW) \
	DETOURED_FUNCTION(OpenEncryptedFileRawA) \
	DETOURED_FUNCTION(OpenFileById) \
	DETOURED_FUNCTION(OpenFileMappingA) \
	DETOURED_FUNCTION(GetMappedFileNameW) \
	DETOURED_FUNCTION(IsProcessorFeaturePresent) \
	//DETOURED_FUNCTION(VirtualFree) \
	//DETOURED_FUNCTION(BaseThreadInitThunk) \
	//DETOURED_FUNCTION(VirtualAllocEx) \
	//DETOURED_FUNCTION(CryptCreateHash) \
	//DETOURED_FUNCTION(CryptHashData) \
	//DETOURED_FUNCTION(CreateFileMapping2) \
	//DETOURED_FUNCTION(CreateFileMappingNumaW) \

#define DETOURED_FUNCTIONS_KERNEL32_DEBUG \

#define DETOURED_FUNCTIONS_NTDLL_DEBUG \
	DETOURED_FUNCTION(RtlAllocateHeap) \
	DETOURED_FUNCTION(RtlReAllocateHeap) \
	DETOURED_FUNCTION(RtlValidateHeap) \
	DETOURED_FUNCTION(NtCreateIoCompletion) \
	DETOURED_FUNCTION(NtQueryFullAttributesFile) \
	DETOURED_FUNCTION(NtFlushBuffersFileEx) \
	DETOURED_FUNCTION(NtReadFile) \
	DETOURED_FUNCTION(ZwSetInformationFile) \
	DETOURED_FUNCTION(ZwQueryDirectoryFile) \
	//DETOURED_FUNCTION(ZwCreateFile) \
	//DETOURED_FUNCTION(ZwOpenFile) \
	//DETOURED_FUNCTION(RtlAllocateHeap) \

#define DETOURED_FUNCTIONS_SHLWAPI_DEBUG \
	DETOURED_FUNCTION(PathFindFileNameW) \
	DETOURED_FUNCTION(PathIsRelativeW) \
	DETOURED_FUNCTION(PathIsDirectoryEmptyW) \
	DETOURED_FUNCTION(SHCreateStreamOnFileW) \
	DETOURED_FUNCTION(PathFileExistsW) \


#define DETOURED_FUNCTIONS_UCRTBASE_DEBUG \
	DETOURED_FUNCTION(_wcsnicoll_l) \
	DETOURED_FUNCTION(_wgetenv) \
	DETOURED_FUNCTION(_wgetenv_s) \
	DETOURED_FUNCTION(getenv) \
	DETOURED_FUNCTION(getenv_s) \
	DETOURED_FUNCTION(_wmakepath_s) \
	//DETOURED_FUNCTION(_wsopen_s) \
	//DETOURED_FUNCTION(_fileno) \

#if UBA_USE_MIMALLOC
#define DETOURED_FUNCTIONS_MEMORY_DEBUG \
	DETOURED_FUNCTION(_aligned_msize) \
	//DETOURED_FUNCTION(_free_dbg) \

#endif

#else
#define DETOURED_FUNCTIONS_KERNELBASE_DEBUG
#define DETOURED_FUNCTIONS_KERNEL32_DEBUG
#define DETOURED_FUNCTIONS_NTDLL_DEBUG
#define DETOURED_FUNCTIONS_SHLWAPI_DEBUG
#define DETOURED_FUNCTIONS_UCRTBASE_DEBUG
#define DETOURED_FUNCTIONS_MEMORY_DEBUG
#endif

extern "C" {
	enum FS_INFORMATION_CLASS {};
	NTSTATUS NTAPI NtQueryVolumeInformationFile(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock, PVOID FsInformation, ULONG Length, FS_INFORMATION_CLASS FsInformationClass);
	NTSTATUS NTAPI NtQueryFullAttributesFile(POBJECT_ATTRIBUTES ObjectAttributes, PVOID Attributes);
	NTSTATUS NTAPI NtQueryInformationFile(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass);
	NTSTATUS NTAPI NtQueryDirectoryFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry, PUNICODE_STRING FileName, BOOLEAN RestartScan);
	NTSTATUS NTAPI NtFsControlFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, ULONG FsControlCode, PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength);
	NTSTATUS NTAPI NtCopyFileChunk(HANDLE Source, HANDLE Dest, HANDLE Event, PIO_STATUS_BLOCK IoStatusBlock, ULONG Length, PULONG SourceOffset, PULONG DestOffset, PULONG SourceKey, PULONG DestKey, ULONG Flags);
	NTSTATUS NTAPI NtFlushBuffersFileEx(HANDLE FileHandle, ULONG Flags, PVOID Parameters, ULONG ParametersSize, PIO_STATUS_BLOCK IoStatusBlock);
	NTSTATUS NTAPI NtReadFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key);
	NTSTATUS NTAPI NtSetInformationFile(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass);
	NTSTATUS NTAPI NtCreateSection(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PLARGE_INTEGER MaximumSize, ULONG SectionPageProtection, ULONG AllocationAttributes, HANDLE FileHandle);
	NTSTATUS NTAPI NtCreateIoCompletion(PHANDLE IoCompletionHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, ULONG Count);
	NTSTATUS NTAPI ZwCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock, PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength);
	NTSTATUS NTAPI ZwOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock, ULONG ShareAccess, ULONG OpenOptions);
	NTSTATUS NTAPI ZwQueryDirectoryFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry, PUNICODE_STRING FileName, BOOLEAN RestartScan);
	NTSTATUS NTAPI ZwSetInformationFile(HANDLE FileHandle,PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass);
	PVOID WINAPI ResolveDelayLoadedAPI(PVOID ParentModuleBase, PCIMAGE_DELAYLOAD_DESCRIPTOR DelayloadDescriptor, void* FailureDllHook, void* FailureSystemHook, PIMAGE_THUNK_DATA ThunkAddress, ULONG Flags);
	void WINAPI RtlExitUserThread(ULONG);
	BOOLEAN NTAPI RtlFreeHeap(PVOID HeapHandle, ULONG Flags, PVOID HeapBase);
	BOOLEAN NTAPI RtlValidateHeap(HANDLE HeapPtr, ULONG Flags, PVOID Block);
	PVOID NTAPI RtlAllocateHeap( PVOID HeapHandle, ULONG Flags, SIZE_T Size);
	PVOID NTAPI RtlReAllocateHeap(PVOID HeapHandle, ULONG Flags, PVOID BaseAddress, SIZE_T Size);
	SIZE_T NTAPI RtlSizeHeap(HANDLE HeapPtr, ULONG Flags, PVOID Ptr);
	void NTAPI BaseThreadInitThunk(ULONG Unknown,LPTHREAD_START_ROUTINE StartAddress,PVOID ThreadParameter);
	void* _expand_base(void* memblock, size_t size);
}


#define DETOURED_FUNCTION(Func) extern decltype(Func)* True_##Func;
DETOURED_FUNCTIONS
#undef DETOURED_FUNCTION


struct FILE_DIRECTORY_INFORMATION {
  ULONG         NextEntryOffset;
  ULONG         FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  ULONG         FileAttributes;
  ULONG         FileNameLength;
  WCHAR         FileName[1];
};

struct FILE_FULL_DIR_INFORMATION {
	ULONG         NextEntryOffset;
	ULONG         FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	ULONG         FileAttributes;
	ULONG         FileNameLength;
	ULONG         EaSize;
	WCHAR         FileName[1];
};

struct FILE_RENAME_INFORMATION {
    union {
        BOOLEAN ReplaceIfExists;  // FileRenameInformation
        ULONG Flags;              // FileRenameInformationEx
    } DUMMYUNIONNAME;
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
};

struct FILE_IS_REMOTE_DEVICE_INFORMATION {
  BOOLEAN IsRemote;
};

struct FILE_ID_INFORMATION {
  ULONGLONG   VolumeSerialNumber;
  FILE_ID_128 FileId;
};

struct FILE_NAME_INFORMATION {
  ULONG FileNameLength;
  WCHAR FileName[1];
};

namespace uba
{
	struct DetoursPayload;

	void PreInit(const DetoursPayload& payload);
	void Init(const DetoursPayload& payload, u64 startTime);
	void Deinit(u64 startTime);
	void PostDeinit();
}
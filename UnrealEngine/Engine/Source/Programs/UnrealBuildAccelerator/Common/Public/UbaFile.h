// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaMemory.h"
#include "UbaPlatform.h"

namespace uba
{
	class Logger;

	#if !PLATFORM_WINDOWS
	inline constexpr u32 ERROR_FILE_NOT_FOUND = ENOENT;
	inline constexpr u32 ERROR_PATH_NOT_FOUND = ENOENT;
	inline constexpr u32 ERROR_ALREADY_EXISTS = EEXIST;
	inline constexpr u32 ERROR_ACCESS_DENIED = EACCES;
	inline constexpr u32 MOVEFILE_REPLACE_EXISTING = 0x00000001;
	inline constexpr u32 FILE_FLAG_NO_BUFFERING = 0;
	inline constexpr u32 FILE_FLAG_OVERLAPPED = 0;
	inline constexpr u32 INVALID_FILE_ATTRIBUTES = (u32)-1;
	inline constexpr u32 CREATE_ALWAYS = 2;
	inline constexpr u32 GENERIC_WRITE = 0x40000000L;
	inline constexpr u32 DELETE = 0x00010000L;
	inline constexpr u32 GENERIC_READ = 0x80000000L;
	inline constexpr u32 FILE_SHARE_WRITE = 0x00000002;
	inline constexpr u32 FILE_SHARE_READ = 0x00000001;
	inline constexpr u32 FILE_FLAG_BACKUP_SEMANTICS = 0x02000000;
	inline constexpr u32 OPEN_EXISTING = 3;
	inline constexpr u32 PAGE_READONLY = 0x02;
	#endif

	inline constexpr u64 FileHandleFlagMask = 0x0000'0000'ffff'ffff;

	#if PLATFORM_WINDOWS
	inline constexpr u64 OverlappedIoFlag = 0x0000'0001'0000'0000; // Only used by windows
	#endif

	struct FileInformation
	{
		u32 attributes;
		u32 volumeSerialNumber;
		u64 lastWriteTime;
		u64 size;
		u64 index;
	};
	bool GetFileInformationByHandle(FileInformation& out, Logger& logger, const tchar* fileName, FileHandle hFile);
	bool GetFileInformation(FileInformation& out, Logger& logger, const tchar* fileName);

	bool ReadFile(Logger& logger, const tchar* fileName, FileHandle fileHandle, void* b, u64 bufferLen);
	bool OpenFileSequentialRead(Logger& logger, const tchar* fileName, FileHandle& outHandle, bool fileNotFoundIsError = true, bool overlapped = false);
	bool FileExists(Logger& logger, const tchar* fileName, u64* outSize = nullptr, u32* outAttributes = nullptr);
	bool SetEndOfFile(Logger& logger, const tchar* fileName, FileHandle handle, u64 size);
	bool GetDirectoryOfCurrentModule(Logger& logger, StringBufferBase& out);
	bool DeleteAllFiles(Logger& logger, const tchar* dir, bool deleteDir = true, u32* count = nullptr);
	bool SearchPathForFile(Logger& logger, StringBufferBase& out, const tchar* file, const tchar* applicationDir);

	//
	FileHandle CreateFileW(const tchar* fileName, u32 desiredAccess, u32 shareMode, u32 createDisp, u32 flagsAndAttributes);
	bool CloseFile(const tchar* fileName, FileHandle h);
	bool CreateDirectoryW(const tchar* pathName);
	bool RemoveDirectoryW(const tchar* pathName);
	bool DeleteFileW(const tchar* fileName);
	bool CopyFileW(const tchar* existingFileName, const tchar* newFileName, bool bFailIfExists);
	u32 GetLongPathNameW(const tchar* lpszShortPath, tchar* lpszLongPath, u32 cchBuffer);
	bool GetFileLastWriteTime(u64& outTime, FileHandle hFile);
	bool SetFileLastWriteTime(FileHandle fileHandle, u64 writeTime);
	bool MoveFileExW(const tchar* existingFileName, const tchar* newFileName, u32 dwFlags);
	bool GetFileSizeEx(u64& outFileSize, FileHandle hFile);

	u32 GetFileAttributesW(const tchar* fileName);
	bool IsDirectory(u32 attributes);
	bool IsReadOnly(u32 attributes);
	u32 DefaultAttributes(bool execute = false);
	bool CreateHardLinkW(const tchar* newFileName, const tchar* existingFileName);
	u32 GetFullPathNameW(const tchar* fileName, u32 nBufferLength, tchar* lpBuffer, tchar** lpFilePart);
	bool SearchPathW(const tchar* a, const tchar* b, const tchar* c, u32 d, tchar* e, tchar** f);
	u64 GetSystemTimeAsFileTime();
	u64 GetFileTimeAsSeconds(u64 fileTime);
	bool GetCurrentDirectoryW(StringBufferBase& out);


	class DirectoryCache
	{
	public:
		bool CreateDirectory(Logger& logger, const tchar* dir);
		void Clear();

	private:
		ReaderWriterLock m_createdDirsLock;
		struct CreatedDir { ReaderWriterLock lock; bool handled = false; };
		UnorderedMap<TString, CreatedDir> m_createdDirs;
	};
}

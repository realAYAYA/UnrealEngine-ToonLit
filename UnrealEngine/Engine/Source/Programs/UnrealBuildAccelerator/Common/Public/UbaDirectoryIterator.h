// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaFile.h"
#include "UbaLogger.h"
#include "UbaStringBuffer.h"

namespace uba
{
	struct DirectoryEntry
	{
		const tchar* name;
		u32 nameLen;
		u64 lastWritten;
		u32 attributes;
		u32 volumeSerial;
		u64 id;
		u64 size;
	};

	struct DirectoryInfo
	{
		u32 attributes;
		u32 volumeSerial;
		u64 id;
	};

	using IteratorFunc = Function<void(const DirectoryEntry&)>;
	using DirectoryInfoFunc = Function<void(const DirectoryInfo&)>;
}



#if PLATFORM_WINDOWS
#include <winternl.h>

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
extern "C" NTSTATUS NtQueryDirectoryFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry, PUNICODE_STRING FileName, BOOLEAN RestartScan);

constexpr int FileIdBothDirectoryInformation = 37;
struct FILE_ID_BOTH_DIR_INFORMATION {
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
  CCHAR         ShortNameLength;
  WCHAR         ShortName[12];
  LARGE_INTEGER FileId;
  WCHAR         FileName[1];
};

namespace uba
{
	inline bool TraverseDir(Logger& logger, const tchar* dirPath, const IteratorFunc& iteratorFunc, bool errorOnNotFound = false, const DirectoryInfoFunc& infoFunc = {})
	{
		StringBuffer<> str;
		str.Append(L"\\??\\").Append(dirPath);

		if (dirPath[1] == ':' && dirPath[2] == 0) // if we are all the way to the root of the drive we add a slash since NtCreateFile needs that
			str.Append(L"\\");

		UNICODE_STRING uniName;
		RtlInitUnicodeString(&uniName, str.data);

		HANDLE handle;

		OBJECT_ATTRIBUTES ObjectAttributes;
		IO_STATUS_BLOCK IoStatusBlock;
		memset(&IoStatusBlock, 0, sizeof(IoStatusBlock));
		InitializeObjectAttributes(&ObjectAttributes, &uniName, OBJ_CASE_INSENSITIVE, NULL, NULL);
		ULONG ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		NTSTATUS res = NtCreateFile(&handle, FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_ATTRIBUTE_UNPINNED, &ObjectAttributes, &IoStatusBlock, 0, FILE_ATTRIBUTE_NORMAL, ShareAccess, FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT, NULL, 0);
		if (res == STATUS_OBJECT_NAME_NOT_FOUND || res == STATUS_OBJECT_PATH_NOT_FOUND)
			return !errorOnNotFound;
		if (res == STATUS_NOT_A_DIRECTORY || res == STATUS_ACCESS_DENIED || res == STATUS_NO_MEDIA_IN_DEVICE)
			return false;
		if (res != STATUS_SUCCESS)
			return logger.Error(L"NtCreateFile for TraverseDir on %s failed with error code %x", dirPath, (DWORD)res);
		auto hg = MakeGuard([&]() { NtClose(handle); });

		bool exists = IoStatusBlock.Information == 1;
		if (!exists)
			return !errorOnNotFound;

		BY_HANDLE_FILE_INFORMATION dirInfo;
		if (!GetFileInformationByHandle(handle, &dirInfo))
		{
			logger.Warning(L"GetFileInformationByHandle returned error %s", LastErrorToText().data);
			return false;
		}

		u32 volumeSerial = dirInfo.dwVolumeSerialNumber;

		if (infoFunc)
		{
			DirectoryInfo di;
			di.attributes = dirInfo.dwFileAttributes;
			di.volumeSerial = volumeSerial;
			di.id = ToLargeInteger(dirInfo.nFileIndexHigh, dirInfo.nFileIndexLow).QuadPart;
			UBA_ASSERT(di.id);
			infoFunc(di);
		}

		u8 buff[64*1024];
		while (true)
		{
			res = NtQueryDirectoryFile(handle, 0, NULL, NULL, &IoStatusBlock, buff, sizeof(buff) - 2, (FILE_INFORMATION_CLASS)FileIdBothDirectoryInformation, FALSE, NULL, FALSE);
			if (res != STATUS_SUCCESS)
			{
				if (res != STATUS_NO_MORE_FILES)
					logger.Error(L"NtQueryDirectoryFile returned error 0x%x", res);
				break;
			}

			u8* it = buff;
			while (true)
			{
				auto& FileInformation = *(FILE_ID_BOTH_DIR_INFORMATION*)it;

				auto fileName = FileInformation.FileName;
				u64 fileNameCharCount = FileInformation.FileNameLength/sizeof(tchar);
				wchar_t& fileNameEnd = fileName[fileNameCharCount];
				wchar_t old = fileNameEnd;
				fileNameEnd = 0;
				if (fileNameCharCount == 1 && fileName[0] == '.' || (fileNameCharCount == 2 && fileName[0] == '.' && fileName[1] == '.'))
				{
					if (!FileInformation.NextEntryOffset)
						break;
					fileNameEnd = old;
					it += FileInformation.NextEntryOffset;
					continue;
				}

				DirectoryEntry entry;
				entry.name = fileName;
				entry.nameLen = u32(fileNameCharCount);
				entry.lastWritten = FileInformation.LastWriteTime.QuadPart;
				entry.attributes = FileInformation.FileAttributes;
				entry.volumeSerial = volumeSerial;
				entry.id = FileInformation.FileId.QuadPart;
				entry.size = FileInformation.EndOfFile.QuadPart;
				iteratorFunc(entry);

				if (!FileInformation.NextEntryOffset)
					break;
				fileNameEnd = old;
				it += FileInformation.NextEntryOffset;
			}
		}
		return true;
	}
}

#else

#include <dirent.h>

namespace uba
{
	inline bool TraverseDir(Logger& logger, const tchar* dirPath, const IteratorFunc& iteratorFunc, bool errorOnNotFound = false, const DirectoryInfoFunc& infoFunc = {})
	{
		char dirPath2[1024];
		strcpy(dirPath2, dirPath);
		u32 dirPathLen = strlen(dirPath2);

		DIR* dir = opendir(dirPath2);
		if (!dir)
		{
			if (errno == ENOENT || errno == ENOTDIR)
				return !errorOnNotFound;
			UBA_ASSERTF(false, "TraverseDir error handling not implemented %s (%s)", dirPath, strerror(errno));
			return false;
		}

		auto dg = MakeGuard([&]() { closedir(dir); });

		if (infoFunc)
		{
			int fd = dirfd(dir);
			if (fd == -1)
			{
				UBA_ASSERTF(false, "TraverseDir:dirfd error handling not implemented");
				return false;
			}
			struct stat dirAttr;
			int res = fstat(fd, &dirAttr);
			if (res == -1)
			{
				UBA_ASSERTF(false, "TraverseDir:fstat error handling not implemented");
				return false;
			}
			DirectoryInfo di;
			di.attributes = dirAttr.st_mode;
			di.volumeSerial = dirAttr.st_dev;
			di.id = dirAttr.st_ino;

			infoFunc(di);
		}


		if (dirPath2[dirPathLen - 1] != '/')
			dirPath2[dirPathLen++] = '/';

		struct dirent* pDirent;
		while ((pDirent = readdir(dir)) != NULL)
		{
			char* fileName = pDirent->d_name;
			if (fileName[0] == '.' && (fileName[1] == 0 || (fileName[1] == '.' && fileName[2] == 0))) 
				continue;

			strcpy(dirPath2 + dirPathLen, fileName);

			struct stat attr;
			int res = stat(dirPath2, &attr);
			if (res == -1)
			{
				// In the case of symlinks we may need to try lstat
				// just in case.
				if (errno == ENOENT) {
					res = lstat(dirPath2, &attr);
				}

				if (errno == ENODEV || errno == EACCES)
					continue;
				UBA_ASSERTF(res == 0, "TraverseDir:stat error handling not added for file %s (error %s)", dirPath2, strerror(errno));
			}

			DirectoryEntry entry;
			entry.name = fileName;
			entry.nameLen = strlen(fileName);
			entry.lastWritten = FromTimeSpec(attr.st_mtimespec);
			entry.attributes = attr.st_mode;
			entry.volumeSerial = attr.st_dev;
			entry.id = attr.st_ino;
			entry.size = attr.st_size;
			iteratorFunc(entry);
		}

		return true;
	}
}
#endif

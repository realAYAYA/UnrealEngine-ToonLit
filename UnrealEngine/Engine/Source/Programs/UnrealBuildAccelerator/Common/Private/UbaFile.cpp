// Copyright Epic Games, Inc. All Rights Reserved.

#define Local_GetLongPathNameW uba::GetLongPathNameW

#include "UbaFile.h"
#include "UbaDirectoryIterator.h"
#include "UbaEvent.h"
#include "UbaPathUtils.h"
#include "UbaProcessStats.h"

#if PLATFORM_WINDOWS
#include <io.h>
#include <psapi.h>
#include <WinCon.h>
#include <RestartManager.h>
#pragma comment(lib, "Rstrtmgr.lib")
#else
#include <dlfcn.h>
#include <execinfo.h>
#include <sys/time.h>
#endif

#if PLATFORM_MAC
#include <mach-o/dyld.h>
#endif

namespace uba
{
#if PLATFORM_WINDOWS
	void GetProcessHoldingFile(StringBufferBase& out, const tchar* fileName)
	{
		DWORD dwSession;
		WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };
		DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);
		if (dwError != ERROR_SUCCESS)
			return;
		auto sg = MakeGuard([dwSession]() { RmEndSession(dwSession); });
		dwError = RmRegisterResources(dwSession, 1, &fileName, 0, NULL, 0, NULL);
		if (dwError != ERROR_SUCCESS)
			return;

		DWORD dwReason;
		UINT nProcInfoNeeded;
		UINT nProcInfo = 10;
		RM_PROCESS_INFO rgpi[10] = {};
		dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rgpi, &dwReason);
		if (dwError != ERROR_SUCCESS)
			return;

		for (u32 i = 0; i < nProcInfo; i++)
		{
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, rgpi[i].Process.dwProcessId);
			if (!hProcess)
				continue;
			auto pg = MakeGuard([hProcess]() { CloseHandle(hProcess); });

			FILETIME ftCreate, ftExit, ftKernel, ftUser;
			if (!GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser))
				continue;
			if (CompareFileTime(&rgpi[i].Process.ProcessStartTime, &ftCreate) != 0)
				continue;
			WCHAR sz[MaxPath];
			DWORD cch = MaxPath;
			if (!QueryFullProcessImageNameW(hProcess, 0, sz, &cch))
				continue;
			if (cch <= MaxPath)
				out.Appendf(TC(" - %s"), sz); // Using Appendf to have capacity check
		}
	}

	HANDLE asHANDLE(FileHandle fh)
	{
		return (HANDLE)(fh == InvalidFileHandle ? InvalidFileHandle : (fh & FileHandleFlagMask));
	}
#else
	int asFileDescriptor(FileHandle fh)
	{
		if (fh == InvalidFileHandle)
			return (int)fh;
		return (int)(fh & FileHandleFlagMask);
	}
#endif


	bool ReadFile(Logger& logger, const tchar* fileName, FileHandle fileHandle, void* b, u64 bufferLen)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().readFile);
		u8* buffer = (u8*)b;
		u64 readLeft = bufferLen;
		u64 firstZeroReadTime = 0;

		while (readLeft)
		{
			u32 toRead = u32(Min(readLeft, u64(~u32(0)) - 1));
#if PLATFORM_WINDOWS
			DWORD wasRead = 0;
			if (!::ReadFile(asHANDLE(fileHandle), buffer, toRead, &wasRead, NULL))
				if (GetLastError() != ERROR_IO_PENDING)
					return logger.Error(TC("ERROR reading file %s (error: %s)"), fileName, LastErrorToText().data);
#else
			ssize_t wasRead = read(asFileDescriptor(fileHandle), buffer, toRead);
			if (wasRead == -1)
			{
				UBA_ASSERTF(false, TC("ERROR ReadFile error handling not implemented (Trying to read %u bytes from fd %llu)"), toRead, fileHandle);
				return false;
			}
#endif
			if (wasRead == 0)
			{
				if (firstZeroReadTime == 0)
					firstZeroReadTime = GetTime();
				else if (TimeToMs(GetTime() - firstZeroReadTime) > 3*1000)
					return logger.Error(TC("ERROR reading file %s trying to read %u bytes from offset %llu but ReadFile returns 0 bytes read.. Is the file big enough?"), fileName, toRead, bufferLen - readLeft);
			}

			readLeft -= wasRead;
			buffer += wasRead;
		}
		return true;
	}

	bool OpenFileSequentialRead(Logger& logger, const tchar* fileName, FileHandle& outHandle, bool fileNotFoundIsError, bool overlapped)
	{
		u32 dwFlagsAndAttributes = DefaultAttributes();
		#if PLATFORM_WINDOWS
		dwFlagsAndAttributes |= (overlapped ? FILE_FLAG_OVERLAPPED : FILE_FLAG_SEQUENTIAL_SCAN);
		#endif

		outHandle = uba::CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, OPEN_EXISTING, dwFlagsAndAttributes);
		if (outHandle != InvalidFileHandle)
			return true;
		u32 lastError = GetLastError();
		if (lastError == ERROR_FILE_NOT_FOUND || lastError == ERROR_PATH_NOT_FOUND)
			return !fileNotFoundIsError;
		if (lastError == ERROR_ACCESS_DENIED)
		{
			// Remotes can ask for files via storage (SessionServer::HandleMessage->StoreCasFile->StorageImpl::StoreCasFile->this) and sometimes they exist as folders (but should be treated as not existing)
			u32 attr = GetFileAttributesW(fileName);
			if (attr != INVALID_FILE_ATTRIBUTES)
				if (IsDirectory(attr))
					return false;
		}
		StringBuffer<4096> additionalInfo;
		#if PLATFORM_WINDOWS
		if (lastError == ERROR_SHARING_VIOLATION)
			GetProcessHoldingFile(additionalInfo, fileName);
		#endif
		return logger.Error(TC("ERROR opening file %s for read (%s%s)"), fileName, LastErrorToText(lastError).data, additionalInfo.data);
	}

	bool GetFileInformationByHandle(FileInformation& out, Logger& logger, const tchar* fileName, FileHandle hFile)
	{
#if PLATFORM_WINDOWS
		BY_HANDLE_FILE_INFORMATION info;
		if (!::GetFileInformationByHandle(asHANDLE(hFile), &info))
			return logger.Error(TC("GetFileInformationByHandle failed on %s (%s)"), fileName, LastErrorToText().data);
		out.attributes = info.dwFileAttributes;
		out.volumeSerialNumber = info.dwVolumeSerialNumber;
		out.lastWriteTime = (u64&)info.ftLastWriteTime;
		out.size = u64(ToLargeInteger(info.nFileSizeHigh, info.nFileSizeLow).QuadPart);
		out.index = u64(ToLargeInteger(info.nFileIndexHigh, info.nFileIndexLow).QuadPart);
		return true;
#else
		struct stat attr;
		int res = fstat(asFileDescriptor(hFile), &attr);
		if (res != 0)
		{
			UBA_ASSERTF(false, TC("GetFileInformationByHandle (fstat) error handling not implemented"));
			return false;
		}
		out.lastWriteTime = FromTimeSpec(attr.st_mtimespec);
		out.attributes = attr.st_mode;
		out.volumeSerialNumber = attr.st_dev;
		out.index = attr.st_ino;
		out.size = attr.st_size;
		return true;
#endif
	}

	bool GetFileInformation(FileInformation& out, Logger& logger, const tchar* fileName)
	{
#if PLATFORM_WINDOWS
		FileHandle h = uba::CreateFileW(fileName, 0, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS);
		if (h == InvalidFileHandle)
			return false;// logger.Error(TC("GetFileInformation: CreateFile failed for file %s (%s)"), fileName, LastErrorToText().data);
		auto handleGuard = MakeGuard([&]() { uba::CloseFile(fileName, h); });
		if (!GetFileInformationByHandle(out, logger, fileName, h))
			return false;// logger.Error(TC("Failed to get file information for %s while checking file added for write. This should not happen! (%s)"), fileName, LastErrorToText().data);
		return true;
#else
		struct stat attr;
		int res = stat(fileName, &attr);
		if (res != 0)
		{
			UBA_ASSERTF(errno == ENOENT, TC("GetFileInformation: stat failed for file %s and this error handling not implemented (%s)"), fileName, strerror(errno));
			SetLastError(ERROR_FILE_NOT_FOUND);
			return false;
		}
		out.lastWriteTime = FromTimeSpec(attr.st_mtimespec);
		out.attributes = attr.st_mode;
		out.volumeSerialNumber = attr.st_dev;
		out.index = attr.st_ino;
		out.size = attr.st_size;
		return true;
#endif
	}

	bool FileExists(Logger& logger, const tchar* fileName, u64* outSize, u32* outAttributes)
	{
#if PLATFORM_WINDOWS
		WIN32_FILE_ATTRIBUTE_DATA data;
		if (!::GetFileAttributesExW(fileName, GetFileExInfoStandard, &data))
		{
			DWORD lastError = GetLastError();
			if (lastError != ERROR_FILE_NOT_FOUND && lastError != ERROR_PATH_NOT_FOUND)
				logger.Error(TC("GetFileAttributesW failed on %s (%s)"), fileName, LastErrorToText(lastError).data);
			return false;
		}

		if (outSize)
		{
			LARGE_INTEGER li;
			li.HighPart = (LONG)data.nFileSizeHigh;
			li.LowPart = data.nFileSizeLow;
			*outSize = u64(li.QuadPart);
		}
		if (outAttributes)
			*outAttributes = data.dwFileAttributes;
		return true;
#else
		struct stat attr;
		if (stat(fileName, &attr) == -1)
		{
			if (errno == ENOENT)
				return false;
			UBA_ASSERTF(false, TC("FileExists error handling implemented"));
			return false;
		}

		if (outSize)
			*outSize = attr.st_size;
		if (outAttributes)
			*outAttributes = attr.st_mode;
		return true;
#endif
	}

	bool SetEndOfFile(Logger& logger, const tchar* fileName, FileHandle handle, u64 size)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().setFileInfo);
#if PLATFORM_WINDOWS
		FILE_END_OF_FILE_INFO info;
		info.EndOfFile = ToLargeInteger(size);
		if (!::SetFileInformationByHandle(asHANDLE(handle), FileEndOfFileInfo, &info, sizeof(info)))
			return logger.Error(TC("SetFileInformationByHandle failed on %s (%s)"), fileName, LastErrorToText().data);
		return true;
#else
		UBA_ASSERTF(false, TC("SetEndOfFile not implemented"));
		return false;
#endif
	}

	bool GetDirectoryOfCurrentModule(Logger& logger, StringBufferBase& out)
	{
		#if PLATFORM_WINDOWS
			HMODULE hm = NULL;
			if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&GetDirectoryOfCurrentModule, &hm))
				return logger.Error(TC("GetModuleHandleEx failed (%s)"), LastErrorToText().data);
			if (!GetModuleFileNameW(hm, out.data, out.capacity))
				return logger.Error(TC("GetModuleFileNameW failed (%s)"), LastErrorToText().data);
			const tchar* lastSlash = out.Last('\\');
			out.Resize(lastSlash - out.data);
			return true;

		#else
			// Can be a shared library, not only executable.. so can't use /proc/self/exe
			Dl_info info;
			if (!dladdr((void*)&GetDirectoryOfCurrentModule, &info))
				return logger.Error("dladdr failed to get info for address to GetDirectoryOfCurrentModule");
			out.count = GetFullPathNameW(info.dli_fname, out.capacity, out.data, nullptr);
			if (!out.count)
				return logger.Error("GetFullPathNameW failed to return full name for %s", info.dli_fname);
			char* lastSlash = strrchr(out.data, '/');
			out.Resize(lastSlash - out.data);
			return true;
		#endif
	}

	bool DeleteAllFiles(Logger& logger, const tchar* dir, bool deleteDir, u32* count)
	{
		bool success = true;
		bool traverseRes = TraverseDir(logger, dir,
			[&](const DirectoryEntry& e)
			{
				StringBuffer<> fullPath(dir);
				fullPath.EnsureEndsWithSlash().Append(e.name);

				if (IsReadOnly(e.attributes))
				{
					#if PLATFORM_WINDOWS
					SetFileAttributesW(fullPath.data, e.attributes & ~FILE_ATTRIBUTE_READONLY);
					#else
					UBA_ASSERT(false);
					#endif
				}

				if (IsDirectory(e.attributes))
				{
					if (!DeleteAllFiles(logger, fullPath.data, true, count))
						success = false;
				}
				else
				{
					if (!DeleteFileW(fullPath.data))
					{
						logger.Warning(TC("Failed to delete file %s (%s)"), fullPath.data, LastErrorToText().data);
						success = false;
					}
					else if (count)
						++(*count);
				}
			});

		if (!traverseRes || !success)
			return false;

		if (!deleteDir)
			return true;

		if (RemoveDirectoryW(dir))
			return true;

		u32 lastError = GetLastError();

		if (lastError == ERROR_FILE_NOT_FOUND || lastError == ERROR_PATH_NOT_FOUND)
			return true;

		logger.Warning(TC("Failed to delete directory %s (%s)"), dir, LastErrorToText(lastError).data);
		return false;
	}

	bool SearchPathForFile(Logger& logger, StringBufferBase& out, const tchar* file, const tchar* applicationDir)
	{
		StringBuffer<> fullPath;
		fullPath.Append(applicationDir);
		fullPath.EnsureEndsWithSlash();
		fullPath.Append(file);
		if (GetFileAttributesW(fullPath.data) != INVALID_FILE_ATTRIBUTES)
		{
			FixPath(fullPath.data, nullptr, 0, out);
			return true;
		}

		char varSeparator = IsWindows ? ';' : ':';
		tchar buff[32 * 1024] = { 0 };
		u32 len = GetEnvironmentVariableW(TC("PATH"), buff, sizeof_array(buff));
		if (!len || len == sizeof_array(buff))
			return logger.Error(TC("Failed to get PATH environment variable"));
		if (len >= sizeof_array(buff))
			return logger.Error(TC("Failed to get PATH variable, buffer too small (need %u)"), len);

		tchar* lastStart = buff;
		tchar* it = buff;
		bool end = false;
		while (!end)
		{
			end = *it == 0;
			if (*it != varSeparator && !end)
			{
				++it;
				continue;
			}

			*it = 0;

			fullPath.Resize(0);
			fullPath.Append(lastStart);
			if (*lastStart)
				fullPath.EnsureEndsWithSlash();
			fullPath.Append(file);
			if (GetFileAttributesW(fullPath.data) != INVALID_FILE_ATTRIBUTES)
			{
				FixPath(fullPath.data, nullptr, 0, out);
				return true;
			}

			lastStart = ++it;
		}
		return false;
	}

	FileHandle CreateFileW(const tchar* fileName, u32 desiredAccess, u32 shareMode, u32 createDisp, u32 flagsAndAttributes)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().createFile);
	#if PLATFORM_WINDOWS
		return (FileHandle)(u64)::CreateFileW(fileName, desiredAccess, shareMode, NULL, createDisp, flagsAndAttributes, NULL);
	#else
		int flags = O_CLOEXEC;
		if (createDisp == CREATE_ALWAYS)
			flags |= O_CREAT | O_TRUNC;
		else if (createDisp == OPEN_EXISTING)
			flags = O_NONBLOCK;
		else
			UBA_ASSERTF(false, TC("CreateFileW create disposition %u not supported"), createDisp);

		if ((desiredAccess & (GENERIC_WRITE | GENERIC_READ)) == (GENERIC_WRITE | GENERIC_READ))
			flags |= O_RDWR;
		else if (desiredAccess & GENERIC_WRITE)
			flags |= O_WRONLY;
		else if (desiredAccess & GENERIC_READ)
			flags |= O_RDONLY;
		else if (!desiredAccess)
			flags = O_RDONLY;
		else
			UBA_ASSERTF(false, TC("CreateFileW desired access %u not supported"), desiredAccess);
		int mode = flagsAndAttributes;
		int fd = open(fileName, flags, mode);
		if (fd != -1)
		{
			struct stat attr;
			int res = fstat(fd, &attr);
			if (res != 0)
			{
				UBA_ASSERTF(false, TC("CreateFileW (fstat) error handling for %u (%s) not implemented"), errno, strerror(errno));
			}
			if (S_ISREG(attr.st_mode))
				return (FileHandle)fd;
			close(fd);
			SetLastError(ERROR_ACCESS_DENIED);
			return InvalidFileHandle;
		}

		if (errno == ENOENT)
		{
			SetLastError(ERROR_FILE_NOT_FOUND);
			return InvalidFileHandle;
		}

		if (errno == EACCES)
		{
			SetLastError(ERROR_ACCESS_DENIED);
			return InvalidFileHandle;
		}

		UBA_ASSERTF(false, TC("CreateFileW failed for %s - Error handling for %u (%s) not implemented"), fileName, errno, strerror(errno));
		return InvalidFileHandle;
	#endif
	}

	bool CloseFile(const tchar* fileName, FileHandle h)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().closeFile);
#if PLATFORM_WINDOWS
		return ::CloseHandle(asHANDLE(h));
#else
		if (h == InvalidFileHandle)
			return true;

		if (close(asFileDescriptor(h)) == 0)
			return true;
		UBA_ASSERTF(false, TC("CloseFile error handling not implemented while failing to close %s (%s)"), fileName, strerror(errno));
		return false;
#endif
	}

	bool CreateDirectoryW(const tchar* pathName)
	{
#if PLATFORM_WINDOWS
		return ::CreateDirectoryW(pathName, NULL);
#else
		if (mkdir(pathName, 0777) == 0)
		{
			SetLastError(ERROR_SUCCESS);
			return true;
		}
		if (errno == EEXIST)
		{
			SetLastError(ERROR_ALREADY_EXISTS);
			return false;
		}
		if (errno == ENOENT)
		{
			SetLastError(ERROR_PATH_NOT_FOUND);
			return false;
		}
		UBA_ASSERTF(false, TC("CreateDirectoryW failed creating %s - error handling %i not handled (%s)"), pathName, errno, strerror(errno));
		return false;
#endif
	}

	bool RemoveDirectoryW(const tchar* pathName)
	{
#if PLATFORM_WINDOWS
		if (::RemoveDirectoryW(pathName))
			return true;
		return false;
#else
		int res = rmdir(pathName);
		if (res == 0)
			return true;
		if (errno == ENOENT)
		{
			SetLastError(ERROR_FILE_NOT_FOUND);
			return false;
		}
		UBA_ASSERTF(false, TC("RemoveDirectoryW error handling not implemented (%s): %s"), strerror(errno), pathName);
		return false;
#endif
	}

	bool DeleteFileW(const tchar* fileName)
	{
#if PLATFORM_WINDOWS
		if (::DeleteFileW(fileName))
			return true;
		return false;
#else
		int res = remove(fileName);
		if (res == 0)
			return true;
		if (errno == ENOENT)
		{
			SetLastError(ERROR_FILE_NOT_FOUND);
			return false;
	}
		UBA_ASSERTF(false, TC("DeleteFileW failed on %s - Error handling not implemented (%s)"), fileName, strerror(errno));
		return false;
#endif
	}

	bool CopyFileW(const tchar* existingFileName, const tchar* newFileName, bool bFailIfExists)
	{
#if PLATFORM_WINDOWS
		return ::CopyFileW(existingFileName, newFileName, bFailIfExists);
#else

		UBA_ASSERTF(false, TC("CopyFileW not implemented"));
		return false;
#endif
	}

	u32 GetLongPathNameW(const tchar* lpszShortPath, tchar* lpszLongPath, u32 cchBuffer)
	{
#if PLATFORM_WINDOWS
		return ::GetLongPathNameW(lpszShortPath, lpszLongPath, cchBuffer);
#else
		UBA_ASSERTF(false, TC("GetLongPathNameW not implemented"));
		return false;
#endif
	}

	bool GetFileLastWriteTime(u64& outTime, FileHandle hFile)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().getFileTime);
#if PLATFORM_WINDOWS
		FILETIME lastWriteTime;
		auto res = ::GetFileTime(asHANDLE(hFile), NULL, NULL, &lastWriteTime);
		outTime = (u64&)lastWriteTime;
		return res;
#else
		struct stat attr;
		int res = fstat(asFileDescriptor(hFile), &attr);
		if (res != 0)
		{
			UBA_ASSERTF(false, TC("GetFileLastWriteTime (fstat) error handling not implemented: %s"), strerror(errno));
			return false;
		}
		outTime = FromTimeSpec(attr.st_mtimespec);
		return true;
#endif
	}

	bool SetFileLastWriteTime(FileHandle fileHandle, u64 writeTime)
	{
#if PLATFORM_WINDOWS
		return SetFileTime(asHANDLE(fileHandle), (FILETIME*)&writeTime, NULL, (FILETIME*)&writeTime);
#else
		return true;
#endif
	}

	bool MoveFileExW(const tchar* existingFileName, const tchar* newFileName, u32 dwFlags)
	{
#if PLATFORM_WINDOWS
		return ::MoveFileExW(existingFileName, newFileName, dwFlags);
#else
		int res = rename(existingFileName, newFileName);
		if (res == 0)
		{
			SetLastError(ERROR_SUCCESS);
			return true;
		}
		UBA_ASSERTF(false, TC("MoveFileExW error handling not implemented"));
		return false;
#endif
	}

	bool GetFileSizeEx(u64& outFileSize, FileHandle hFile)
	{
#if PLATFORM_WINDOWS
		LARGE_INTEGER lpFileSize;
		if (!::GetFileSizeEx(asHANDLE(hFile), &lpFileSize))
			return false;
		outFileSize = u64(lpFileSize.QuadPart);
		return true;
#else
		struct stat attr;
		int res = fstat(asFileDescriptor(hFile), &attr);
		if (res == 0)
		{
			outFileSize = attr.st_size;
			return true;
		}

		UBA_ASSERTF(false, TC("GetFileSizeEx error handling not implemented"));
		return false;
#endif
	}

	u32 GetFileAttributesW(const tchar* fileName)
	{
#if PLATFORM_WINDOWS
		return ::GetFileAttributesW(fileName);
#else
		struct stat attr;
		if (stat(fileName, &attr) == -1)
		{
			if (errno == ENOENT)
			{
				SetLastError(ERROR_FILE_NOT_FOUND);
				return INVALID_FILE_ATTRIBUTES;
			}
			UBA_ASSERTF(false, TC("GetFileAttributesW error handling not implemented %s (%s)"), fileName, strerror(errno));
			return INVALID_FILE_ATTRIBUTES;
		}
		return attr.st_mode;
#endif
	}

	bool IsDirectory(u32 attributes)
	{
#if PLATFORM_WINDOWS
		return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
		return S_ISDIR(attributes);
#endif
	}

	bool IsReadOnly(u32 attributes)
	{
#if PLATFORM_WINDOWS
		return (attributes & FILE_ATTRIBUTE_READONLY) != 0;
#else
		return false;
#endif
	}

	u32 DefaultAttributes(bool execute)
	{
#if PLATFORM_WINDOWS
		return FILE_ATTRIBUTE_NORMAL;
#else
		return S_IRUSR | S_IWUSR | (execute ? S_IXUSR : 0);
#endif
	}

	bool CreateHardLinkW(const tchar* newFileName, const tchar* existingFileName)
	{
#if PLATFORM_WINDOWS
		return ::CreateHardLinkW(newFileName, existingFileName, NULL);
#else
#if 1//PLATFORM_MAC
		int res = symlink(existingFileName, newFileName);
#else
		int res = link(existingFileName, newFileName);
#endif
		if (res == 0)
			return true;

		UBA_ASSERTF(false, TC("CreateHardLinkW %s to %s error handling not implemented (%s)"), existingFileName, newFileName, strerror(errno));
		return false;
#endif
	}

	u32 GetFullPathNameW(const tchar* fileName, u32 nBufferLength, tchar* lpBuffer, tchar** lpFilePart)
	{
#if PLATFORM_WINDOWS
		return ::GetFullPathNameW(fileName, nBufferLength, lpBuffer, lpFilePart);
#else
		char source[1024];
		size_t sourceStart = 0;
		size_t fileNameStart = 0;

		if (fileName[0] == '~' && fileName[1] == '/')
		{
			strcpy(source, getenv("HOME"));
			sourceStart = strlen(source);
			source[sourceStart++] = '/';
			fileNameStart = 2;
			TSprintf_s(source + sourceStart, 1024 - sourceStart, "%s", fileName + fileNameStart);
			return strlen(strcpy(lpBuffer, source));
		}

#if 0
		char fullPath[1024];
		if (!realpath(fileName, fullPath))
		{
			UBA_ASSERTF(false, TC("realpath error handling not implemented for path %s (%s)"), fileName, strerror(errno));
			return 0;
		}
		u32 len = TStrlen(fullPath);
		UBA_ASSERT(len < sizeof_array(fullPath));
#elif 0
		const char* fullPath = canonicalize_file_name(fileName);
		u32 len = TStrlen(fullPath);
#else
		char cwd[PATH_MAX];
		if (!getcwd(cwd, PATH_MAX))
		{
			UBA_ASSERT(false);
			return 0;
		}
		u32 cwdlen = u32(strlen(cwd));
		cwd[cwdlen++] = '/';
		cwd[cwdlen] = 0;

		char fullPath[1024];
		u32 len;
		if (!FixPath2(fileName, cwd, cwdlen, fullPath, sizeof(fullPath), &len))
			return 0;
#endif
		UBA_ASSERT(len < nBufferLength);
		memcpy(lpBuffer, fullPath, len + 1);
		return len;
#endif
	}

	bool SearchPathW(const tchar* a, const tchar* b, const tchar* c, u32 d, tchar* e, tchar** f)
	{
#if PLATFORM_WINDOWS
		return ::SearchPathW(a, b, c, d, e, f);
#else
		UBA_ASSERTF(false, TC("SearchPathW not implemented"));
		return false;
#endif
	}

	u64 GetSystemTimeAsFileTime()
	{
#if PLATFORM_WINDOWS
		FILETIME temp;
		::GetSystemTimeAsFileTime(&temp); // Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
		return u64(ToLargeInteger(temp.dwHighDateTime, temp.dwLowDateTime).QuadPart);
#else
		timeval tv;
		gettimeofday(&tv, NULL);
		return u64(tv.tv_sec) * 10'000'000ull + u64(tv.tv_usec)*10ull;
#endif
	}

	u64 GetFileTimeAsSeconds(u64 fileTime)
	{
#if PLATFORM_WINDOWS
		return TimeToMs(fileTime)/1000;
#else
		return fileTime / 10'000'000ull;
#endif
	}

	bool GetCurrentDirectoryW(StringBufferBase& out)
	{
#if PLATFORM_WINDOWS
		u32 res = ::GetCurrentDirectoryW(out.capacity, out.data);
		if (!res || res > out.capacity)
		{
			UBA_ASSERT(false);
			return false;
		}
		out.count = res;
		return true;
#else
		if (!getcwd(out.data, out.capacity))
		{
			UBA_ASSERT(false);
			return false;
		}
		out.count = strlen(out.data);
		return true;
#endif
	}

	bool DirectoryCache::CreateDirectory(Logger& logger, const tchar* dir)
	{
		u64 dirLen = TStrlen(dir);
		if (dir[dirLen - 1] == PathSeparator)
			--dirLen;
		TString key(dir, dir + dirLen);
		dir = key.c_str();

		SCOPED_WRITE_LOCK(m_createdDirsLock, lock);
		CreatedDir& cd = m_createdDirs.try_emplace(key).first->second;
		lock.Leave();
		SCOPED_WRITE_LOCK(cd.lock, dirLock);
		if (cd.handled)
			return true;
		cd.handled = true;
		if (uba::CreateDirectoryW(dir))
			return true;
		u32 lastError = GetLastError();
		if (lastError == ERROR_ALREADY_EXISTS)
			return true;
		if (lastError != ERROR_PATH_NOT_FOUND)
			return logger.Error(TC("Failed to create directory %s (%s)"), dir, LastErrorToText(lastError).data);

		tchar temp[512];
		const tchar* lastSep = TStrrchr(dir, PathSeparator);
		u64 pos = u64(lastSep - dir);
		memcpy(temp, dir, pos * sizeof(tchar));
		temp[pos] = 0;
		if (pos == 2 && temp[1] == ':')
			return false;
		if (!CreateDirectory(logger, temp))
			return false;
		if (!uba::CreateDirectoryW(dir))
			return logger.Error(TC("Failed to create directory %s (%s)"), dir, LastErrorToText().data);
		return true;
	}

	void DirectoryCache::Clear()
	{
		SCOPED_WRITE_LOCK(m_createdDirsLock, lock);
		m_createdDirs.clear();
	}
}

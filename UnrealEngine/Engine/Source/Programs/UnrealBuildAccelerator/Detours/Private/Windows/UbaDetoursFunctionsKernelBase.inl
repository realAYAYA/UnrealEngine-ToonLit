// Copyright Epic Games, Inc. All Rights Reserved.

const wchar_t* ToString(BOOL b) { return b ? L"Success" : L"Error"; }

DWORD Local_GetLongPathNameW(LPCWSTR lpszShortPath, LPWSTR lpszLongPath, DWORD cchBuffer)
{
	SCOPED_WRITE_LOCK(g_longPathNameCacheLock, lock);
	auto findIt = g_longPathNameCache.find(lpszShortPath);
	if (findIt != g_longPathNameCache.end())
	{
		const wchar_t* longPath = findIt->second;
		u64 len = wcslen(longPath);
		UBA_ASSERT(cchBuffer > len);
		memcpy(lpszLongPath, longPath, (len + 1) * 2);
		return u32(len);
	}

	wchar_t* newShortPath = g_memoryBlock.Strdup(lpszShortPath);
	DEBUG_LOG_TRUE(L"GetLongPathNameW", L"(Detour disabled under this call to handle ~) (%ls)", lpszShortPath);

	SuppressDetourScope _;
	DWORD res = True_GetLongPathNameW(lpszShortPath, lpszLongPath, cchBuffer);
	if (res == 0)
		return res;
	g_longPathNameCache.insert({ newShortPath, g_memoryBlock.Strdup(lpszLongPath) });
	return res;
}

LPWSTR Detoured_GetCommandLineW()
{
	DETOURED_CALL(GetCommandLineW);
	if (!g_runningRemote)
	{
		LPWSTR str = True_GetCommandLineW();
		DEBUG_LOG_TRUE(L"GetCommandLineW", L"");// str);
		return str;
	}
	DEBUG_LOG_DETOURED(L"GetCommandLineW", L"");// g_virtualCommandLine);
	return g_virtualCommandLine;
}

DWORD Detoured_GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer)
{
	DETOURED_CALL(GetCurrentDirectoryW);
	u64 length = g_virtualWorkingDir.count - 1; // Skip last slash
	SetLastError(ERROR_SUCCESS);
	if (lpBuffer == nullptr || nBufferLength < length + 1)
	{
		DEBUG_LOG_DETOURED(L"GetCurrentDirectoryW", L"(buffer too small: %u)", nBufferLength);
		return DWORD(length + 1);
	}
	memcpy(lpBuffer, g_virtualWorkingDir.data, length * 2);
	lpBuffer[length] = 0; // Skip last slash
	DEBUG_LOG_DETOURED(L"GetCurrentDirectoryW", L"(%ls)", lpBuffer);
	return DWORD(length);

	//DEBUG_LOG_TRUE(L"GetCurrentDirectoryW", L"");
	//auto res = True_GetCurrentDirectoryW(nBufferLength, lpBuffer);
	//return res;
}

DWORD Detoured_GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer)
{
	DETOURED_CALL(GetCurrentDirectoryA);
	u64 length = g_virtualWorkingDir.count - 1; // Skip last slash
	SetLastError(ERROR_SUCCESS);
	if (lpBuffer == nullptr || nBufferLength < length + 1)
	{
		DEBUG_LOG_DETOURED(L"GetCurrentDirectoryA", L"(buffer too small: %u)", nBufferLength);
		return DWORD(length + 1);
	}
	size_t res;
	errno_t err = wcstombs_s(&res, lpBuffer, nBufferLength, g_virtualWorkingDir.data, length);
	if (err)
		UBA_ASSERTF(false, L"wcstombs_s failed for string '%s' with error code: %u", err);
	DEBUG_LOG_DETOURED(L"GetCurrentDirectoryA", L"(%hs)", lpBuffer);
	return DWORD(length);
}

void Shared_SetCurrentDirectory(const wchar_t* workingDirBuffer)
{
	u32 charLen = 0;
	wchar_t temp[256];
	FixPath2(workingDirBuffer, g_virtualWorkingDir.data, g_virtualWorkingDir.count, temp, sizeof_array(temp), &charLen);
	g_virtualWorkingDir.Clear().Append(temp).Append('\\');
}


BOOL Detoured_SetCurrentDirectoryW(LPCWSTR lpPathName)
{
	DETOURED_CALL(SetCurrentDirectoryW);

	Shared_SetCurrentDirectory(lpPathName);

	if (g_runningRemote)
	{
		DEBUG_LOG_DETOURED(L"SetCurrentDirectoryW", L"%ls", lpPathName);
		return true;
	}

	DEBUG_LOG_TRUE(L"SetCurrentDirectoryW", L"%ls", lpPathName);
	return True_SetCurrentDirectoryW(lpPathName);
}


BOOL Detoured_DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions)
{
	DETOURED_CALL(DuplicateHandle);
	if (hSourceHandle == PseudoHandle || !isDetouredHandle(hSourceHandle))
	{
		auto res = True_DuplicateHandle(hSourceProcessHandle, hSourceHandle, hTargetProcessHandle, lpTargetHandle, dwDesiredAccess, bInheritHandle, dwOptions);
		DEBUG_LOG_TRUE(L"DuplicateHandle", L"%llu %llu -> %ls", uintptr_t(hSourceHandle), lpTargetHandle ? uintptr_t(*lpTargetHandle) : 0ull, ToString(res));
		return res;
	}

	auto& dh = asDetouredHandle(hSourceHandle);

	HANDLE trueHandle = dh.trueHandle;
	HANDLE targetHandle = INVALID_HANDLE_VALUE;

	BOOL res = TRUE;
	if (trueHandle != INVALID_HANDLE_VALUE)
		res = True_DuplicateHandle(hSourceProcessHandle, trueHandle, hTargetProcessHandle, &targetHandle, dwDesiredAccess, bInheritHandle, dwOptions);
	else
		SetLastError(ERROR_SUCCESS);

	auto newDh = new DetouredHandle(dh.type);
	newDh->trueHandle = targetHandle;
	newDh->dirTableOffset = dh.dirTableOffset;
	newDh->fileObject = dh.fileObject;
	if (FileObject* fo = dh.fileObject)
	{
		//UBA_ASSERT(!fo->fileInfo->isFileMap);
		InterlockedIncrement(&fo->refCount);
	}
	*lpTargetHandle = makeDetouredHandle(newDh);
	DEBUG_LOG_TRUE(L"DuplicateHandle", L"%llu %llu -> %ls", uintptr_t(hSourceHandle), uintptr_t(*lpTargetHandle), ToString(res));
	return res;
}

HANDLE Detoured_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	DETOURED_CALL(CreateFileW);
	DEBUG_LOG_DETOURED(L"CreateFileW", L"%ls", lpFileName);
	u32 disallowDetour = Equals(lpFileName, L"nul");
	t_disallowDetour += disallowDetour;
	t_createFileFileName = lpFileName;
	HANDLE h = True_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	t_createFileFileName = nullptr;
	t_disallowDetour -= disallowDetour;
	return h;
}

// Calls directly to NtCreateFile so need to be detoured
HANDLE Detoured_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	DETOURED_CALL(CreateFileA);
	DEBUG_LOG_TRUE(L"CreateFileA", L"%hs", lpFileName);
	StringBuffer<> fileName;
	fileName.Appendf(L"%hs", lpFileName);
	u32 disallowDetour = fileName.Equals(L"nul");
	t_disallowDetour += disallowDetour;
	t_createFileFileName = fileName.data;
	HANDLE h = True_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	t_createFileFileName = nullptr;
	t_disallowDetour -= disallowDetour;
	return h;
}

BOOL Detoured_CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	DETOURED_CALL(CreateDirectoryW);

	StringBuffer<> pathName;
	FixPath(pathName, lpPathName);
	if (pathName.StartsWith(g_systemTemp.data))
	{
		SuppressCreateFileDetourScope s;
		BOOL res = True_CreateDirectoryW(lpPathName, lpSecurityAttributes);
		DEBUG_LOG_TRUE(L"CreateDirectoryW", L"%ls -> %ls", lpPathName, ToString(res));
		return res;
	}

	BOOL res;
	u32 errorCode = 0;
	StringKey pathNameKey = ToStringKeyLower(pathName);

	{
		TimerScope ts(g_stats.createFile);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_CreateDirectory);
		writer.WriteStringKey(pathNameKey);
		writer.WriteString(pathName);
		writer.Flush();
		BinaryReader reader;
		res = reader.ReadBool();
		errorCode = reader.ReadU32();
	}

	SetLastError(errorCode);
	DEBUG_LOG_DETOURED(L"CreateDirectoryW", L"%ls -> %ls (%u)", lpPathName, ToString(res), errorCode);
	return res;
}

BOOL Detoured_RemoveDirectoryW(LPCWSTR lpPathName)
{
	DETOURED_CALL(RemoveDirectoryW);

	StringBuffer<> pathName;
	FixPath(pathName, lpPathName);
	BOOL res;
	if (!g_runningRemote || pathName.StartsWith(g_systemTemp.data))
	{
		SuppressCreateFileDetourScope s; // TODO: Revisit this.. will not work remotely
		res = True_RemoveDirectoryW(lpPathName);
	}
	else
	{
		UBA_ASSERTF(!g_runningRemote, L"RemoveDirectory is not implemented for remote (removing %s)", lpPathName);
		res = false;
	}
	DEBUG_LOG_TRUE(L"RemoveDirectoryW", L"%ls -> %ls", lpPathName, ToString(res));

	return res;
}

BOOL Detoured_LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh)
{
	DETOURED_CALL(LockFile);
	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}
	DEBUG_LOG_TRUE(L"LockFile", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	return True_LockFile(trueHandle, dwFileOffsetLow, dwFileOffsetHigh, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh);
}

BOOL Detoured_LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped)
{
	DETOURED_CALL(LockFileEx);
	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}
	DEBUG_LOG_TRUE(L"LockFileEx", L"%llu %ls", uintptr_t(hFile), HandleToName(hFile));
	return True_LockFileEx(trueHandle, dwFlags, dwReserved, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);
}

BOOL Detoured_UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh)
{
	DETOURED_CALL(UnlockFile);
	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}
	DEBUG_LOG_TRUE(L"UnlockFile", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	return True_UnlockFile(trueHandle, dwFileOffsetLow, dwFileOffsetHigh, nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh);
}

BOOL Detoured_UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh, LPOVERLAPPED lpOverlapped)
{
	DETOURED_CALL(UnlockFileEx);
	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}
	DEBUG_LOG_TRUE(L"UnlockFile", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	return True_UnlockFileEx(trueHandle, dwReserved, nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh, lpOverlapped);
}

BOOL Detoured_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	DETOURED_CALL(ReadFile);
	HANDLE trueHandle = hFile;
	UBA_ASSERT(!isListDirectoryHandle(hFile));
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);

		if (dh.type == HandleType_Std) // HACK HACK
		{
			UBA_ASSERTF(false, L"Trying to read input from stdin while application is running in a way console can not be accessed");
			memcpy(lpBuffer, "Y\r\n", 3);
			*lpNumberOfBytesRead = 3;
			return TRUE;
		}

		FileInfo& fi = *dh.fileObject->fileInfo;
		if (fi.isFileMap || fi.memoryFile)
		{
			// TODO: Handle lpOverlapped - If a read happen and there is 0 left it should return 0 with SetLastError(ERROR_HANDLE_EOF)
			if (!EnsureMapped(dh))
			{
				DEBUG_LOG_DETOURED(L"ReadFile", L"%llu %u (%ls) -> FAILED TO MAP", uintptr_t(hFile), nNumberOfBytesToRead, HandleToName(hFile));
				return FALSE;
			}
			u8* mem = fi.fileMapMem ? fi.fileMapMem : fi.memoryFile->baseAddress;
			u64 size = fi.fileMapMem ? fi.fileMapMemEnd - mem : fi.memoryFile->writtenSize;
			UBA_ASSERTF(dh.pos <= size, L"Filepointer is higher than size of file (pointer: %llu, size: %llu) (%ls)", dh.pos, size, HandleToName(hFile));
			u64 leftToRead = size - dh.pos;
			if (nNumberOfBytesToRead > leftToRead)
				nNumberOfBytesToRead = (DWORD)leftToRead;
			if (nNumberOfBytesToRead)
				memcpy(lpBuffer, mem + dh.pos, nNumberOfBytesToRead);
			dh.pos += nNumberOfBytesToRead;
			if (lpNumberOfBytesRead)
				*lpNumberOfBytesRead = nNumberOfBytesToRead;
			SetLastError(ERROR_SUCCESS);
			DEBUG_LOG_DETOURED(L"ReadFile", L"%llu %u (%ls) -> Success", uintptr_t(hFile), nNumberOfBytesToRead, HandleToName(hFile));
			return TRUE;
		}
		UBA_ASSERT(dh.trueHandle != INVALID_HANDLE_VALUE);
		trueHandle = dh.trueHandle;
	}

	BOOL res = True_ReadFile(trueHandle, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
	DEBUG_LOG_TRUE(L"ReadFile", L"%llu %u/%u (%ls) -> %ls", uintptr_t(hFile), lpNumberOfBytesRead ? *lpNumberOfBytesRead : ~0u, nNumberOfBytesToRead, HandleToName(hFile), ToString(res));
	return res;
}


BOOL Detoured_WriteConsoleA(HANDLE hConsoleOutput, const VOID* lpBuffer, DWORD nNumberOfCharsToWrite, LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved)
{
	DETOURED_CALL(WriteConsoleA);
	//DEBUG_LOG_TRUE_AND_DETOURED(L"WriteConsoleA (%hs)", (char*)lpBuffer);
	Shared_WriteConsole((const char*)lpBuffer, nNumberOfCharsToWrite, false);
	if (lpNumberOfCharsWritten)
		*lpNumberOfCharsWritten = nNumberOfCharsToWrite;
	return TRUE;//True_WriteConsoleA(hConsoleOutput, lpBuffer, nNumberOfCharsToWrite, lpNumberOfCharsWritten, lpReserved);
}

BOOL Detoured_WriteConsoleW(HANDLE hConsoleOutput, const VOID* lpBuffer, DWORD nNumberOfCharsToWrite, LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved)
{
	DETOURED_CALL(WriteConsoleW);
	//DEBUG_LOG_DETOURED(L"WriteConsoleW"", L""); // Too much spam
	Shared_WriteConsole((const wchar_t*)lpBuffer, nNumberOfCharsToWrite, false);
	if (lpNumberOfCharsWritten)
		*lpNumberOfCharsWritten = nNumberOfCharsToWrite;
	return TRUE;//True_WriteConsoleW(hConsoleOutput, lpBuffer, nNumberOfCharsToWrite, lpNumberOfCharsWritten, lpReserved);
}

BOOL Detoured_ReadConsoleW(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead, LPDWORD lpNumberOfCharsRead, PCONSOLE_READCONSOLE_CONTROL pInputControl)
{
	DETOURED_CALL(ReadConsoleW);
	Rpc_WriteLogf(L"WARNING Got call to ReadConsoleW.. this is not handled by Uba yet");
	return 0;
	//return True_ReadConsoleW(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
}

UINT Detoured_GetDriveTypeW(LPCWSTR lpRootPathName)
{
	DETOURED_CALL(GetDriveTypeW);
	if (g_runningRemote)
	{
		DEBUG_LOG_DETOURED(L"GetDriveType", L"%ls", lpRootPathName);
		return DRIVE_FIXED;
	}
	DEBUG_LOG_TRUE(L"GetDriveType", L"%ls", lpRootPathName);
	SuppressCreateFileDetourScope s; // Convenient since it will call NtQueryVolumeInformationFile
	return True_GetDriveTypeW(lpRootPathName);
}

BOOL Detoured_GetDiskFreeSpaceExW(LPCWSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailableToCaller, PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes)
{
	DETOURED_CALL(GetDiskFreeSpaceExW);
	StringBuffer<MaxPath> path;
	if (g_runningRemote)
	{
		if (lpDirectoryName)
		{
			UBA_ASSERT(lpDirectoryName[1] == ':');
			if (ToLower(lpDirectoryName[0]) == ToLower(g_virtualWorkingDir[0]))
			{
				if (lpDirectoryName[3] == 0)
					path.Append(g_exeDir.data, 3);
				else
					path.Append(g_exeDir);
				lpDirectoryName = path.data;
			}
		}
	}

	DEBUG_LOG_TRUE(L"GetDiskFreeSpaceExW", L"%ls", lpDirectoryName);
	SuppressCreateFileDetourScope s; // Convenient since it will call NtQueryVolumeInformationFile
	return True_GetDiskFreeSpaceExW(lpDirectoryName, lpFreeBytesAvailableToCaller, lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes);
}


BOOL Detoured_GetVolumeInformationByHandleW(HANDLE hFile, LPWSTR lpVolumeNameBuffer, DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize)
{
	DETOURED_CALL(GetVolumeInformationByHandleW);
	HANDLE trueHandle = hFile;

	u32 entryOffset = ~u32(0);

	if (isDetouredHandle(hFile))
	{
		DetouredHandle& dh = asDetouredHandle(hFile);
		trueHandle = dh.trueHandle;
		entryOffset = dh.dirTableOffset;
		UBA_ASSERT(entryOffset != ~u32(0) || trueHandle != INVALID_HANDLE_VALUE);
	}
	else if (isListDirectoryHandle(hFile))
	{
		auto& listHandle = asListDirectoryHandle(hFile);
		if (listHandle.dir.tableOffset != InvalidTableOffset)
			entryOffset = listHandle.dir.tableOffset | 0x80000000;
		else
			UBA_ASSERT(false);
		trueHandle = INVALID_HANDLE_VALUE;
	}

	if (entryOffset != ~u32(0))
	{
		UBA_ASSERT(!lpVolumeNameBuffer);
		UBA_ASSERT(!lpMaximumComponentLength);
		UBA_ASSERT(!lpFileSystemFlags);
		DirectoryTable::EntryInformation entryInfo;
		g_directoryTable.GetEntryInformation(entryInfo, entryOffset);
		if (lpVolumeSerialNumber)
			*lpVolumeSerialNumber = entryInfo.volumeSerial;
		if (lpFileSystemNameBuffer)
		{
			UBA_ASSERT(nFileSystemNameSize > 5);
			wcscpy_s(lpFileSystemNameBuffer, nFileSystemNameSize, L"NTFS"); // TODO: Not everyone has NTFS?
		}
		SetLastError(ERROR_SUCCESS);
		DEBUG_LOG_DETOURED(L"GetVolumeInformationByHandleW", L"%llu %ls", uintptr_t(hFile), HandleToName(hFile));
		return true;
	}
	bool res = True_GetVolumeInformationByHandleW(trueHandle, lpVolumeNameBuffer, nVolumeNameSize, lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);
	return res;
}

BOOL Detoured_GetVolumeInformationW(LPCWSTR lpRootPathName, LPWSTR lpVolumeNameBuffer, DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize)
{
	DETOURED_CALL(GetVolumeInformationW);
	if (g_runningRemote)
	{
		UBA_ASSERT(!lpVolumeNameBuffer);
		UBA_ASSERT(!lpVolumeSerialNumber);
		UBA_ASSERT(!lpMaximumComponentLength);
		UBA_ASSERT(!lpFileSystemFlags);

		wcscpy_s(lpFileSystemNameBuffer, nFileSystemNameSize, L"NTFS"); // TODO: Not everyone has NTFS?
		SetLastError(ERROR_SUCCESS);
		DEBUG_LOG_DETOURED(L"GetVolumeInformationW", L"%ls", lpRootPathName);
		return true;
	}
	SuppressCreateFileDetourScope s;
	auto res = True_GetVolumeInformationW(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize, lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);
	return res;
}

LPVOID Detoured_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
{
	DETOURED_CALL(VirtualAlloc);
	// Special cl.exe handling
	if (lpAddress != nullptr && g_clExeBaseReservedMemory != nullptr && lpAddress >= g_clExeBaseReservedMemory && uintptr_t(lpAddress) < uintptr_t(g_clExeBaseReservedMemory) + g_clExeBaseAddressSize)
	{
		DEBUG_LOG(L"VirtualAlloc releasing cl.exe reserved memory at 0x%llx", uintptr_t(lpAddress));
		VirtualFree(g_clExeBaseReservedMemory, 0, MEM_RELEASE);
		g_clExeBaseReservedMemory = nullptr;
	}


	u32 counter = 0;
	do
	{
		void* res = True_VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
		if (res)
			return res;
		if (!(flAllocationType & MEM_COMMIT))
			return res;
		DWORD error = GetLastError();
		if (error != ERROR_NOT_ENOUGH_MEMORY && error != ERROR_COMMITMENT_LIMIT)
			return res;
		Rpc_AllocFailed(L"VirtualAlloc", error);
		++counter;

	} while (counter <= 10);

	return nullptr;
}

BOOL Detoured_GetQueuedCompletionStatusEx(HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries, ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds, BOOL fAlertable)
{
	DETOURED_CALL(GetQueuedCompletionStatusEx);
	DEBUG_LOG_TRUE(L"GetQueuedCompletionStatusEx", L"%llu (Timeout: %ums)", u64(CompletionPort), dwMilliseconds);
	bool res = True_GetQueuedCompletionStatusEx(CompletionPort, lpCompletionPortEntries, ulCount, ulNumEntriesRemoved, dwMilliseconds, fAlertable);
	if (res)
		Rpc_UpdateTables(); // This is a bit ugly but we know this is how msbuild worker nodes sync with each other..
	return res;
}

DWORD Detoured_GetSecurityInfo(HANDLE handle, SE_OBJECT_TYPE ObjectType, SECURITY_INFORMATION SecurityInfo, PSID* ppsidOwner, PSID* ppsidGroup, PACL* ppDacl, PACL* ppSacl, PSECURITY_DESCRIPTOR* ppSecurityDescriptor)
{
	DETOURED_CALL(GetSecurityInfo);
	if (isDetouredHandle(handle))
	{
		handle = asDetouredHandle(handle).trueHandle;
		UBA_ASSERTF(handle != INVALID_HANDLE_VALUE, L"GetSecurityInfo");
	}

	DEBUG_LOG_TRUE(L"GetSecurityInfo", L"");
	return True_GetSecurityInfo(handle, ObjectType, SecurityInfo, ppsidOwner, ppsidGroup, ppDacl, ppSacl, ppSecurityDescriptor);
}

void WriteStdFile(LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, bool isError)
{
	if (!g_echoOn)
		return;

	SCOPED_WRITE_LOCK(g_stdFileLock, lock);
	u32 start = 0;
	u32 i = 0;
	auto bufferStr = (const char*)lpBuffer;
	while (i != nNumberOfBytesToWrite)
	{
		if (bufferStr[i] == '\n')
		{
			int len = i - start;
			if (len > 0 && bufferStr[i - 1] == '\r')
				--len;
			if (len)
				g_stdFile.Appendf(L"%.*hs", len, bufferStr + start);
			Rpc_WriteLog(g_stdFile.data, g_stdFile.count, false, isError);
			g_stdFile.Clear();
			start = i + 1;
		}
		++i;
	}
	if (u32 left = nNumberOfBytesToWrite - start)
		g_stdFile.Appendf(L"%.*hs", left, bufferStr + start);
}

BOOL Detoured_WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	DETOURED_CALL(WriteFile);
	HANDLE trueHandle = hFile;
	UBA_ASSERT(!isListDirectoryHandle(hFile));
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		auto& fo = *dh.fileObject;

		if (dh.type == HandleType_Std)
		{
			WriteStdFile(lpBuffer, nNumberOfBytesToWrite, hFile == g_stdHandle[0]);
			*lpNumberOfBytesWritten = nNumberOfBytesToWrite;
			SetLastError(ERROR_SUCCESS);
			return true;
		}
		auto& fi = *fo.fileInfo;
		if (MemoryFile* mf = fi.memoryFile)
		{
			mf->Write(dh, lpBuffer, nNumberOfBytesToWrite);
			*lpNumberOfBytesWritten = nNumberOfBytesToWrite;
			SetLastError(ERROR_SUCCESS);
			DEBUG_LOG_DETOURED(L"WriteFile", L"(MEMORY) %llu (%ls) ToWrite: %u -> Success", uintptr_t(hFile), HandleToName(hFile), nNumberOfBytesToWrite);
			return TRUE;
		}
		UBA_ASSERTF(!fi.isFileMap, L"Trying to write to file %ls which is a filemap. This is not supported\n", HandleToName(hFile));
		UBA_ASSERTF(dh.trueHandle != INVALID_HANDLE_VALUE, L"Trying to write to file %ls which does not have a valid handle\n", HandleToName(hFile));
		trueHandle = dh.trueHandle;
	}
	else if (hFile == PseudoHandle)
	{
		DEBUG_LOG_DETOURED(L"WriteFile", L"(PseudoHandle) -> Success");
		SetLastError(ERROR_SUCCESS);
		return true;
	}
	else if (hFile == g_stdHandle[1] || hFile == g_stdHandle[0])
	{
		WriteStdFile(lpBuffer, nNumberOfBytesToWrite, hFile == g_stdHandle[0]);
		SetLastError(ERROR_SUCCESS);
		return true;
	}

	BOOL res = True_WriteFile(trueHandle, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
	DEBUG_LOG_TRUE(L"WriteFile", L"%llu (%ls) -> %ls", uintptr_t(hFile), HandleToName(hFile), ToString(res));
	return res;
}

BOOL Detoured_WriteFileEx(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	DETOURED_CALL(WriteFileEx);
	//DEBUG_LOG_TRUE(L"WriteFileEx", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	//UBA_ASSERT(!isDetouredHandle(hFile));
	UBA_ASSERT(isDetouredHandle(hFile));
	DetouredHandle& h = asDetouredHandle(hFile);
	UBA_ASSERT(h.trueHandle != INVALID_HANDLE_VALUE);
	return True_WriteFileEx(h.trueHandle, lpBuffer, nNumberOfBytesToWrite, lpOverlapped, lpCompletionRoutine);
}

BOOL Detoured_FlushFileBuffers(HANDLE hFile)
{
	DETOURED_CALL(FlushFileBuffers);

	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		DetouredHandle& dh = asDetouredHandle(hFile);
		FileInfo& fi = *dh.fileObject->fileInfo;
		if (fi.memoryFile)
		{
			DEBUG_LOG_DETOURED(L"FlushFileBuffers", L"%llu (%ls) -> Success", uintptr_t(hFile), HandleToName(hFile));
			SetLastError(ERROR_SUCCESS);
			return true;
		}
		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}

	BOOL res = True_FlushFileBuffers(trueHandle);
	DEBUG_LOG_TRUE(L"FlushFileBuffers", L"%llu (%ls) -> %ls", uintptr_t(hFile), HandleToName(hFile), ToString(res));
	return res;
}

DWORD Detoured_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
	DETOURED_CALL(GetFileSize);

	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		FileInfo& fi = *dh.fileObject->fileInfo;
		if (fi.size != InvalidValue)
		{
			LARGE_INTEGER li = ToLargeInteger(fi.size);
			if (lpFileSizeHigh)
				*lpFileSizeHigh = li.HighPart;
			DEBUG_LOG_DETOURED(L"GetFileSize", L"%llu (%ls) -> %u", uintptr_t(hFile), HandleToName(hFile), li.LowPart);
			SetLastError(ERROR_SUCCESS);
			return li.LowPart;
		}
		if (fi.memoryFile)
		{
			LARGE_INTEGER li = ToLargeInteger(fi.memoryFile->writtenSize);
			if (lpFileSizeHigh)
				*lpFileSizeHigh = li.HighPart;
			DEBUG_LOG_DETOURED(L"GetFileSize", L"%llu (%ls) -> %u", uintptr_t(hFile), HandleToName(hFile), li.LowPart);
			SetLastError(ERROR_SUCCESS);
			return li.LowPart;
		}
		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}

	DEBUG_LOG_TRUE(L"GetFileSize", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	return True_GetFileSize(trueHandle, lpFileSizeHigh); // Calls NtQueryInformationFile
}

DWORD Detoured_GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize)
{
	DETOURED_CALL(GetFileSizeEx);
	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		DetouredHandle& dh = asDetouredHandle(hFile);
		FileInfo& fi = *dh.fileObject->fileInfo;
		if (fi.size != InvalidValue)
		{
			*lpFileSize = ToLargeInteger(fi.size);
			SetLastError(ERROR_SUCCESS);
			DEBUG_LOG_DETOURED(L"GetFileSizeEx", L"%llu (%ls) (Size:%llu) -> 1", uintptr_t(hFile), HandleToName(hFile), fi.size);
			return 1;
		}
		if (fi.memoryFile)
		{
			*lpFileSize = ToLargeInteger(fi.memoryFile->writtenSize);
			SetLastError(ERROR_SUCCESS);
			DEBUG_LOG_DETOURED(L"GetFileSizeEx", L"%llu (%ls) (Size:%llu) -> 1", uintptr_t(hFile), HandleToName(hFile), fi.memoryFile->writtenSize);
			return 1;
		}

		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}
	DEBUG_LOG_TRUE(L"GetFileSizeEx", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	return True_GetFileSizeEx(trueHandle, lpFileSize); // This ends up in Detoured_NtQueryInformationFile
}

DWORD Detoured_SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	DETOURED_CALL(SetFilePointer);

	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		FileInfo& fi = *dh.fileObject->fileInfo;
		if (fi.memoryFile || fi.isFileMap)
		{
			LARGE_INTEGER liDistanceToMove;
			liDistanceToMove.LowPart = 0;
			liDistanceToMove.HighPart = lpDistanceToMoveHigh ? *lpDistanceToMoveHigh : 0;
			liDistanceToMove.QuadPart += lDistanceToMove;

			if (dwMoveMethod == FILE_BEGIN)
				dh.pos = liDistanceToMove.QuadPart;
			else if (dwMoveMethod == FILE_CURRENT)
				dh.pos += liDistanceToMove.QuadPart;
			else if (dwMoveMethod == FILE_END)
			{
				u64 size = 0;
				if (fi.memoryFile)
					size = fi.memoryFile->writtenSize;
				else
				{
					//UBA_ASSERT(fi.fileMapMemEnd);
					size = fi.size;// fileMapMemEnd - fi.fileMapMem;
				}
				dh.pos = Max(0ll, (LONGLONG)size + liDistanceToMove.QuadPart);
			}
			DEBUG_LOG_DETOURED(L"SetFilePointer", L"%llu %lli %u (%ls) -> %u", uintptr_t(hFile), liDistanceToMove.QuadPart, dwMoveMethod, HandleToName(hFile), DWORD(dh.pos));
			SetLastError(ERROR_SUCCESS);
			return DWORD(dh.pos);
		}

		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}

	DEBUG_LOG_TRUE(L"SetFilePointer", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	return True_SetFilePointer(trueHandle, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}

DWORD Detoured_SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod)
{
	DETOURED_CALL(SetFilePointerEx);
	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		DetouredHandle& dh = asDetouredHandle(hFile);
		if (dh.type == HandleType_Std)
		{
			if (lpNewFilePointer)
				*lpNewFilePointer = ToLargeInteger(0);
			// TODO: What should we do with this?
			SetLastError(ERROR_SUCCESS);
			DEBUG_LOG_DETOURED(L"SetFilePointerEx", L"%llu %lli %u (%ls) -> Success", uintptr_t(hFile), liDistanceToMove.QuadPart, dwMoveMethod, HandleToName(hFile));
			return TRUE;
		}

		FileInfo& fi = *dh.fileObject->fileInfo;
		if (fi.memoryFile || fi.isFileMap)
		{
			if (dwMoveMethod == FILE_BEGIN)
				dh.pos = liDistanceToMove.QuadPart;
			else if (dwMoveMethod == FILE_CURRENT)
				dh.pos += liDistanceToMove.QuadPart;
			else if (dwMoveMethod == FILE_END)
			{
				u64 size = 0;
				if (fi.memoryFile)
					size = fi.memoryFile->writtenSize;
				else if (fi.fileMapMem)
					size = fi.fileMapMemEnd - fi.fileMapMem;
				else
					size = fi.size;
				dh.pos = Max(0ll, (LONGLONG)size + liDistanceToMove.QuadPart);
			}
			if (lpNewFilePointer)
				*lpNewFilePointer = ToLargeInteger(dh.pos);
			SetLastError(ERROR_SUCCESS);
			DEBUG_LOG_DETOURED(L"SetFilePointerEx", L"%llu %lli %u (%ls) -> Success", uintptr_t(hFile), liDistanceToMove.QuadPart, dwMoveMethod, HandleToName(hFile));
			return TRUE;
		}
		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}
	DEBUG_LOG_TRUE(L"SetFilePointerEx", L"%llu %lli %u (%ls)", uintptr_t(hFile), liDistanceToMove.QuadPart, dwMoveMethod, HandleToName(hFile));
	return True_SetFilePointerEx(trueHandle, liDistanceToMove, lpNewFilePointer, dwMoveMethod); // This ends up in NtSetInformationFile
}

BOOL Detoured_SetEndOfFile(HANDLE hFile)
{
	DETOURED_CALL(SetEndOfFile);
	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		FileInfo& fi = *dh.fileObject->fileInfo;
		if (MemoryFile* mf = fi.memoryFile)
		{
			DEBUG_LOG_DETOURED(L"SetEndOfFile (MEMORY)", L"%llu (%ls) -> Success", uintptr_t(hFile), HandleToName(hFile));
			mf->writtenSize = dh.pos;
			mf->isReported = false;
			mf->EnsureCommited(dh, mf->writtenSize);
			SetLastError(ERROR_SUCCESS);
			return true;
		}
		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}

	BOOL res = True_SetEndOfFile(trueHandle);
	DEBUG_LOG_TRUE(L"SetEndOfFile", L"%llu (%ls) -> %ls", uintptr_t(hFile), HandleToName(hFile), ToString(res));
	return res;
}

BOOL Detoured_SetFileTime(HANDLE hFile, const FILETIME* lpCreationTime, const FILETIME* lpLastAccessTime, const FILETIME* lpLastWriteTime)
{
	DETOURED_CALL(SetFileTime);
	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		if (!lpCreationTime && !lpLastWriteTime)
		{
			DEBUG_LOG_DETOURED(L"SetFileTime", L"%llu IGNORE (%ls)", uintptr_t(hFile), HandleToName(hFile));
			return TRUE;
		}
		DetouredHandle& dh = asDetouredHandle(hFile);
		trueHandle = dh.trueHandle;
		UBA_ASSERTF(trueHandle != INVALID_HANDLE_VALUE, L"Want to SetFileTime on %ls which has no true file handle set", HandleToName(hFile));
	}
	DEBUG_LOG_TRUE(L"SetFileTime", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	return True_SetFileTime(trueHandle, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
}

BOOL Detoured_GetFileTime(HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime)
{
	DETOURED_CALL(GetFileTime);
	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		DetouredHandle& dh = asDetouredHandle(hFile);
		u32 entryOffset = dh.dirTableOffset;
		if (entryOffset != ~u32(0))
		{
			DirectoryTable::EntryInformation entryInfo;
			g_directoryTable.GetEntryInformation(entryInfo, entryOffset);
			if (lpLastWriteTime)
				*(u64*)lpLastWriteTime = entryInfo.lastWrite;
			if (lpCreationTime)
				*(u64*)lpCreationTime = entryInfo.lastWrite;
			if (lpLastAccessTime)
				*(u64*)lpLastAccessTime = entryInfo.lastWrite;
			//UBA_ASSERT(!lpLastAccessTime);
			DEBUG_LOG_DETOURED(L"GetFileTime", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
			return TRUE;
		}
		trueHandle = asDetouredHandle(hFile).trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}
	DEBUG_LOG_TRUE(L"GetFileTime", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	return True_GetFileTime(trueHandle, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
}

DWORD Detoured_GetFileType(HANDLE hFile)
{
	DETOURED_CALL(GetFileType);
	if (isDetouredHandle(hFile))
	{
		DetouredHandle& dh = asDetouredHandle(hFile);
		SetLastError(ERROR_SUCCESS);
		if (dh.type == HandleType_Std)
			return FILE_TYPE_CHAR;
		UBA_ASSERT(dh.type == HandleType_File);
		DEBUG_LOG_DETOURED(L"GetFileType", L"%llu (%ls) -> %u", uintptr_t(hFile), HandleToName(hFile), FILE_TYPE_DISK);
		return FILE_TYPE_DISK;
	}
	if (isListDirectoryHandle(hFile))
	{
		DEBUG_LOG_DETOURED(L"GetFileType", L"%llu (%ls) -> %u", uintptr_t(hFile), HandleToName(hFile), FILE_TYPE_DISK);
		SetLastError(ERROR_SUCCESS);
		return FILE_TYPE_DISK;
	}
	else if (hFile == PseudoHandle)
	{
		DEBUG_LOG_DETOURED(L"GetFileType", L"PseudoHandle -> FILE_TYPE_CHAR");
		SetLastError(ERROR_SUCCESS);
		return FILE_TYPE_CHAR;
	}

	DEBUG_LOG_TRUE(L"GetFileType", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	return True_GetFileType(hFile); // Calling NtQueryVolumeInformationFile
}

BOOL Shared_GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation, LPCWSTR originalName)
{
	const wchar_t* fixedFileName = lpFileName;
	if (StartsWith(fixedFileName, L"\\\\?\\"))
		fixedFileName += 4;

	if (!CanDetour(fixedFileName))
	{
		DEBUG_LOG_TRUE(L"GetFileAttributesExW", L"(%ls)", lpFileName);
		return True_GetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
	}

	FileAttributes attr;
	const wchar_t* realName = Shared_GetFileAttributes(attr, fixedFileName);

	if (!attr.useCache)
	{
		DEBUG_LOG_TRUE(L"GetFileAttributesExW", L"(%ls)", lpFileName);
		return True_GetFileAttributesExW(realName, fInfoLevelId, lpFileInformation);
	}

	SetLastError(attr.lastError);

	memcpy(lpFileInformation, &attr.data, sizeof(WIN32_FILE_ATTRIBUTE_DATA));

	DEBUG_LOG_DETOURED(L"GetFileAttributesExW", L"(%ls) -> %u", lpFileName, attr.exists);
	return attr.exists;
}

BOOL Detoured_GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
{
	DETOURED_CALL(GetFileAttributesExW);
	if (t_disallowDetour != 0 || !lpFileName || wcschr(lpFileName, ',') != nullptr || Contains(lpFileName, L"::")) // Some weird .net path used by dotnet.exe ... ignore for now!
	{
		UBA_ASSERT(!g_runningRemote);
		DEBUG_LOG_TRUE(L"GetFileAttributesExW", L"(%ls)", lpFileName);
		return True_GetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
	}

	StringBuffer<> fixedName;
	FixPath(fixedName, lpFileName);

	if (!g_rules->CanExist(fixedName.data))
	{
		SetLastError(ERROR_FILE_NOT_FOUND);
		return FALSE;
	}

	return Shared_GetFileAttributesExW(fixedName.data, fInfoLevelId, lpFileInformation, lpFileName);
}

DWORD Detoured_GetFileAttributesW(LPCWSTR lpFileName)
{
	DETOURED_CALL(GetFileAttributesW);
	if (t_disallowDetour != 0 || Equals(lpFileName, L"nul"))
	{
		DWORD res = True_GetFileAttributesW(lpFileName);
		DEBUG_LOG_TRUE(L"GetFileAttributesW", L"(NODETOUR) (%ls) -> %u", lpFileName, res);
		return res;
	}

	StringBuffer<> fixedPath;
	if (!FixPath(fixedPath, lpFileName))
		return INVALID_FILE_ATTRIBUTES;

	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!Shared_GetFileAttributesExW(fixedPath.data, GetFileExInfoStandard, &data, lpFileName))
		return INVALID_FILE_ATTRIBUTES;

	return data.dwFileAttributes;
}

BOOL Detoured_SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes)
{
	DETOURED_CALL(SetFileAttributesW);
	if (KeepInMemory(lpFileName, u32(wcslen(lpFileName))))
	{
		DEBUG_LOG_DETOURED(L"SetFileAttributesW", L"(%ls) %u", lpFileName, dwFileAttributes);
		SetLastError(ERROR_SUCCESS);
		return true;
	}
	DEBUG_LOG_TRUE(L"SetFileAttributesW", L"(%ls) %u", lpFileName, dwFileAttributes);
	return True_SetFileAttributesW(lpFileName, dwFileAttributes);
}

DWORD Detoured_GetLongPathNameW(LPCWSTR lpszShortPath, LPWSTR lpszLongPath, DWORD cchBuffer)
{
	DETOURED_CALL(GetLongPathNameW);

	if (wcsncmp(lpszShortPath, L"\\\\?\\", 4) == 0)
		lpszShortPath += 4;

	// TODO: Add support for ~ and "\\?\"
	if (!wcschr(lpszShortPath, '?'))
	{
		StringBuffer<> fixedName;
		FixPath(fixedName, lpszShortPath);

		DEBUG_LOG_DETOURED(L"GetLongPathNameW", L"(%ls)", lpszShortPath);
		WIN32_FILE_ATTRIBUTE_DATA data;
		bool success = Shared_GetFileAttributesExW(fixedName.data, GetFileExInfoStandard, &data, lpszShortPath);

		DWORD res = 0;
		if (success)
		{
			res = fixedName.count;
			memcpy(lpszLongPath, fixedName.data, res * 2 + 2);
		}

#if UBA_DEBUG_VALIDATE
		if (g_validateFileAccess)
		{
			if (!wcschr(lpszShortPath, '~') && !wcschr(lpszShortPath, '?'))
			{
				wchar_t temp[MaxPath];
				UBA_ASSERT(cchBuffer <= sizeof_array(temp));
				SuppressDetourScope _;
				DWORD res2 = True_GetLongPathNameW(lpszShortPath, temp, cchBuffer); (void)res2;
				UBA_ASSERT(res == res2);
			}
		}
#endif

		if (!success)
			SetLastError(ERROR_FILE_NOT_FOUND);
		return res;
	}

	return Local_GetLongPathNameW(lpszShortPath, lpszLongPath, cchBuffer);
}

DWORD Detoured_GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR* lpFilePart)
{
	DETOURED_CALL(GetFullPathNameW);
	if (g_runningRemote)
	{
		StringBuffer<> temp;
		FixPath(temp, lpFileName);
		u64 requiredSize = temp.count + 1;
		if (nBufferLength < requiredSize)
			return DWORD(requiredSize);
		memcpy(lpBuffer, temp.data, requiredSize * 2);
		if (lpFilePart)
			*lpFilePart = wcsrchr(lpBuffer, '\\') + 1;
		auto res = DWORD(temp.count);
		DEBUG_LOG_DETOURED(L"GetFullPathNameW", L"%ls -> %u", temp.data, res);
		SetLastError(ERROR_SUCCESS);
		return res;
	}

	auto res = True_GetFullPathNameW(lpFileName, nBufferLength, lpBuffer, lpFilePart);
	DEBUG_LOG_TRUE(L"GetFullPathNameW", L"%ls -> %u", lpFileName, res);
	return res;
}

BOOL Detoured_GetVolumePathNameW(LPCWSTR lpszFileName, LPWSTR lpszVolumePathName, DWORD cchBufferLength)
{
	DETOURED_CALL(GetVolumePathNameW);
	//if (lpszFileName[1] == ':')
	//{
	//	memcpy(lpszVolumePathName, lpszFileName, 6);
	//	lpszVolumePathName[3] = 0;
	//	return TRUE;
	//}

	if (g_runningRemote)
	{
		UBA_ASSERT(cchBufferLength > 3);
		memcpy(lpszVolumePathName, g_virtualWorkingDir.data, 6);
		lpszVolumePathName[3] = 0;
		DEBUG_LOG_DETOURED(L"GetVolumePathNameW", L"(%ls) -> %s", lpszFileName, lpszVolumePathName);
		SetLastError(ERROR_SUCCESS);
		return TRUE;
	}

	DEBUG_LOG_TRUE(L"GetVolumePathNameW", L"(%ls)", lpszFileName);
	SuppressCreateFileDetourScope cfs;
	auto res = True_GetVolumePathNameW(lpszFileName, lpszVolumePathName, cchBufferLength);
	return res;
}

DWORD Shared_GetModuleFileNameW(HMODULE hModule, const wchar_t* moduleName, u32 moduleNameLen, LPWSTR lpFilename, DWORD nSize)
{
	if (nSize <= moduleNameLen)
	{
		if (nSize)
		{
			memcpy(lpFilename, moduleName, nSize * 2);
			lpFilename[nSize - 1] = 0;
		}
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		DEBUG_LOG_DETOURED(L"GetModuleFileNameW", L"%llu  %u INSUFFICIENT BUFFER (%ls) -> %u", uintptr_t(hModule), nSize, moduleName, moduleNameLen + 1);
		return nSize;
	}

	memcpy(lpFilename, moduleName, moduleNameLen * 2);
	lpFilename[moduleNameLen] = 0;
	DEBUG_LOG_DETOURED(L"GetModuleFileNameW", L"%llu  %u (%ls) -> %u", uintptr_t(hModule), nSize, lpFilename, moduleNameLen);
	SetLastError(ERROR_SUCCESS);
	return moduleNameLen;
}

DWORD Detoured_GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize)
{
	DETOURED_CALL(GetModuleFileNameW);

	// If null we use the virtual application name
	if (hModule == NULL)
		return Shared_GetModuleFileNameW(hModule, g_virtualApplication.data, u32(g_virtualApplication.count), lpFilename, nSize);

	{
		// Check if there are any stored paths from dynamically loaded dlls
		SCOPED_READ_LOCK(g_loadedModulesLock, lock);
		auto findIt = g_loadedModules.find(hModule);
		if (findIt != g_loadedModules.end())
			return Shared_GetModuleFileNameW(hModule, findIt->second.c_str(), u32(findIt->second.size()), lpFilename, nSize);
	}

	if (!g_runningRemote)
		return True_GetModuleFileNameW(hModule, lpFilename, nSize);

	wchar_t moduleName[350];
	DWORD res = True_GetModuleFileNameW(hModule, moduleName, sizeof_array(moduleName));
	if (res == 0)
		return res;
	UBA_ASSERT(GetLastError() != ERROR_INSUFFICIENT_BUFFER);

	// This could be dlls that are loaded early one so might not exist in g_loadedModules
	// TODO: These could be wrong.. since the files could have been copied from different directories into the remote exedir
	if (!StartsWith(moduleName, g_exeDir.data))
		return Shared_GetModuleFileNameW(hModule, moduleName, res, lpFilename, nSize);

	StringBuffer<350> fileName;
	fileName.Append(g_virtualApplicationDir);
	fileName.Append(moduleName + g_exeDir.count);
	return Shared_GetModuleFileNameW(hModule, fileName.data, u32(fileName.count), lpFilename, nSize);
}

DWORD Detoured_GetModuleFileNameExW(HANDLE hProcess, HMODULE hModule, LPWSTR lpFilename, DWORD nSize)
{
	if (hProcess != (HANDLE)-1)
	{
		UBA_ASSERT(!g_runningRemote); // Not implemented
		return True_GetModuleFileNameExW(hProcess, hModule, lpFilename, nSize);
	}
	return Detoured_GetModuleFileNameW(hModule, lpFilename, nSize);
}

BOOL Detoured_CopyFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, LPPROGRESS_ROUTINE lpProgressRoutine, LPVOID lpData, LPBOOL pbCancel, DWORD dwCopyFlags)
{
	DETOURED_CALL(CopyFileExW);

	StringBuffer<> fromName;
	FixPath(fromName, lpExistingFileName);
	StringKey fromKey = ToStringKeyLower(fromName);

	StringBuffer<> toName;
	FixPath(toName, lpNewFileName);
	StringKey toKey = ToStringKeyLower(toName);

	StringBuffer<> newFromName;
	StringBuffer<> newToName;
	u32 closeId;
	u32 lastError;
	u32 directoryTableSize;
	{
		TimerScope ts(g_stats.copyFile);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_CopyFile);
		writer.WriteStringKey(fromKey);
		writer.WriteString(fromName);
		writer.WriteStringKey(toKey);
		writer.WriteString(toName);
		writer.Flush();
		BinaryReader reader;
		reader.ReadString(newFromName);
		reader.ReadString(newToName);
		closeId = reader.ReadU32();
		lastError = reader.ReadU32();
		directoryTableSize = reader.ReadU32();
	}

	if (closeId == ~0u) // Copy was made server side
	{
		g_directoryTable.ParseDirectoryTable(directoryTableSize);
		UBA_ASSERT(g_runningRemote);
		SetLastError(lastError);
		return lastError == ERROR_SUCCESS;
	}

	// TODO: This copy should probably be moved to session process instead.. to handle failing to copy better

	bool res;
	{
		SuppressCreateFileDetourScope cfs;
		res = True_CopyFileExW(newFromName.data, newToName.data, lpProgressRoutine, lpData, pbCancel, dwCopyFlags);
	}
	DEBUG_LOG_TRUE(L"CopyFileExW", L"%ls to %ls  (%ls to %ls) -> %ls", lpExistingFileName, lpNewFileName, newFromName.data, newToName.data, ToString(res));

	// We need to report the new file that has been added (and we must do it _after_ it has been copied
	if (!closeId)
		return res;

	bool deleteOnClose = res == false; // If failing to copy we set deleteOnClose
	Rpc_UpdateCloseHandle(newToName.data, closeId, deleteOnClose, L"", 0, 0, true);

	return res;
}

BOOL Detoured_CopyFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists)
{
	DETOURED_CALL(CopyFileW);
	DEBUG_LOG_TRUE(L"CopyFileW", L"");
	return Detoured_CopyFileExW(lpExistingFileName, lpNewFileName, (LPPROGRESS_ROUTINE)NULL, (LPVOID)NULL, (LPBOOL)NULL, bFailIfExists ? (DWORD)COPY_FILE_FAIL_IF_EXISTS : 0);
}


BOOL Detoured_CreateHardLinkW(LPCWSTR lpFileName, LPCWSTR lpExistingFileName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	DETOURED_CALL(CreateHardLinkW);

	// TODO: Merge this code with CopyFileEx which is identical

	StringBuffer<> fromName;
	FixPath(fromName, lpExistingFileName);
	StringKey fromKey = ToStringKeyLower(fromName);

	StringBuffer<> toName;
	FixPath(toName, lpFileName);
	StringKey toKey = ToStringKeyLower(toName);

	StringBuffer<> newFromName;
	StringBuffer<> newToName;
	u32 closeId;
	{
		TimerScope ts(g_stats.copyFile);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_CopyFile);
		writer.WriteStringKey(fromKey);
		writer.WriteString(fromName);
		writer.WriteStringKey(toKey);
		writer.WriteString(toName);
		writer.Flush();
		BinaryReader reader;
		reader.ReadString(newFromName);
		reader.ReadString(newToName);
		closeId = reader.ReadU32();
	}

	bool res;
	{
		SuppressCreateFileDetourScope cfs;
		res = True_CreateHardLinkW(lpFileName, lpExistingFileName, lpSecurityAttributes);
	}
	DEBUG_LOG_TRUE(L"CreateHardLinkW", L"%ls to %ls  (%ls to %ls) -> %ls", lpExistingFileName, lpFileName, newFromName.data, newToName.data, ToString(res));

	// We need to report the new file that has been added (and we must do it _after_ it has been copied
	if (closeId)
		Rpc_UpdateCloseHandle(newToName.data, closeId, false, L"", 0, 0, true);

	return res;
}

BOOL Detoured_DeleteFileW(LPCWSTR lpFileName)
{
	DETOURED_CALL(DeleteFileW);
	LPCWSTR original = lpFileName;

	StringBuffer<> fixedName;
	FixPath(fixedName, lpFileName);

	StringBuffer<> fixedNameLower(fixedName);
	fixedNameLower.MakeLower();

	if (!CanDetour(fixedNameLower.data))
	{
		DEBUG_LOG_TRUE(L"DeleteFileW", L"(%ls)", original);
		return True_DeleteFileW(original);
	}

	if (KeepInMemory(fixedNameLower.data, fixedNameLower.count))
	{
		DEBUG_LOG_DETOURED(L"DeleteFileW", L"(INMEMORY) (%ls) -> Success", lpFileName);
		SetLastError(ERROR_SUCCESS);
		return TRUE;
	}

	StringKey fileNameKey = ToStringKey(fixedNameLower);

	u32 directoryTableSize;
	bool result;
	u32 errorCode;
	{
		u32 closeId = 0;
		TimerScope ts(g_stats.deleteFile);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_DeleteFile);
		writer.WriteString(fixedName);
		writer.WriteStringKey(fileNameKey);
		writer.WriteU32(closeId);
		writer.Flush();
		BinaryReader reader;
		result = reader.ReadBool();
		errorCode = reader.ReadU32();
		directoryTableSize = reader.ReadU32();
		pcs.Leave();
		DEBUG_LOG_PIPE(L"DeleteFile", L"%ls", lpFileName);
	}
	DEBUG_LOG_DETOURED(L"DeleteFileW", L"(%ls) -> %ls", lpFileName, ToString(result));

	g_directoryTable.ParseDirectoryTable(directoryTableSize);
	g_mappedFileTable.SetDeleted(fileNameKey, lpFileName, true);
	SetLastError(errorCode);
	return result;
}

bool Shared_MoveFile(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags)
{
	DETOURED_CALL(MoveFileExW);
	StringBuffer<> source;
	FixPath(source, lpExistingFileName);

	StringKey sourceKey = ToStringKeyLower(source);

	if (KeepInMemory(source.data, source.count))
	{
		SCOPED_WRITE_LOCK(g_mappedFileTable.m_lookupLock, lock);
		auto it = g_mappedFileTable.m_lookup.find(sourceKey);
		UBA_ASSERTF(it != g_mappedFileTable.m_lookup.end(), L"Can't find %ls", source.data);
		FileInfo& sourceInfo = it->second;
		lock.Leave();

		StringBuffer<> dest;
		FixPath(dest, lpNewFileName);

		if (IsOutputFile(dest.data, dest.count, GENERIC_WRITE))
		{
			sourceInfo.deleted = true;
			UBA_ASSERT(!sourceInfo.memoryFile->isLocalOnly);
			dest.MakeLower();
			StringKey destKey = ToStringKey(dest);
			SCOPED_WRITE_LOCK(g_mappedFileTable.m_lookupLock, lock2);
			auto insres = g_mappedFileTable.m_lookup.try_emplace(destKey);
			lock2.Leave();
			FileInfo& destInfo = insres.first->second;
			UBA_ASSERT(!insres.second); // This is here just to get a chance to investigate this scenario.. might work
			UBA_ASSERTF(!destInfo.trueFileMapHandle && (!destInfo.memoryFile || g_rules->IsThrowAway(dest.data, dest.count)), TC("Moving file %s to %s that is an output file that is not a memory file is not supported"), source.data, lpNewFileName);
			destInfo.memoryFile = sourceInfo.memoryFile;
			sourceInfo.memoryFile = nullptr;
			DEBUG_LOG_DETOURED(L"MoveFileExW", L"(memfile->memfile) %ls to %ls -> Success", lpExistingFileName, lpNewFileName);
			SetLastError(ERROR_SUCCESS);
			return true;
		}

		UBA_ASSERT(!KeepInMemory(dest.data, dest.count));

		DEBUG_LOG_DETOURED(L"MoveFileExW", L"(memfile->file) %ls to %ls", lpExistingFileName, lpNewFileName);

		HANDLE h = CreateFile(lpNewFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		auto cg = MakeGuard([&]() { CloseHandle(h); });
		UBA_ASSERT(sourceInfo.memoryFile->writtenSize < ~0u);
		u32 toWrite = u32(sourceInfo.memoryFile->writtenSize);
		DWORD written = 0;
		if (!WriteFile(h, sourceInfo.memoryFile->baseAddress, toWrite, &written, NULL))
			return false;

		sourceInfo.deleted = true;
		return written == toWrite;
	}

	StringBuffer<> dest;
	FixPath(dest, lpNewFileName);
	StringKey destKey = ToStringKeyLower(dest);

	u32 directoryTableSize;
	u32 errorCode;
	bool result;
	{
		TimerScope ts(g_stats.moveFile);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_MoveFile);
		writer.WriteStringKey(sourceKey);
		writer.WriteString(source);
		writer.WriteStringKey(destKey);
		writer.WriteString(dest);
		writer.WriteU32(dwFlags);
		writer.Flush();
		BinaryReader reader;
		result = reader.ReadBool();
		errorCode = reader.ReadU32();
		directoryTableSize = reader.ReadU32();
		pcs.Leave();
		DEBUG_LOG_PIPE(L"MoveFile", L"%ls to %ls", lpExistingFileName, lpNewFileName);
	}

	DEBUG_LOG_DETOURED(L"MoveFileExW", L"(PIPE) (%ls to %ls) -> %ls", lpExistingFileName, lpNewFileName, ToString(result));

	g_directoryTable.ParseDirectoryTable(directoryTableSize);
	g_mappedFileTable.SetDeleted(sourceKey, source.data, true);
	g_mappedFileTable.SetDeleted(destKey, dest.data, false);
	SetLastError(errorCode);

	return result;
}

BOOL Detoured_MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags)
{
	//UBA_ASSERT(!Contains(lpNewFileName, L"E:\\dev\\fn\\Engine\\Source\\Programs\\Shared\\EpicGames.UHT\\obj\\project.assets.json"));
	return Shared_MoveFile(lpExistingFileName, lpNewFileName, dwFlags);
}

// MoveFileW ends up here
BOOL Detoured_MoveFileWithProgressW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, LPPROGRESS_ROUTINE lpProgressRoutine, LPVOID lpData, DWORD dwFlags)
{
	DETOURED_CALL(MoveFileWithProgressW);
	StringBuffer<> source;
	FixPath(source, lpExistingFileName);

	return Shared_MoveFile(lpExistingFileName, lpNewFileName, dwFlags);

	//UBA_ASSERT(!g_runningRemote);
	//DEBUG_LOG_TRUE(L"MoveFileWithProgressW", L"%ls to %ls", lpExistingFileName, lpNewFileName);
	//return True_MoveFileWithProgressW(lpExistingFileName, lpNewFileName, lpProgressRoutine, lpData, dwFlags);
}

bool Shared_GetNextFile(WIN32_FIND_DATA& outData, ListDirectoryHandle& listHandle)
{
	while (true)
	{
		if (listHandle.it == listHandle.fileTableOffsets.size())
			return false;

		constexpr u32 maxLen = sizeof_array(outData.cFileName);

		if (listHandle.it < 0)
		{
			if (listHandle.it == -2)
				wcscpy_s(outData.cFileName, maxLen, L".");
			else
				wcscpy_s(outData.cFileName, maxLen, L"..");
			outData.nFileSizeHigh = 0;
			outData.nFileSizeLow = 0;
			outData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
			outData.cAlternateFileName[0] = 0;
			(u64&)outData.ftLastWriteTime = 0;
			(u64&)outData.ftCreationTime = 0;
			(u64&)outData.ftLastAccessTime = 0;

			++listHandle.it;
			return true;
		}

		DirectoryTable::EntryInformation info;
		u32 fileTableOffset = listHandle.fileTableOffsets[listHandle.it++];
		g_directoryTable.GetEntryInformation(info, fileTableOffset, outData.cFileName, maxLen);
		if (info.attributes == 0) // File was deleted
			continue;

		LARGE_INTEGER li = ToLargeInteger(info.size);
		outData.nFileSizeHigh = li.HighPart;
		outData.nFileSizeLow = li.LowPart;
		outData.dwFileAttributes = info.attributes;
		outData.cAlternateFileName[0] = 0;
		(u64&)outData.ftLastWriteTime = info.lastWrite;

		// TODO: These are wrong.. 
		(u64&)outData.ftCreationTime = info.lastWrite;
		(u64&)outData.ftLastAccessTime = info.lastWrite;
		return true;
	}
}

__forceinline
HANDLE Local_FindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags, const wchar_t* funcName)
{
	DEBUG_LOG_TRUE(funcName, L"(NODETOUR) (%ls)", lpFileName);
	SuppressCreateFileDetourScope s; // Needed for cmd.exe copy right now.. NtCreate's flags are set the same as directory search but the first file is not a directory.
	auto res = True_FindFirstFileExW(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
	UBA_ASSERT(!isDetouredHandle(res) && !isListDirectoryHandle(res));
	return res;
}


__forceinline HANDLE Shared_FindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags, const wchar_t* funcName)
{
	if (t_disallowDetour != 0 || Equals(lpFileName, L"nul") || !g_allowFindFileDetour)
	{
		return Local_FindFirstFileExW(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags, funcName);
	}

	StringBuffer<> lowerName;
	FixPath(lowerName, lpFileName);
	lowerName.MakeLower();
	const wchar_t* buf = lowerName.data;

	if (wcsncmp(buf, L"\\\\?\\", 4) == 0)
		buf += 4;

	if (wcsncmp(buf, g_systemTemp.data, g_systemTemp.count) == 0 || wcsncmp(buf, g_systemRoot.data, g_systemRoot.count) == 0)
		return Local_FindFirstFileExW(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags, funcName);

	wchar_t* fileName = const_cast<wchar_t*>(buf); // Not beautiful but We know this is a different buffer.
	wchar_t* lastBackslash = wcsrchr(fileName, '\\');
	if (lastBackslash)
		fileName = lastBackslash + 1;

	UBA_ASSERT(lastBackslash);
	u64 bufChars = (lastBackslash - buf + 1);

	if (wcscmp(fileName, L"*") == 0 || wcscmp(fileName, L"*.*") == 0)
	{
		*fileName = 0;
	}

	// We must remove a slash at the end so it matches our cache entries
	if (bufChars > 3)
	{
		wchar_t* temp = (wchar_t*)buf;
		if (temp[bufChars - 1] == '\\')
			temp[--bufChars] = 0;
	}

	DirHash hash(buf, bufChars);

	SCOPED_WRITE_LOCK(g_directoryTable.m_lookupLock, lookLock);
	auto insres = g_directoryTable.m_lookup.try_emplace(hash.key, &g_memoryBlock);
	DirectoryTable::Directory& dir = insres.first->second;
	if (insres.second)
	{
		CHECK_PATH(buf);
		if (g_directoryTable.EntryExistsNoLock(hash.key, buf, bufChars) != DirectoryTable::Exists_No)
			Rpc_UpdateDirectory(hash.key, buf, bufChars, false);
	}
	bool exists = false;
	if (dir.tableOffset != InvalidTableOffset)
	{
		u32 entryOffset = dir.tableOffset | 0x80000000;
		DirectoryTable::EntryInformation entryInfo;
		g_directoryTable.GetEntryInformation(entryInfo, entryOffset);
		exists = entryInfo.attributes != 0;
	}

#if UBA_DEBUG_VALIDATE
	HANDLE validateHandle = INVALID_HANDLE_VALUE;
	/*
	if (g_validateFileAccess)
	{
		NTSTATUS res = exists ? 0 : -1;
		IO_STATUS_BLOCK IoStatusBlock2;
		NTSTATUS res2 = True_NtCreateFile(&validateHandle, DesiredAccess, ObjectAttributes, &IoStatusBlock2, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);
		UBA_ASSERT(res < 0 && res2 < 0 || res >= 0 && res2 >= 0);
	}
	*/
#endif

	if (!exists)
	{
		DEBUG_LOG_DETOURED(funcName, L"(%ls) -> NotFound", lpFileName);
		SetLastError(ERROR_FILE_NOT_FOUND);
		return INVALID_HANDLE_VALUE;
	}

	// TODO: Add support for more modes
	UBA_ASSERT(fInfoLevelId == FindExInfoBasic || fInfoLevelId == FindExInfoStandard);
	UBA_ASSERT(fSearchOp == FindExSearchNameMatch);
	UBA_ASSERT(lpSearchFilter == nullptr);
	//UBA_ASSERT(dwAdditionalFlags == 0);

	g_directoryTable.PopulateDirectory(hash.open, dir);



	auto listHandle = new ListDirectoryHandle{ hash.key, insres.first->second };

	if (!*fileName)
		listHandle->it = -2;
	else
		listHandle->it = 0;

	SCOPED_READ_LOCK(dir.lock, lock);
	listHandle->fileTableOffsets.resize(dir.files.size());
	u32 it = 0;
	for (auto& pair : dir.files)
		listHandle->fileTableOffsets[it++] = pair.second;
	lock.Leave();

	listHandle->wildcard = fileName;
#if UBA_DEBUG_VALIDATE
	if (g_validateFileAccess)
		listHandle->validateHandle = validateHandle;
#endif

	auto& data = *(WIN32_FIND_DATA*)lpFindFileData;
	while (true)
	{
		if (!Shared_GetNextFile(data, *listHandle))
		{
			DEBUG_LOG_DETOURED(funcName, L"(%ls) -> NotFound", lpFileName);
			delete listHandle;
			return INVALID_HANDLE_VALUE;
		}
		if (listHandle->wildcard.empty() || PathMatchSpecW(data.cFileName, listHandle->wildcard.c_str()))
			break;
	}

	HANDLE res = makeListDirectoryHandle(listHandle);
	DEBUG_LOG_DETOURED(funcName, L"(%ls) \"%ls\" -> %llu", lpFileName, data.cFileName, uintptr_t(res));
	return res;
}

HANDLE Detoured_FindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
	DETOURED_CALL(FindFirstFileExW);
	return Shared_FindFirstFileExW(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags, L"FindFirstFileExW");
}

HANDLE Detoured_FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
	DETOURED_CALL(FindFirstFileW);
	return Shared_FindFirstFileExW(lpFileName, FindExInfoStandard, lpFindFileData, FindExSearchNameMatch, NULL, 0, L"FindFirstFileW");
}

BOOL Detoured_FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
{
	DETOURED_CALL(FindNextFileW);
	if (isListDirectoryHandle(hFindFile))
	{
		auto& listHandle = asListDirectoryHandle(hFindFile);
		auto& data = *(WIN32_FIND_DATA*)lpFindFileData;
		while (true)
		{
			if (!Shared_GetNextFile(data, listHandle))
			{
				DEBUG_LOG_DETOURED(L"FindNextFileW", L"%llu (NOMORE) -> False", u64(hFindFile));
				SetLastError(ERROR_NO_MORE_FILES);
				return false;
			}
			if (listHandle.wildcard.empty() || PathMatchSpecW(data.cFileName, listHandle.wildcard.c_str()))
			{
				DEBUG_LOG_DETOURED(L"FindNextFileW", L"%llu (%ls) -> True", u64(hFindFile), data.cFileName);
				SetLastError(ERROR_SUCCESS);
				return true;
			}
		}
	}

	UBA_ASSERT(!isDetouredHandle(hFindFile));
	DEBUG_LOG_TRUE(L"FindNextFileW", L"%llu", uintptr_t(hFindFile));
	return True_FindNextFileW(hFindFile, lpFindFileData);
}

BOOL Detoured_FindClose(HANDLE handle)
{
	DETOURED_CALL(FindClose);
	if (isListDirectoryHandle(handle))
	{
		DEBUG_LOG_DETOURED(L"FindClose", L"%llu -> Success", uintptr_t(handle));
		delete& asListDirectoryHandle(handle);
		SetLastError(ERROR_SUCCESS);
		return true;
	}
	UBA_ASSERT(!isDetouredHandle(handle));
	BOOL res = True_FindClose(handle);
	DEBUG_LOG_TRUE(L"FindClose", L"%llu -> %ls", uintptr_t(handle), ToString(res));
	return res;
}

BOOL Detoured_GetFileInformationByHandleEx(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS fileInformationClass, LPVOID lpFileInformation, DWORD dwBufferSize)
{
	DETOURED_CALL(GetFileInformationByHandleEx);

	HANDLE trueHandle = hFile;

	u32 entryOffset = ~u32(0);

#if UBA_DEBUG_VALIDATE
	const wchar_t* originalName = nullptr;
#endif

	if (isDetouredHandle(hFile))
	{
		DetouredHandle& dh = asDetouredHandle(hFile);
		trueHandle = dh.trueHandle;
		entryOffset = dh.dirTableOffset;

		if (entryOffset == ~u32(0) && trueHandle == INVALID_HANDLE_VALUE)
		{
			MemoryFile* mf = dh.fileObject->fileInfo->memoryFile;
			UBA_ASSERTF(mf, L"GetFileInformationByHandleEx called on file %s which has no entry offset or real handle", HandleToName(hFile));

			DEBUG_LOG_DETOURED(L"GetFileInformationByHandleEx", L"(MEMORY) (%u) %llu (%ls)", fileInformationClass, uintptr_t(hFile), HandleToName(hFile));

			if (fileInformationClass == FileIdInfo)
			{
				auto& data = *(FILE_ID_INFO*)lpFileInformation;
				data.VolumeSerialNumber = mf->volumeSerial;
				u64* id = (u64*)&data.FileId;
				id[0] = 0;
				id[1] = mf->fileIndex;
				return TRUE;
			}
			else if (fileInformationClass == FileStandardInfo)
			{
				auto& data = *(FILE_STANDARD_INFO*)lpFileInformation;
				data.EndOfFile = ToLargeInteger(mf->writtenSize);
				data.AllocationSize = ToLargeInteger(mf->committedSize);
				data.DeletePending = dh.fileObject->deleteOnClose;
				data.NumberOfLinks = 1;
				data.Directory = false;
				return TRUE;
			}
			else
			{
				UBA_ASSERTF(!mf, L"GetFileInformationByHandleEx called for memory file using class %u which is not implemented (%s)", fileInformationClass, HandleToName(hFile));
			}
		}

#if UBA_DEBUG_VALIDATE
		originalName = dh.fileObject->fileInfo->originalName;
#endif
	}
	else if (isListDirectoryHandle(hFile))
	{
		auto& listHandle = asListDirectoryHandle(hFile);
		if (listHandle.dir.tableOffset != InvalidTableOffset)
			entryOffset = listHandle.dir.tableOffset | 0x80000000;
		else
			UBA_ASSERT(false);
		trueHandle = INVALID_HANDLE_VALUE;
	}

	if (entryOffset != ~u32(0))
	{
		DEBUG_LOG_DETOURED(L"GetFileInformationByHandleEx", L"(DIRTABLE) (%u) %llu (%ls)", fileInformationClass, uintptr_t(hFile), HandleToName(hFile));
		DirectoryTable::EntryInformation entryInfo;
		g_directoryTable.GetEntryInformation(entryInfo, entryOffset);
		if (fileInformationClass == FileBasicInfo)
		{
			auto& data = *(FILE_BASIC_INFO*)lpFileInformation;
			data.CreationTime = ToLargeInteger(entryInfo.lastWrite);
			data.LastAccessTime = ToLargeInteger(entryInfo.lastWrite);
			data.LastWriteTime = ToLargeInteger(entryInfo.lastWrite);
			data.ChangeTime = ToLargeInteger(entryInfo.lastWrite);
			data.FileAttributes = entryInfo.attributes;
			return TRUE;
		}
		else if (fileInformationClass == FileIdInfo)
		{
			auto& data = *(FILE_ID_INFO*)lpFileInformation;
			data.VolumeSerialNumber = entryInfo.volumeSerial;
			u64* id = (u64*)&data.FileId;
			id[0] = 0;
			id[1] = entryInfo.fileIndex;
			return TRUE;
		}
		else if (fileInformationClass == FileStandardInfo)
		{
			auto& data = *(FILE_STANDARD_INFO*)lpFileInformation;
			data.EndOfFile = ToLargeInteger(entryInfo.size);
			data.AllocationSize = ToLargeInteger(entryInfo.size);
			data.DeletePending = false;
			data.NumberOfLinks = 1;
			data.Directory = (entryInfo.attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;


#if UBA_DEBUG_VALIDATE
			if (g_validateFileAccess && originalName)
			{
				SuppressDetourScope _;
				WIN32_FILE_ATTRIBUTE_DATA validData;
				if (True_GetFileAttributesExW(originalName, GetFileExInfoStandard, &validData))
				{
					u64 size = ToLargeInteger(validData.nFileSizeHigh, validData.nFileSizeLow).QuadPart; (void)size;
					UBA_ASSERTF(u64(data.EndOfFile.QuadPart) == size, L"File size used: %llu Actual file size: %llu (%s)", data.EndOfFile.QuadPart, size, originalName);
				}
				else
				{
					Rpc_WriteLogf(L"FAILED TO GET FILE ATTRIBUTES %s", originalName);
				}
			}
#endif


			return TRUE;
		}
		else if (fileInformationClass == FileRemoteProtocolInfo)
		{
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
			/*
			auto& data = *(FILE_REMOTE_PROTOCOL_INFO*)lpFileInformation;

			SuppressCreateFileDetourScope s;
			HANDLE h = CreateFile(HandleToName(hFile), FILE_READ_ATTRIBUTES|FILE_LIST_DIRECTORY|FILE_WRITE_EA, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
			BOOL res = GetFileInformationByHandleEx(h, FileRemoteProtocolInfo, lpFileInformation, dwBufferSize);
			auto err = GetLastError();
			CloseHandle(h);
			return FALSE;
			*/
		}
		else
		{
			UBA_ASSERTF(trueHandle != INVALID_HANDLE_VALUE, L"GetFileInformationByHandleEx with class %u not Implemented (%ls)", fileInformationClass, HandleToName(hFile));
		}
	}
	DEBUG_LOG_TRUE(L"GetFileInformationByHandleEx", L"(%ls)", HandleToName(hFile));
	return True_GetFileInformationByHandleEx(trueHandle, fileInformationClass, lpFileInformation, dwBufferSize); /// calls GetFileInformationByHandleEx
}

BOOL Detoured_GetFileInformationByHandle(HANDLE hFile, LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
{
	DETOURED_CALL(GetFileInformationByHandle);

	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		u32 dirTableOffset = dh.dirTableOffset;

		UBA_ASSERT(dh.fileObject->fileInfo);
		FileInfo& fi = *dh.fileObject->fileInfo;

		if (dirTableOffset != ~u32(0))
		{
			DirectoryTable::EntryInformation entryInfo;
			g_directoryTable.GetEntryInformation(entryInfo, dirTableOffset);
			lpFileInformation->dwFileAttributes = entryInfo.attributes;
			(u64&)lpFileInformation->ftCreationTime = entryInfo.lastWrite;
			(u64&)lpFileInformation->ftLastAccessTime = entryInfo.lastWrite;
			(u64&)lpFileInformation->ftLastWriteTime = entryInfo.lastWrite;
			lpFileInformation->dwVolumeSerialNumber = entryInfo.volumeSerial;
			LARGE_INTEGER li = ToLargeInteger(entryInfo.fileIndex);
			lpFileInformation->nFileIndexHigh = li.HighPart;
			lpFileInformation->nFileIndexLow = li.LowPart;
			lpFileInformation->nNumberOfLinks = 1;//~u32(0); // TODO
			u64 fileSize = fi.size;
			if (fileSize == InvalidValue)
				fileSize = entryInfo.size;

#if UBA_DEBUG_VALIDATE
			if (g_validateFileAccess && !(entryInfo.attributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				SuppressDetourScope _;
				WIN32_FILE_ATTRIBUTE_DATA data;
				if (dh.trueHandle != INVALID_HANDLE_VALUE)
				{
					BY_HANDLE_FILE_INFORMATION bhfi;
					auto res2 = True_GetFileInformationByHandle(dh.trueHandle, &bhfi);
					UBA_ASSERT(res2 == TRUE);
					u64 size = ToLargeInteger(bhfi.nFileSizeHigh, bhfi.nFileSizeLow).QuadPart; (void)size;
					u64 fileIndex = ToLargeInteger(bhfi.nFileIndexHigh, bhfi.nFileIndexLow).QuadPart; (void)fileIndex;
					UBA_ASSERTF(fileSize == size, L"File size used: %llu Actual file size: %llu (%s)", fileSize, size, fi.originalName);
					//UBA_ASSERTF(entryInfo.attributes == bhfi.dwFileAttributes, L"Attributes used: 0x%x Actual: 0x%x (%s)", entryInfo.attributes, bhfi.dwFileAttributes, fi.originalName);
					UBA_ASSERTF(entryInfo.volumeSerial == bhfi.dwVolumeSerialNumber, L"VolumeSerial used: %u Actual: %u (%s)", entryInfo.volumeSerial, bhfi.dwVolumeSerialNumber, fi.originalName);
					UBA_ASSERTF(entryInfo.fileIndex == fileIndex, L"FileIndex used: %llu Actual: %llu (%s)", entryInfo.fileIndex, fileIndex, fi.originalName);
					UBA_ASSERTF(bhfi.nNumberOfLinks == 1, L"Links used: %llu Actual: %llu (%s)", 1, bhfi.nNumberOfLinks, fi.originalName);
				}
				else if (True_GetFileAttributesExW(fi.originalName, GetFileExInfoStandard, &data))
				{
					u64 size = ToLargeInteger(data.nFileSizeHigh, data.nFileSizeLow).QuadPart; (void)size;
					UBA_ASSERTF(fileSize == size, L"File size used: %llu Actual file size: %llu (%s)", fileSize, size, fi.originalName);
				}
				else
				{
					Rpc_WriteLogf(L"FAILED TO GET FILE ATTRIBUTES %s", fi.originalName);
				}
			}
#endif


			li = ToLargeInteger(fileSize);
			lpFileInformation->nFileSizeHigh = li.HighPart;
			lpFileInformation->nFileSizeLow = li.LowPart;
			DEBUG_LOG_DETOURED(L"GetFileInformationByHandle", L"(file) %llu (%ls) -> Success (size: %llu)", uintptr_t(hFile), HandleToName(hFile), fileSize);
			return TRUE;
		}

		if (MemoryFile* mf = fi.memoryFile)
		{
			DEBUG_LOG_DETOURED(L"GetFileInformationByHandle", L"(memoryfile) %llu (%ls) -> Success", uintptr_t(hFile), HandleToName(hFile));
			lpFileInformation->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
			(u64&)lpFileInformation->ftCreationTime = mf->fileTime;
			(u64&)lpFileInformation->ftLastAccessTime = mf->fileTime;
			(u64&)lpFileInformation->ftLastWriteTime = mf->fileTime;
			lpFileInformation->dwVolumeSerialNumber = mf->volumeSerial;
			LARGE_INTEGER li = ToLargeInteger(mf->fileIndex);
			lpFileInformation->nFileIndexHigh = li.HighPart;
			lpFileInformation->nFileIndexLow = li.LowPart;
			lpFileInformation->nNumberOfLinks = 1;//~u32(0); // TODO
			li = ToLargeInteger(fi.memoryFile->writtenSize);
			lpFileInformation->nFileSizeHigh = li.HighPart;
			lpFileInformation->nFileSizeLow = li.LowPart;
			return TRUE;
		}

		if (g_runningRemote)
		{
			StringBuffer<> fixedName;
			FixPath(fixedName, fi.originalName);

			FileAttributes attr;
			Shared_GetFileAttributes(attr, fixedName.data);

			if (attr.useCache)
			{
				if (!attr.exists)
				{
					// this could be a file that was created locally and is not propagated to directory table


					SetLastError(ERROR_FILE_NOT_FOUND);
					DEBUG_LOG_DETOURED(L"GetFileInformationByHandle", L"remote %llu (%ls) -> NotFound", uintptr_t(hFile), HandleToName(hFile));
					return false;
				}

				UBA_ASSERT(attr.fileIndex);

				LARGE_INTEGER li = ToLargeInteger(attr.fileIndex);
				/*
				#if UBA_DEBUG_VALIDATE
				if (g_validateFileAccess)
				{
					HANDLE h = True_CreateFileW(fileName, 0, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS, NULL);
					auto res = True_GetFileInformationByHandle(h, lpFileInformation);
					True_CloseHandle(h);

					UBA_ASSERT(attr.data.dwFileAttributes == lpFileInformation->dwFileAttributes);
					UBA_ASSERT(attr.volumeSerial == lpFileInformation->dwVolumeSerialNumber);
					UBA_ASSERT(li.HighPart == lpFileInformation->nFileIndexHigh);
					UBA_ASSERT(li.LowPart == lpFileInformation->nFileIndexLow);
					//return res;
				}
				#endif
				*/
				SetLastError(ERROR_SUCCESS);

				UBA_ASSERT(attr.volumeSerial);

				lpFileInformation->dwFileAttributes = attr.data.dwFileAttributes;
				lpFileInformation->ftCreationTime = attr.data.ftCreationTime;
				lpFileInformation->ftCreationTime = attr.data.ftCreationTime;
				lpFileInformation->ftLastAccessTime = attr.data.ftLastAccessTime;
				lpFileInformation->ftLastWriteTime = attr.data.ftLastWriteTime;
				lpFileInformation->dwVolumeSerialNumber = attr.volumeSerial;
				lpFileInformation->nFileIndexHigh = li.HighPart;
				lpFileInformation->nFileIndexLow = li.LowPart;
				lpFileInformation->nNumberOfLinks = 1;//~u32(0); // TODO
				lpFileInformation->nFileSizeHigh = attr.data.nFileSizeHigh;
				lpFileInformation->nFileSizeLow = attr.data.nFileSizeLow;
				DEBUG_LOG_DETOURED(L"GetFileInformationByHandle", L"remote %llu (%ls) -> Success", uintptr_t(hFile), HandleToName(hFile));
				return TRUE;
			}
		}
		UBA_ASSERT(dh.trueHandle != INVALID_HANDLE_VALUE);
		trueHandle = dh.trueHandle;
	}

	auto res = True_GetFileInformationByHandle(trueHandle, lpFileInformation); // Calls NtQueryInformationFile
	DEBUG_LOG_TRUE(L"GetFileInformationByHandle", L"%llu (%ls) -> %u", uintptr_t(hFile), HandleToName(hFile), res);
	return res;
}

BOOL Detoured_SetFileInformationByHandle(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass, LPVOID lpFileInformation, DWORD dwBufferSize)
{
	DETOURED_CALL(SetFileInformationByHandle);

	if (!isDetouredHandle(hFile))
	{
		DEBUG_LOG_TRUE(L"SetFileInformationByHandle", L"%llu (%u)", uintptr_t(hFile), FileInformationClass);
		return True_SetFileInformationByHandle(hFile, FileInformationClass, lpFileInformation, dwBufferSize);
	}

	auto& dh = asDetouredHandle(hFile);
	if (FileInformationClass == FileRenameInfo)
	{
		DEBUG_LOG_TRUE(L"SetFileInformationByHandle", L"%llu (FileRenameInfo)", uintptr_t(hFile));
		auto& info = *(FILE_RENAME_INFO*)lpFileInformation;
		StringBuffer<> newName;
		newName.Append(info.FileName, info.FileNameLength / 2);
		t_renameFileNewName = newName.data;
		bool res = True_SetFileInformationByHandle(hFile, FileInformationClass, lpFileInformation, dwBufferSize);
		t_renameFileNewName = nullptr;
		return res;
	}
	else if (FileInformationClass == FileDispositionInfo)
	{
		auto& info = *(FILE_DISPOSITION_INFO*)lpFileInformation;

		if (info.DeleteFileW)
		{
			DEBUG_LOG_DETOURED(L"SetFileInformationByHandle", L"File is set to be deleted on close (%ls)", HandleToName(hFile));
			dh.fileObject->deleteOnClose = true;
		}
		else if (dh.fileObject->deleteOnClose)
		{
			DEBUG_LOG_DETOURED(L"SetFileInformationByHandle", L"File is set to NOT be deleted on close (%ls)", HandleToName(hFile));
			dh.fileObject->deleteOnClose = false;
		}
		else
		{
			DEBUG_LOG_DETOURED(L"SetFileInformationByHandle", L"%llu (FileDispositionInfo %u)", uintptr_t(hFile), info.DeleteFileW);
		}

		if (dh.fileObject->fileInfo->memoryFile)
			return true;

		DEBUG_LOG_TRUE(L"SetFileInformationByHandle", L"%llu (FileDispositionInfo)", uintptr_t(hFile));
	}
	else
	{
		DEBUG_LOG_TRUE(L"SetFileInformationByHandle", L"%llu (%u)", uintptr_t(hFile), FileInformationClass);
	}

	return True_SetFileInformationByHandle(hFile, FileInformationClass, lpFileInformation, dwBufferSize);
}

HANDLE Detoured_CreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName)
{
	DETOURED_CALL(CreateFileMappingW);
	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		FileInfo& fi = *dh.fileObject->fileInfo;
		if (fi.memoryFile || fi.isFileMap)
		{
			auto mdh = new DetouredHandle(HandleType_FileMapping);
			if (fi.isFileMap)
			{
				// If protection levels are the same we can reuse the "built-in" file mapping
				UBA_ASSERTF((flProtect == PAGE_WRITECOPY ? PAGE_READONLY : flProtect) == fi.fileMapDesiredAccess, L"Code path not implemented (%ls)", HandleToName(hFile));
			}
			mdh->fileObject = dh.fileObject;
			if (MemoryFile* mf = fi.memoryFile)
			{
				LARGE_INTEGER li;
				li.HighPart = dwMaximumSizeHigh;
				li.LowPart = dwMaximumSizeLow;
				if (!(flProtect & MEM_RESERVE) && li.QuadPart)
				{
					mf->EnsureCommited(*mdh, li.QuadPart);
					if (!mf->writtenSize && (flProtect & PAGE_READWRITE)) // TODO: Maybe we should always set writtenSize?
						mf->writtenSize = li.QuadPart;
				}
			}
			InterlockedIncrement(&dh.fileObject->refCount);
			HANDLE res = makeDetouredHandle(mdh);
			DEBUG_LOG_DETOURED(L"CreateFileMappingW", L"(%ls) %llu %llu (%ls) -> Success", (fi.memoryFile ? L"MEMORYFILE" : L"FILEMAP"), uintptr_t(res), uintptr_t(hFile), HandleToName(hFile));
			SetLastError(ERROR_SUCCESS);
			return res;
		}
		UBA_ASSERT(dh.trueHandle != INVALID_HANDLE_VALUE);
		trueHandle = dh.trueHandle;
	}

	HANDLE mappingHandle = True_CreateFileMappingW(trueHandle, lpFileMappingAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
	if (!mappingHandle)
	{
		DEBUG_LOG_TRUE(L"CreateFileMappingW", L"%llu  (File %llu) (%ls) -> Error", uintptr_t(mappingHandle), uintptr_t(hFile), HandleToName(hFile));
		return NULL;
	}

	if (g_allowFileMappingDetour)
	{
		if (GetLastError() == ERROR_ALREADY_EXISTS)
			ToInvestigate(L"Mapping already exists");
		auto detouredHandle = new DetouredHandle(HandleType_FileMapping);
		detouredHandle->trueHandle = mappingHandle;
		mappingHandle = makeDetouredHandle(detouredHandle);
	}
	DEBUG_LOG_TRUE(L"CreateFileMappingW", L"%llu  (File %llu, Size: %llu) (%ls) -> %llu", uintptr_t(mappingHandle), uintptr_t(hFile), ToLargeInteger(dwMaximumSizeHigh, dwMaximumSizeLow).QuadPart, HandleToName(hFile), u64(mappingHandle));
	return mappingHandle;
}

HANDLE Detoured_CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName)
{
	const wchar_t* name = nullptr;
	wchar_t temp[512];
	if (lpName)
	{
		swprintf_s(temp, sizeof_array(temp), L"%hs", lpName);
		name = temp;
	}
	return Detoured_CreateFileMappingW(hFile, lpFileMappingAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, name);
}

HANDLE Detoured_OpenFileMappingW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	DETOURED_CALL(OpenFileMappingW);
	HANDLE mappingHandle = True_OpenFileMappingW(dwDesiredAccess, bInheritHandle, lpName);
	if (!mappingHandle)
	{
		DEBUG_LOG_TRUE(L"OpenFileMappingW", L"%ls -> Error", lpName);
		return NULL;
	}
	if (g_allowFileMappingDetour)
	{
		auto detouredHandle = new DetouredHandle(HandleType_FileMapping);
		detouredHandle->trueHandle = mappingHandle;
		mappingHandle = makeDetouredHandle(detouredHandle);
	}
	DEBUG_LOG_TRUE(L"OpenFileMappingW", L"%ls -> %llu", lpName, u64(mappingHandle));
	return mappingHandle;
}

LPVOID Detoured_MapViewOfFileEx(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap, LPVOID lpBaseAddress)
{
	DETOURED_CALL(MapViewOfFileEx);
	HANDLE trueMappingObject = hFileMappingObject;
	if (isDetouredHandle(hFileMappingObject))
	{
		auto& dh = asDetouredHandle(hFileMappingObject);
		if (dh.fileObject)
		{
			u64 offset = ToLargeInteger(dwFileOffsetHigh, dwFileOffsetLow).QuadPart;

			DEBUG_LOG_DETOURED(L"MapViewOfFileEx", L"(%ls)", HandleToName(hFileMappingObject));
			FileInfo& fi = *dh.fileObject->fileInfo;
			if (fi.fileMapMem && lpBaseAddress && lpBaseAddress != fi.fileMapMem) // This scenario happens with pch files in msvc cl.exe
			{
				if (dh.trueHandle == INVALID_HANDLE_VALUE) // This means we are using the "built-in" mapping handle
					trueMappingObject = fi.trueFileMapHandle;

				u8* res;

				// This actually uses more memory because "offset" can be really high
				//if (res = (u8*)True_MapViewOfFileEx(trueMappingObject, dwDesiredAccess, ToHigh(fi.trueFileMapOffset), ToLow(fi.trueFileMapOffset), dwNumberOfBytesToMap + offset, (u8*)lpBaseAddress - offset))
				//{
				//	// If we don't have any real MapViewOfFile on existing fileMapMem we can just drop the old one and use the new one.
				//	SCOPED_WRITE_LOCK(g_mappedFileTable.m_memLookupLock, lock);
				//	auto it = g_mappedFileTable.m_memLookup.find(fi.fileMapMem);
				//	if (it == g_mappedFileTable.m_memLookup.end())
				//	{
				//		if (!True_UnmapViewOfFile(fi.fileMapMem))
				//		{
				//			UBA_ASSERT(false);
				//		}
				//		fi.fileMapMem = (u8*)res;
				//		fi.fileMapMemEnd = fi.fileMapMem + dwNumberOfBytesToMap;
				//		g_mappedFileTable.m_memLookup[res + offset] = 1;
				//	}
				//	res += offset;
				//}
				//else
				{
					offset += fi.trueFileMapOffset;

					// We have retry here because this is typically where oom happen. Some of these mappings are 1gb pch files
					u32 counter = 0;
					do
					{
						res = (u8*)True_MapViewOfFileEx(trueMappingObject, dwDesiredAccess, ToHigh(offset), ToLow(offset), dwNumberOfBytesToMap, lpBaseAddress);
						if (res)
							break;
						DWORD error = GetLastError();
						if (error != ERROR_NOT_ENOUGH_MEMORY && error != ERROR_COMMITMENT_LIMIT)
							break;
						Rpc_AllocFailed(L"MapViewOfFile", error);
						++counter;
					} while (counter <= 10);
				}

				DEBUG_LOG_TRUE(L"INTERNAL MapViewOfFileEx", L"New FileObject for different base address (%ls)", HandleToName(hFileMappingObject));
				return res;
			}
			else if (!fi.fileMapMem)
				fi.fileMapViewDesiredAccess = dwDesiredAccess;

			if (!EnsureMapped(dh, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap, lpBaseAddress))
				return nullptr;

			SetLastError(ERROR_SUCCESS);

			u8* mem = fi.fileMapMem ? fi.fileMapMem : fi.memoryFile->baseAddress;

			mem += offset;

			if (fi.memoryFile && (dwDesiredAccess & FILE_MAP_WRITE)) // We assume changes will happen
				fi.memoryFile->isReported = false;

			SCOPED_WRITE_LOCK(g_mappedFileTable.m_memLookupLock, lock);
			++g_mappedFileTable.m_memLookup[mem];
			return mem;
		}
		UBA_ASSERT(dh.trueHandle != INVALID_HANDLE_VALUE);
		trueMappingObject = dh.trueHandle;
	}
	void* res = True_MapViewOfFileEx(trueMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap, lpBaseAddress);
	DEBUG_LOG_TRUE(L"MapViewOfFileEx", L"%llu (size %llu) (%ls) -> 0x%llx", uintptr_t(hFileMappingObject), dwNumberOfBytesToMap, HandleToName(hFileMappingObject), uintptr_t(res));

	return res;
}

LPVOID Detoured_MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap)
{
	DETOURED_CALL(MapViewOfFile);
	return Detoured_MapViewOfFileEx(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap, nullptr);
}

BOOL Detoured_UnmapViewOfFileEx(PVOID lpBaseAddress, ULONG UnmapFlags)
{
	DETOURED_CALL(UnmapViewOfFileEx);

	{
		SCOPED_WRITE_LOCK(g_mappedFileTable.m_memLookupLock, lock);
		auto it = g_mappedFileTable.m_memLookup.find(lpBaseAddress);
		if (it != g_mappedFileTable.m_memLookup.end())
		{
			if (!--it->second)
				g_mappedFileTable.m_memLookup.erase(it);
			SetLastError(ERROR_SUCCESS);
			return true;
		}
	}

	auto res = True_UnmapViewOfFileEx(lpBaseAddress, UnmapFlags); (void)res;
	DEBUG_LOG_TRUE(L"UnmapViewOfFileEx", L"0x%llx -> %ls", uintptr_t(lpBaseAddress), ToString(res));

	// TerminateProcess unmaps same memory address twice.. causing this log entry. Ignore for now
	//if (res == 0)
	//	ToInvestigate(L"Failed to Unmap 0x%llx -> %u", uintptr_t(lpBaseAddress), GetLastError());
	return TRUE;
}

BOOL Detoured_UnmapViewOfFile(LPCVOID lpBaseAddress)
{
	DETOURED_CALL(UnmapViewOfFile);
	return Detoured_UnmapViewOfFileEx((PVOID)lpBaseAddress, 0);
}

DWORD Detoured_GetFinalPathNameByHandleW(HANDLE hFile, LPTSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags)
{
	DETOURED_CALL(GetFinalPathNameByHandleW);

	HANDLE trueHandle = hFile;
	if (isDetouredHandle(hFile))
	{
		auto& dh = asDetouredHandle(hFile);
		auto fo = dh.fileObject;
		UBA_ASSERT(fo && fo->fileInfo->originalName);
		const wchar_t* fileName = fo->fileInfo->originalName;

		if (dwFlags == 0)
		{
			if (!fo->newName.empty())
				fileName = fo->newName.c_str();
			StringBuffer<> buffer;
			FixPath2(fileName, g_virtualWorkingDir.data, g_virtualWorkingDir.count, buffer.data, buffer.capacity, &buffer.count);

			if (cchFilePath <= buffer.count)
			{
				SetLastError(ERROR_NOT_ENOUGH_MEMORY);
				DEBUG_LOG_DETOURED(L"GetFinalPathNameByHandleW", L"%llu (%ls) -> Error (not enough mem)", uintptr_t(hFile), lpszFilePath);
				return buffer.count + 1;
			}

			// Unfortunately casing can be wrong here.. and we need to fix that. Let's use the directory table for that
			// Note, this really only matters when building linux target from windows.. then there is path validation that errors if this is not properly fixed
			StringBuffer<> buffer2;
			g_directoryTable.GetFinalPath(buffer2, fileName);
			UBA_ASSERT(buffer2.count == buffer.count);

			memcpy(lpszFilePath, buffer2.data, (buffer2.count + 1) * sizeof(tchar));
			DEBUG_LOG_DETOURED(L"GetFinalPathNameByHandleW", L"%llu (%ls) -> Success", uintptr_t(hFile), lpszFilePath);

			SetLastError(ERROR_SUCCESS);
			return buffer.count;
		}
		trueHandle = dh.trueHandle;
		UBA_ASSERTF(trueHandle != INVALID_HANDLE_VALUE, L"GetFinalPathNameByHandleW using flags (%u) on detoured file not handled (%s)", dwFlags, fileName);
	}

	auto res = True_GetFinalPathNameByHandleW(trueHandle, lpszFilePath, cchFilePath, dwFlags); // Calls NtQueryInformationFile and NtQueryObject
	DEBUG_LOG_TRUE(L"GetFinalPathNameByHandleW", L"%llu (%ls) -> %u", uintptr_t(hFile), (res != 0 ? lpszFilePath : L"UNKNOWN"), res);
	return res;
}

DWORD Detoured_SearchPathW(LPCWSTR lpPath, LPCWSTR lpFileName, LPCWSTR lpExtension, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR* lpFilePart)
{
	DETOURED_CALL(SearchPathW);
	if (g_runningRemote && !t_disallowDetour)
	{
		g_rules->RepairMalformedLibPath(lpFileName);

		const wchar_t* original = lpFileName; (void)original;
		u64 pathLen = wcslen(lpFileName);
		StringBuffer<512> tempBuf;
		Rpc_GetFullFileName(lpFileName, pathLen, tempBuf, true);
		UBA_ASSERT(nBufferLength > pathLen);
		memcpy(lpBuffer, lpFileName, pathLen * sizeof(tchar) + 2);
		DEBUG_LOG_DETOURED(L"SearchPathW", L"%ls %ls -> %ls", lpPath, original, lpFileName);
		SetLastError(ERROR_SUCCESS);
		return DWORD(pathLen);
	}

	DEBUG_LOG_TRUE(L"SearchPathW", L"%ls %ls", lpPath, lpFileName);
	return True_SearchPathW(lpPath, lpFileName, lpExtension, nBufferLength, lpBuffer, lpFilePart);
}

using AdditionalLoads = Vector<HMODULE, GrowingAllocator<HMODULE>>;
using VisitedModules = std::unordered_set<StringKey, std::hash<StringKey>, std::equal_to<StringKey>, GrowingAllocator<StringKey>>;

HMODULE Recursive_LoadLibraryExW(LPCWSTR lpLibFileName, LPCWSTR originalName, DWORD dwFlags, AdditionalLoads& additionalLoads, VisitedModules& visitedModules)
{
	if (!visitedModules.insert(ToStringKeyNoCheck(lpLibFileName, wcslen(lpLibFileName))).second)
		return 0;

	// Important that this code is not doing allocations.. it could cause a recursive stack overflow
	struct Import { wchar_t name[128]; bool isKnown;  Import(const wchar_t* s, bool ik) : isKnown(ik) { wcscpy_s(name, sizeof_array(name), s); } };
	std::vector<Import, GrowingAllocator<Import>> importedModules(&g_memoryBlock);
	{
		SuppressCreateFileDetourScope cfs;
		if (!FindImports(lpLibFileName, [&](const wchar_t* import, bool isKnown)
			{
				if (!GetModuleHandleW(import))
					importedModules.emplace_back(import, isKnown);
			}))
		{
			UBA_ASSERTF(false, L"Failed to find imports for binary %ls (%ls)", lpLibFileName, originalName);
		}
	}
	for (auto& importedModule : importedModules)
	{
		if (importedModule.isKnown && !g_isRunningWine)
			continue;

		HMODULE checkModule = GetModuleHandleW(importedModule.name);
		if (checkModule)
			continue;

		if (importedModule.isKnown) // We need to catch dbghelp.dll and imagehlp.dll
		{
			if (HMODULE h = True_LoadLibraryExW(importedModule.name, 0, 0))
			{
				OnModuleLoaded(h, importedModule.name);
				additionalLoads.push_back(h);
			}
			continue;
		}

		const wchar_t* path = importedModule.name;
		if (path[1] == ':')
			if (const wchar_t* lastSlash = wcsrchr(path, '\\'))
				path = lastSlash + 1;
		u64 pathLen = wcslen(path);
		StringBuffer<512> tempBuf;
		Rpc_GetFullFileName(path, pathLen, tempBuf, false);

		if (HMODULE r = Recursive_LoadLibraryExW(path, importedModule.name, dwFlags, additionalLoads, visitedModules))
			additionalLoads.push_back(r);
	}

	StringBuffer<512> newName;
	if (originalName[1] != ':' && !Equals(lpLibFileName, originalName))
	{
		newName.Append(g_virtualApplicationDir).Append(originalName);
		originalName = newName.data;
	}

	TrackInput(originalName);

	DEBUG_LOG_TRUE(L"INTERNAL LoadLibraryExW", L"%ls", originalName);

	SuppressCreateFileDetourScope cfs;
	auto res = True_LoadLibraryExW(lpLibFileName, 0, 0);
	if (res)
	{
		if (originalName[1] == ':')
		{
			SCOPED_WRITE_LOCK(g_loadedModulesLock, lock);
			g_loadedModules[res] = originalName;
		}
		if (g_isRunningWine)
			OnModuleLoaded(res, lpLibFileName);
	}
	return res;
}

HMODULE Shared_LoadLibrary(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	if (!g_runningRemote && !g_trackInputsMem)
		return NULL;

	StringBuffer<> path;
	path.Append(lpLibFileName).FixPathSeparators();

	bool detourDll = path.EndsWith(L".exe") || path.EndsWith(L".dll");
	if (detourDll && path.StartsWith(g_systemRoot.data))
		detourDll = g_runningRemote && GetFileAttributes(path.data) == INVALID_FILE_ATTRIBUTES; // It might be that remote machine actually doesn't have the file in system32. then we need to detour

	if (detourDll)
	{
		SuppressCreateFileDetourScope cfs;
		detourDll = !GetModuleHandle(lpLibFileName); // This internally can end up calling NtCreate and we don't want NtCreate to handle the download of the file because of paths
	}

	if (!detourDll)
		return NULL;

	u64 pathLen = path.count;
	u64 toSkip = 0;
	if (path.StartsWith(g_exeDir.data))
		toSkip = g_exeDir.count;
	else if (path.StartsWith(g_virtualApplicationDir.data))
		toSkip = g_virtualApplicationDir.count;
	const wchar_t* fileName = path.data + toSkip;
	pathLen -= toSkip;

	StringBuffer<512> tempBuf;
	const wchar_t* newPath = fileName;
	u64 newPathLen = pathLen;
	if (g_runningRemote)
		Rpc_GetFullFileName(newPath, newPathLen, tempBuf, false);

	AdditionalLoads additionalLoads(&g_memoryBlock); // Don't do allocations
	VisitedModules visitedModules(&g_memoryBlock);
	HMODULE res = Recursive_LoadLibraryExW(newPath, fileName, dwFlags, additionalLoads, visitedModules);
	for (HMODULE h : additionalLoads)
		FreeLibrary(h);
	return res;
}

HMODULE Detoured_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	DETOURED_CALL(LoadLibraryExW);
	DEBUG_LOG_DETOURED(L"LoadLibraryExW", L"(%ls)", lpLibFileName);


	if (!g_rules->AllowLoadLibrary(lpLibFileName))
		return 0;

	if (HMODULE res = Shared_LoadLibrary(lpLibFileName, hFile, dwFlags))
		return res;

	DEBUG_LOG_TRUE(L"LoadLibraryExW", L"(%ls)", lpLibFileName);
	return True_LoadLibraryExW(lpLibFileName, hFile, dwFlags);
}

HANDLE Detoured_GetStdHandle(DWORD nStdHandle)
{
	DETOURED_CALL(GetStdHandle);
	if (g_isDetachedProcess)
	{
		HANDLE res = g_stdHandle[nStdHandle + 12]; // STD_INPUT_HANDLE -10, STD_OUTPUT_HANDLE -11, STD_ERROR_HANDLE -12
		DEBUG_LOG_DETOURED(L"GetStdHandle", L"%u -> %llu", nStdHandle, u64(res));
		SetLastError(ERROR_SUCCESS);
		return res;
	}

	auto res = True_GetStdHandle(nStdHandle);
	DEBUG_LOG_TRUE(L"GetStdHandle", L"%u -> %llu", nStdHandle, u64(res));
	return res;
}

BOOL Detoured_SetStdHandle(DWORD nStdHandle, HANDLE hHandle)
{
	DETOURED_CALL(SetStdHandle);

	if (g_isDetachedProcess)
		return true;

	if (nStdHandle == STD_OUTPUT_HANDLE)
		g_stdHandle[1] = (hHandle != g_nullFile && GetFileType(hHandle) == FILE_TYPE_CHAR) ? hHandle : 0;
	else if (nStdHandle == STD_ERROR_HANDLE)
		g_stdHandle[0] = (hHandle != g_nullFile && GetFileType(hHandle) == FILE_TYPE_CHAR) ? hHandle : 0;

	//UBA_ASSERTF(!isDetouredHandle(hHandle), L"Trying to use handle %ls for std stream", HandleToName(hHandle));
	HANDLE trueHandle = hHandle;

	// TODO: Reason we have change to true handle is because this is transferred to child processes which can't use this process detoured handles
	// ... need to fix this.
	if (isDetouredHandle(hHandle))
	{
		auto& dh = asDetouredHandle(hHandle);
		trueHandle = dh.trueHandle;
		UBA_ASSERT(trueHandle != INVALID_HANDLE_VALUE);
	}
	DEBUG_LOG_TRUE(L"SetStdHandle", L"%u -> %llu", nStdHandle, u64(trueHandle));
	return True_SetStdHandle(nStdHandle, trueHandle);
}


BOOL Detoured_GetConsoleMode(HANDLE hConsoleHandle, LPDWORD lpMode)
{
	DETOURED_CALL(GetConsoleMode);
	if (hConsoleHandle == g_stdHandle[0] || hConsoleHandle == g_stdHandle[1])
	{
		*lpMode = 0xffff;
		return true;
	}
	else if (hConsoleHandle == g_stdHandle[2])
	{
		*lpMode = 0xffff;
		return true;
	}

	if (g_isDetachedProcess)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		DEBUG_LOG_DETOURED(L"GetConsoleMode", L"%llu -> Error", uintptr_t(hConsoleHandle));
		return false;
	}

	auto res = True_GetConsoleMode(hConsoleHandle, lpMode);
	DEBUG_LOG_TRUE(L"GetConsoleMode", L"%llu %u-> %ls", uintptr_t(hConsoleHandle), *lpMode, ToString(res));
	return res;
}

BOOL Detoured_SetConsoleMode(HANDLE hConsoleHandle, DWORD mode)
{
	DETOURED_CALL(SetConsoleMode);
	DEBUG_LOG_DETOURED(L"SetConsoleMode", L"(%u)", mode);

	g_echoOn = (mode & ~503) != 0; // TODO: This might be wrong. Trying to figure out how echo off in batch files work in terms of win32 calls
	{
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_EchoOn);
		writer.WriteBool(g_echoOn);
		writer.Flush();
	}
	return True_SetConsoleMode(hConsoleHandle, mode);
}

BOOL Detoured_GetConsoleTitleW(LPTSTR lpConsoleTitle, DWORD nSize)
{
	DETOURED_CALL(GetConsoleTitleW);
	DEBUG_LOG_DETOURED(L"GetConsoleTitleW", L"");
	*lpConsoleTitle = 0;
	return true;
}

BOOL Detoured_CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
	DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	DETOURED_CALL(CreateProcessW);
	DEBUG_LOG_DETOURED(L"CreateProcessW", L"%ls %ls %u %u %llu", lpApplicationName, lpCommandLine ? lpCommandLine : L"", dwCreationFlags, lpStartupInfo->dwFlags, u64(lpStartupInfo->hStdInput));

	if ((!lpApplicationName || !*lpApplicationName) && (!lpCommandLine || !*lpCommandLine))
	{
		SetLastError(ERROR_FILE_NOT_FOUND);
		return FALSE;
	}

	if (lpCommandLine && (Contains(lpCommandLine, L"winedbg") || Contains(lpCommandLine, L"werfault.exe") || Contains(lpCommandLine, L"vsjitdebugger.exe") || Contains(lpCommandLine, L"crashpad_handler.exe")))
	{
		if (g_runningRemote)
		{
			UbaAssert(L"Suppress debugger startup and try to report issue instead. This message is here to hopefully see callstack", "", 0, "", 0, false);
			return false;
		}
		else
		{
			SuppressDetourScope _;
			BOOL res = True_CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
			True_WaitForSingleObject(lpProcessInformation->hProcess, 10000);
			return res;
		}
	}

	TString commandLine;
	TString currentDir;
	u32 processId = 0;
	char dll[1024];
	{
		TimerScope ts(g_stats.createProcess);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_CreateProcess);
		writer.WriteString(lpApplicationName ? lpApplicationName : L"");
		writer.WriteString(lpCommandLine ? lpCommandLine : L"");
		writer.WriteString(lpCurrentDirectory ? lpCurrentDirectory : g_virtualWorkingDir.data);
		writer.Flush();
		BinaryReader reader;
		processId = reader.ReadU32();
		UBA_ASSERT(processId > 0);

		reader.Skip(sizeof(u32)); // Rules index

		u32 dllNameSize = reader.ReadU32();
		reader.ReadBytes(dll, dllNameSize);
		dll[dllNameSize] = 0;

		currentDir = reader.ReadString();
		commandLine = reader.ReadString();
		DEBUG_LOG_PIPE(L"CreateProcess", L"%ls %ls", lpApplicationName, lpCommandLine ? lpCommandLine : L"");
	}

	LPCSTR dlls[] = { dll };

	UBA_ASSERT(!isDetouredHandle(lpStartupInfo->hStdOutput));
	UBA_ASSERT(!isDetouredHandle(lpStartupInfo->hStdInput));
	UBA_ASSERT(!isDetouredHandle(lpStartupInfo->hStdError));

	lpStartupInfo->dwFlags |= STARTF_USESHOWWINDOW;
	lpStartupInfo->wShowWindow = SW_HIDE;

	if (g_rules->AllowDetach())
		dwCreationFlags |= DETACHED_PROCESS;
	else
		dwCreationFlags |= CREATE_NO_WINDOW;

	UBA_ASSERT((dwCreationFlags & CREATE_SUSPENDED) == 0);
	dwCreationFlags |= CREATE_SUSPENDED;
	BOOL res = true;
	u32 lastError = ERROR_SUCCESS;
	u32 retryCount = 0;

	++t_disallowDetour;
	while (true)
	{
		res = true;
		if (DetourCreateProcessWithDllsW(NULL, (wchar_t*)commandLine.c_str(), NULL, NULL, bInheritHandles, dwCreationFlags, lpEnvironment, currentDir.c_str(), lpStartupInfo, lpProcessInformation, sizeof_array(dlls), dlls, True_CreateProcessW))
			break;
		res = false;
		lastError = GetLastError();
		if (lastError != ERROR_ACCESS_DENIED && lastError != ERROR_INTERNAL_ERROR)
			break;
		// We have no idea why this is happening.. but it seems to recover when retrying.
		// Could it be related to two process spawning at the exact same time or something?
		// It happens extremely rarely and can happen on both host and remotes
		if (retryCount++ >= 5)
			break;
		const wchar_t* errorText = lastError == ERROR_ACCESS_DENIED ? L"access denied" : L"internal error";
		Rpc_WriteLogf(L"DetourCreateProcessWithDllEx failed with %ls, retrying %ls (Working dir: %ls)", errorText, commandLine.c_str(), currentDir.c_str());
		Sleep(100 + (rand() % 200)); // We have no idea
		continue;
	}
	--t_disallowDetour;
	UBA_ASSERTF(res, L"Failed to spawn process %ls (Error code: %u)", commandLine.c_str(), lastError);

	{
		TimerScope ts(g_stats.createProcess);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_StartProcess);
		writer.WriteU32(processId);
		writer.WriteBool(res);
		writer.WriteU32(lastError);
		writer.WriteU64(u64(lpProcessInformation->hProcess));
		writer.WriteU32(lpProcessInformation->dwProcessId);
		writer.WriteU64(u64(lpProcessInformation->hThread));
		writer.Flush();
		DEBUG_LOG_PIPE(L"StartProcess", L"%ls %ls", lpApplicationName, lpCommandLine ? lpCommandLine : L"");
	}

	HANDLE trueHandle = lpProcessInformation->hProcess;

	if (!res || trueHandle == INVALID_HANDLE_VALUE)
	{
		DEBUG_LOG_DETOURED(L"CreateProcessW", L"FAILED");
		return FALSE;
	}
	auto detouredHandle = new DetouredHandle(HandleType_Process);
	detouredHandle->trueHandle = trueHandle;
	lpProcessInformation->hProcess = makeDetouredHandle(detouredHandle);

	DEBUG_LOG_DETOURED(L"CreateProcessW", L"%llu", u64(lpProcessInformation->hProcess));
	return TRUE;
}

void Detoured_ExitProcess(UINT uExitCode)
{
	// Can't log this one
	DETOURED_CALL(ExitProcess);
	//DEBUG_LOG_TRUE(L"ExitProcess", L"(%u)", uExitCode);

	{
		CloseCaches();
		SendExitMessage((DWORD)uExitCode, GetTime());
		PostDeinit();
	}

#if UBA_DEBUG_LOG_ENABLED
	FlushDebugLog();
#endif
	True_ExitProcess(uExitCode);
}

BOOL Detoured_TerminateProcess(HANDLE hProcess, UINT uExitCode)
{
	DETOURED_CALL(TerminateProcess);
	DEBUG_LOG_DETOURED(L"TerminateProcess", L"%llu (%ls) ExitCode: %u", u64(hProcess), HandleToName(hProcess), uExitCode);

	// Some processes actually call terminateprocess on themselves when exiting, ugh.
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		CloseCaches();
		SendExitMessage((DWORD)uExitCode, GetTime());
		PostDeinit();
	}

	if (isDetouredHandle(hProcess))
		hProcess = asDetouredHandle(hProcess).trueHandle;
	return True_TerminateProcess(hProcess, uExitCode);
}

BOOL Detoured_GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode)
{
	DETOURED_CALL(GetExitCodeProcess);
	if (isDetouredHandle(hProcess))
		hProcess = asDetouredHandle(hProcess).trueHandle;
	BOOL res = True_GetExitCodeProcess(hProcess, lpExitCode);
	DEBUG_LOG_DETOURED(L"GetExitCodeProcess", L"%llu Exit code: %u -> %ls", uintptr_t(hProcess), *lpExitCode, ToString(res));
	return res;
}

BOOL Detoured_CreateTimerQueueTimer(PHANDLE phNewTimer, HANDLE TimerQueue, WAITORTIMERCALLBACK Callback, PVOID Parameter, DWORD DueTime, DWORD Period, ULONG Flags)
{
	DETOURED_CALL(CreateTimerQueueTimer);
	BOOL res = True_CreateTimerQueueTimer(phNewTimer, TimerQueue, Callback, Parameter, DueTime, Period, Flags);
	DEBUG_LOG_TRUE(L"CreateTimerQueueTimer", L"%p -> %ls", *phNewTimer, ToString(res));
	return res;
}

BOOL Detoured_DeleteTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer, HANDLE CompletionEvent)
{
	DETOURED_CALL(DeleteTimerQueueTimer);
	BOOL res = True_DeleteTimerQueueTimer(TimerQueue, Timer, CompletionEvent);
	if (!res && IsRunningWine())
	{
		DEBUG_LOG_DETOURED(L"DeleteTimerQueueTimer", L"%p %p %p -> %ls (WINE ignored)", TimerQueue, Timer, CompletionEvent, ToString(res));
		return true;
	}
	DEBUG_LOG_TRUE(L"DeleteTimerQueueTimer", L"%p %p %p -> %ls", TimerQueue, Timer, CompletionEvent, ToString(res));
	return res;
}

DWORD Detoured_WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
	return WaitForSingleObjectEx(hHandle, dwMilliseconds, false);
}

// Both WaitForSingleObject and WaitForSingleObjectEx is needed to support Wine
DWORD Detoured_WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable)
{
	DETOURED_CALL(WaitForSingleObjectEx);
	bool isProcess = false;
	if (isDetouredHandle(hHandle))
	{
		DetouredHandle& dh = asDetouredHandle(hHandle);
		hHandle = asDetouredHandle(hHandle).trueHandle;
		isProcess = dh.type == HandleType_Process;
	}

	auto res = True_WaitForSingleObjectEx(hHandle, dwMilliseconds, bAlertable);

	if (res != WAIT_OBJECT_0 || !isProcess)
		return res;

#if UBA_DEBUG_LOG_ENABLED
	if (isLogging())
	{
		auto lastError = GetLastError();
		DWORD exitCode;
		True_GetExitCodeProcess(hHandle, &exitCode);
		DEBUG_LOG_DETOURED(L"WaitForSingleObjectEx", L"for process %llu. Exit code: %u", uintptr_t(hHandle), exitCode);
		SetLastError(lastError);
	}
#endif

	Rpc_UpdateTables();

	return res;
}
DWORD Detoured_WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
	DETOURED_CALL(WaitForMultipleObjects);

	bool isProcess = false;
	HANDLE* tempHandles = (HANDLE*)_malloca(nCount * sizeof(HANDLE));
	if (!tempHandles)
		FatalError(1355, L"Here to please static analyzer");

	for (u32 i = 0; i != nCount; ++i)
	{
		HANDLE hHandle = lpHandles[i];
		if (isDetouredHandle(hHandle))
		{
			DetouredHandle& dh = asDetouredHandle(hHandle);
			hHandle = asDetouredHandle(hHandle).trueHandle;
			isProcess |= dh.type == HandleType_Process;
		}
		tempHandles[i] = hHandle;
	}

	auto res = True_WaitForMultipleObjectsEx(nCount, tempHandles, bWaitAll, dwMilliseconds, false);

#ifndef __clang_analyzer__
	_freea(tempHandles);
#endif

	if (!isProcess || res != WAIT_OBJECT_0)
		return res;

	DEBUG_LOG_DETOURED(L"WaitForMultipleObjects", L"");

	Rpc_UpdateTables();

	return res;
}

DWORD Detoured_WaitForMultipleObjectsEx(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds, BOOL bAlertable)
{
	DETOURED_CALL(WaitForMultipleObjectsEx);

	bool isProcess = false;
	HANDLE* tempHandles = (HANDLE*)_malloca(nCount * sizeof(HANDLE));
	if (!tempHandles)
		FatalError(1355, L"Here to please static analyzer");

	for (u32 i = 0; i != nCount; ++i)
	{
		HANDLE hHandle = lpHandles[i];
		if (isDetouredHandle(hHandle))
		{
			DetouredHandle& dh = asDetouredHandle(hHandle);
			hHandle = asDetouredHandle(hHandle).trueHandle;
			isProcess |= dh.type == HandleType_Process;
		}
		tempHandles[i] = hHandle;
	}

	auto res = True_WaitForMultipleObjectsEx(nCount, tempHandles, bWaitAll, dwMilliseconds, bAlertable);
#ifndef __clang_analyzer__
	_freea(tempHandles);
#endif

	if (!isProcess || res != WAIT_OBJECT_0)
		return res;

	DEBUG_LOG_DETOURED(L"WaitForMultipleObjectsEx", L"");

	Rpc_UpdateTables();

	return res;
}

LANGID Detoured_GetUserDefaultUILanguage()
{
	DETOURED_CALL(GetUserDefaultUILanguage);
	DEBUG_LOG_DETOURED(L"GetUserDefaultUILanguage", L"");
	//UBA_ASSERTF(g_runningRemote || True_GetUserDefaultUILanguage() == g_uiLanguage, L"Session process has language id %u while this process is set to use %u", g_uiLanguage, True_GetUserDefaultUILanguage());
	return LANGID(g_uiLanguage);
}

BOOL Detoured_GetThreadPreferredUILanguages(DWORD dwFlags, PULONG pulNumLanguages, PZZWSTR pwszLanguagesBuffer, PULONG pcchLanguagesBuffer)
{
	DETOURED_CALL(GetThreadPreferredUILanguages);

	if (dwFlags & MUI_LANGUAGE_ID)
	{
		DEBUG_LOG_DETOURED(L"GetThreadPreferredUILanguages", L"");
		//UBA_ASSERT(!(dwFlags & ~MUI_LANGUAGE_ID);
		*pulNumLanguages = 1;
		*pcchLanguagesBuffer = 6;

		if (!pwszLanguagesBuffer)
			return TRUE;

		swprintf_s(pwszLanguagesBuffer, 6, L"%04x", g_uiLanguage);
		pwszLanguagesBuffer[5] = 0;
		return TRUE;
	}
	else // MUI_LANGUAGE_NAME
	{
		// TODO: We need to get the string of g_uiLanguage
		//UBA_ASSERTF(!g_runningRemote, L"GetThreadPreferredUILanguages uses unsupported flag on remote execution: %u", dwFlags);
		auto res = True_GetThreadPreferredUILanguages(dwFlags, pulNumLanguages, pwszLanguagesBuffer, pcchLanguagesBuffer);
		DEBUG_LOG_TRUE(L"GetThreadPreferredUILanguages", L"-> %ls", ToString(res));
		return res;
	}
}

#if defined(DETOURED_INCLUDE_DEBUG)

BOOL Detoured_GetDiskFreeSpaceExA(LPCSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailableToCaller, PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes)
{
	DETOURED_CALL(GetDiskFreeSpaceExA);
	DEBUG_LOG_TRUE(L"GetDiskFreeSpaceExA", L"%hs", lpDirectoryName);
	return True_GetDiskFreeSpaceExA(lpDirectoryName, lpFreeBytesAvailableToCaller, lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes);
}

DWORD Detoured_GetLongPathNameA(LPCSTR lpszShortPath, LPSTR lpszLongPath, DWORD cchBuffer)
{
	DETOURED_CALL(GetLongPathNameA);
	DEBUG_LOG_TRUE(L"GetLongPathNameA", L"");
	UBA_ASSERT(!g_runningRemote);
	return True_GetLongPathNameA(lpszShortPath, lpszLongPath, cchBuffer);
}

DWORD Detoured_GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart)
{
	// Is verified that both windows and wine are calling GetFullPathNameW
	DETOURED_CALL(GetFullPathNameA);
	DEBUG_LOG_TRUE(L"GetFullPathNameA", L"");
	return True_GetFullPathNameA(lpFileName, nBufferLength, lpBuffer, lpFilePart);
}

DWORD Detoured_GetFileAttributesA(LPCSTR lpFileName)
{
	// Is verified that both windows and wine are calling GetFullPathNameW
	DEBUG_LOG_TRUE(L"GetFileAttributesA", L"");
	return True_GetFileAttributesA(lpFileName);
}

BOOL Detoured_GetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
{
	DETOURED_CALL(GetFileAttributesExA);
	DEBUG_LOG_TRUE(L"GetFileAttributesExA", L"");
	UBA_ASSERT(!g_runningRemote);
	return True_GetFileAttributesExA(lpFileName, fInfoLevelId, lpFileInformation);
}

DWORD Shared_GetModuleFileNameA(HMODULE hModule, const wchar_t* moduleName, u32 moduleNameLen, LPSTR lpFilename, DWORD nSize)
{
	if (nSize <= moduleNameLen)
	{
		if (nSize)
		{
			size_t res;
			errno_t err = wcstombs_s(&res, lpFilename, nSize, moduleName, nSize);
			if (err)
				UBA_ASSERTF(false, L"wcstombs_s failed for string '%s' with error code: %u", err);
			lpFilename[nSize - 1] = 0;
		}
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		DEBUG_LOG_DETOURED(L"GetModuleFileNameExA", L"%llu  %u INSUFFICIENT BUFFER (%hs) -> %u", uintptr_t(hModule), nSize, moduleName, moduleNameLen + 1);
		return nSize;
	}

	size_t res;
	errno_t err = wcstombs_s(&res, lpFilename, nSize, moduleName, moduleNameLen);
	if (err)
		UBA_ASSERTF(false, L"wcstombs_s failed for string '%s' with error code: %u", err);
	lpFilename[moduleNameLen] = 0;
	memset(lpFilename + moduleNameLen, 0, nSize - moduleNameLen);
	DEBUG_LOG_DETOURED(L"GetModuleFileNameExA", L"%llu  %u (%hs) -> %u", uintptr_t(hModule), nSize, lpFilename, moduleNameLen);
	SetLastError(ERROR_SUCCESS);
	return moduleNameLen + 1;
}

DWORD Detoured_GetModuleFileNameExA(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
	DETOURED_CALL(GetModuleFileNameExA);

	if (hProcess != (HANDLE)-1)
		if (GetProcessId(hProcess) != GetCurrentProcessId())
		{
			UBA_ASSERTF(!g_runningRemote, L"THIS SHOULD NOT HAPPEN!");
			return True_GetModuleFileNameExA(hProcess, hModule, lpFilename, nSize);
		}

	// If null we use the virtual application name
	if (hModule == NULL)
		return Shared_GetModuleFileNameA(hModule, g_virtualApplication.data, u32(g_virtualApplication.count), lpFilename, nSize);

	{
		// Check if there are any stored paths from dynamically loaded dlls
		SCOPED_READ_LOCK(g_loadedModulesLock, lock);
		auto findIt = g_loadedModules.find(hModule);
		if (findIt != g_loadedModules.end())
			return Shared_GetModuleFileNameA(hModule, findIt->second.c_str(), u32(findIt->second.size()), lpFilename, nSize);
	}

	if (!g_runningRemote)
		return True_GetModuleFileNameA(hModule, lpFilename, nSize);

	wchar_t moduleName[350];
	DWORD res = True_GetModuleFileNameW(hModule, moduleName, sizeof_array(moduleName));
	if (res == 0)
		return res;
	UBA_ASSERT(GetLastError() != ERROR_INSUFFICIENT_BUFFER);

	// This could be dlls that are loaded early one so might not exist in g_loadedModules
	// TODO: These could be wrong.. since the files could have been copied from different directories into the remote exedir
	if (!StartsWith(moduleName, g_exeDir.data))
		return Shared_GetModuleFileNameA(hModule, moduleName, res, lpFilename, nSize);

	StringBuffer<350> fileName;
	fileName.Append(g_virtualApplicationDir);
	fileName.Append(moduleName + g_exeDir.count);
	return Shared_GetModuleFileNameA(hModule, fileName.data, u32(fileName.count), lpFilename, nSize);
}

DWORD Detoured_GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
	DETOURED_CALL(GetModuleFileNameA);
	DEBUG_LOG_TRUE(L"GetModuleFileNameA", L"");
	//UBA_ASSERT(!g_runningRemote);
	return Detoured_GetModuleFileNameExA((HANDLE)-1, hModule, lpFilename, nSize);
}

DWORD Detoured_GetModuleBaseNameA(HANDLE hProcess, HMODULE hModule, LPSTR lpBaseName, DWORD nSize)
{
	DETOURED_CALL(GetModuleBaseNameA);
	DEBUG_LOG_TRUE(L"GetModuleBaseNameA", L"");

	char temp[1024];
	DWORD res = GetModuleFileNameExA(hProcess, hModule, temp, sizeof_array(temp)); (void)res;
	UBA_ASSERT(res != 0 && res < sizeof_array(temp));
	char* moduleName = temp;
	if (char* lastSlash = strrchr(temp, '\\'))
		moduleName = lastSlash + 1;
	DWORD len = (DWORD)strlen(moduleName);
	UBA_ASSERTF(len < nSize, L"Module name %hs does not fit in buffer size (is %u, needs %u)", moduleName, nSize, len);
	strcpy_s(lpBaseName, nSize, moduleName);
	memset(lpBaseName + len, 0, nSize - len);
	return len;
}

DWORD Detoured_GetModuleBaseNameW(HANDLE hProcess, HMODULE hModule, LPWSTR lpBaseName, DWORD nSize)
{
	DETOURED_CALL(GetModuleBaseNameW);
	DEBUG_LOG_TRUE(L"GetModuleBaseNameW", L"");

	wchar_t temp[1024];
	DWORD res = GetModuleFileNameExW(hProcess, hModule, temp, sizeof_array(temp)); (void)res;
	UBA_ASSERT(res != 0 && res < sizeof_array(temp));
	wchar_t* moduleName = temp;
	if (wchar_t* lastSlash = wcsrchr(temp, '\\'))
		moduleName = lastSlash + 1;
	DWORD len = (DWORD)wcslen(moduleName);
	UBA_ASSERTF(len < nSize, L"Module name %hs does not fit in buffer size (is %u, needs %u)", moduleName, nSize, len);
	wcscpy_s(lpBaseName, nSize, moduleName);
	return len;
}

LPTOP_LEVEL_EXCEPTION_FILTER Detoured_SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
{
	DETOURED_CALL(SetUnhandledExceptionFilter);
	DEBUG_LOG_TRUE(L"SetUnhandledExceptionFilter", L"");
	return True_SetUnhandledExceptionFilter(lpTopLevelExceptionFilter);
}

BOOL Detoured_CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
	DWORD dwCreationFlags, LPVOID lpEnvironment, LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	DETOURED_CALL(CreateProcessA);
	DEBUG_LOG_TRUE(L"CreateProcessA", L"(%hs)", lpCommandLine ? lpCommandLine : "");
	return True_CreateProcessA(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

BOOL Detoured_FlushInstructionCache(HANDLE hProcess, LPCVOID lpBaseAddress, SIZE_T dwSize)
{
	DETOURED_CALL(FlushInstructionCache);
	UBA_ASSERT(!isDetouredHandle(hProcess));
	BOOL res = True_FlushInstructionCache(hProcess, lpBaseAddress, dwSize);
	//DEBUG_LOG_DETOURED(L"FlushInstructionCache", L"%llu -> %ls", uintptr_t(hProcess), ToString(res));
	return res;
}

HANDLE Detoured_CreateFile2(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams)
{
	DETOURED_CALL(CreateFile2);
	DEBUG_LOG_TRUE(L"CreateFile2", L"(%ls)", lpFileName);
	return True_CreateFile2(lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
}

HANDLE Detoured_CreateFileTransactedW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, HANDLE hTransaction, PUSHORT pusMiniVersion, PVOID lpExtendedParameter)
{
	DETOURED_CALL(CreateFileTransactedW);
	DEBUG_LOG_TRUE(L"CreateFileTransacted", L"(%ls)", lpFileName);
	return True_CreateFileTransactedW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, hTransaction, pusMiniVersion, lpExtendedParameter);
}

HFILE Detoured_OpenFile(LPCSTR lpFileName, LPOFSTRUCT lpReOpenBuff, UINT uStyle)
{
	DETOURED_CALL(OpenFile);
	DEBUG_LOG_TRUE(L"OpenFile", L"(%hs)", lpFileName);
	return True_OpenFile(lpFileName, lpReOpenBuff, uStyle);
}

HANDLE Detoured_ReOpenFile(HANDLE hOriginalFile, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwFlagsAndAttributes)
{
	DETOURED_CALL(ReOpenFile);
	if (isDetouredHandle(hOriginalFile))
	{
		DEBUG_LOG_DETOURED(L"TODO ReOpenFile", L"(%ls)", HandleToName(hOriginalFile));
		return INVALID_HANDLE_VALUE;
	}
	DEBUG_LOG_TRUE(L"ReOpenFile", L"(%ls)", HandleToName(hOriginalFile));
	return True_ReOpenFile(hOriginalFile, dwDesiredAccess, dwShareMode, dwFlagsAndAttributes);
}

BOOL Detoured_ReadFileEx(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	DETOURED_CALL(ReadFileEx);
	DEBUG_LOG_TRUE(L"ReadFileEx", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	UBA_ASSERT(!isDetouredHandle(hFile));
	UBA_ASSERT(!isListDirectoryHandle(hFile));
	return True_ReadFileEx(hFile, lpBuffer, nNumberOfBytesToRead, lpOverlapped, lpCompletionRoutine);
}

BOOL Detoured_ReadFileScatter(HANDLE hFile, FILE_SEGMENT_ELEMENT* aSegmentArray, DWORD nNumberOfBytesToRead, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped)
{
	DETOURED_CALL(ReadFileScatter);
	DEBUG_LOG_TRUE(L"ReadFileScatter", L"%llu (%ls)", uintptr_t(hFile), HandleToName(hFile));
	UBA_ASSERT(!isDetouredHandle(hFile));
	UBA_ASSERT(!isListDirectoryHandle(hFile));
	return True_ReadFileScatter(hFile, aSegmentArray, nNumberOfBytesToRead, lpReserved, lpOverlapped);
}


void Detoured_SetLastError(DWORD dwErrCode)
{
	DETOURED_CALL(SetLastError);
	if (dwErrCode != ERROR_SUCCESS)
		while (false) {}
	True_SetLastError(dwErrCode);
}

DWORD Detoured_GetLastError()
{
	DETOURED_CALL(GetLastError);
	auto res = True_GetLastError();
	if (res != ERROR_SUCCESS)
		while (false) {}
	return res;
}

BOOL Detoured_SetFileValidData(HANDLE hFile, LONGLONG ValidDataLength)
{
	DETOURED_CALL(SetFileValidData);
	DEBUG_LOG_TRUE(L"SetFileValidData", L"(%ls)", HandleToName(hFile));
	UBA_ASSERT(!isDetouredHandle(hFile));
	return True_SetFileValidData(hFile, ValidDataLength);
}

BOOL Detoured_ReplaceFileW(LPCWSTR lpReplacedFileName, LPCWSTR lpReplacementFileName, LPCWSTR lpBackupFileName, DWORD dwReplaceFlags, LPVOID lpExclude, LPVOID lpReserved)
{
	UBA_ASSERT(!g_runningRemote);
	DETOURED_CALL(ReplaceFileW);
	DEBUG_LOG_TRUE(L"ReplaceFileW", L"");
	return True_ReplaceFileW(lpReplacedFileName, lpReplacementFileName, lpBackupFileName, dwReplaceFlags, lpExclude, lpReserved);
}

BOOL Detoured_CreateHardLinkA(LPCSTR lpFileName, LPCSTR lpExistingFileName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	UBA_ASSERT(!g_runningRemote);
	DETOURED_CALL(CreateHardLinkA);
	DEBUG_LOG_TRUE(L"CreateHardLinkA", L"");
	return True_CreateHardLinkA(lpFileName, lpExistingFileName, lpSecurityAttributes);
}

BOOL Detoured_DeleteFileA(LPCSTR lpFileName)
{
	UBA_ASSERT(!g_runningRemote);
	DETOURED_CALL(DeleteFileA);
	DEBUG_LOG_TRUE(L"DeleteFileA", L"");
	return True_DeleteFileA(lpFileName);
}

BOOLEAN Detoured_CreateSymbolicLinkW(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags)
{
	UBA_ASSERT(!g_runningRemote);
	DETOURED_CALL(CreateSymbolicLinkW);
	DEBUG_LOG_TRUE(L"CreateSymbolicLinkW", L"");
	return True_CreateSymbolicLinkW(lpSymlinkFileName, lpTargetFileName, dwFlags);
}

BOOLEAN Detoured_CreateSymbolicLinkA(LPCSTR lpSymlinkFileName, LPCSTR lpTargetFileName, DWORD  dwFlags)
{
	UBA_ASSERT(!g_runningRemote);
	DETOURED_CALL(CreateSymbolicLinkA);
	DEBUG_LOG_TRUE(L"CreateSymbolicLinkA", L"");
	return True_CreateSymbolicLinkA(lpSymlinkFileName, lpTargetFileName, dwFlags);
}

DWORD Detoured_SetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpValue)
{
	DETOURED_CALL(SetEnvironmentVariableW);
	DWORD res = True_SetEnvironmentVariableW(lpName, lpValue);
	DEBUG_LOG_TRUE(L"SetEnvironmentVariableW", L"%ls -> %ls", lpName, lpValue);
	return res;
}

DWORD Detoured_GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize)
{
	DETOURED_CALL(GetEnvironmentVariableW);
	DWORD res = True_GetEnvironmentVariableW(lpName, lpBuffer, nSize);
	DEBUG_LOG_TRUE(L"GetEnvironmentVariableW", L"%ls -> %ls", lpName, res ? lpBuffer : L"NOTFOUND");
	return res;
}

DWORD Detoured_GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
	DETOURED_CALL(GetEnvironmentVariableA);
	DWORD res = True_GetEnvironmentVariableA(lpName, lpBuffer, nSize);
	DEBUG_LOG_TRUE(L"GetEnvironmentVariableA", L"%hs -> %hs", lpName, res ? lpBuffer : "NOTFOUND");
	return res;
}

LPWCH Detoured_GetEnvironmentStringsW()
{
	DETOURED_CALL(GetEnvironmentStringsW);
	DEBUG_LOG_TRUE(L"GetEnvironmentStringsW", L"");
	auto res = True_GetEnvironmentStringsW();

	auto it = res;
	while (*it)
	{
		DEBUG_LOG(L"		VAR: %ls", it);
		it += wcslen(it) + 1;
	}

	return res;
}

DWORD Detoured_ExpandEnvironmentStringsW(LPCWSTR lpSrc, LPWSTR lpDst, DWORD nSize)
{
	DETOURED_CALL(ExpandEnvironmentStringsW);
	DEBUG_LOG_TRUE(L"ExpandEnvironmentStringsW", L"%ls", lpSrc);
	return True_ExpandEnvironmentStringsW(lpSrc, lpDst, nSize);
}

UINT Detoured_GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpPrefixString, UINT uUnique, LPTSTR lpTempFileName)
{
	DETOURED_CALL(GetTempFileNameW);
	DEBUG_LOG_TRUE(L"GetTempFileNameW", L"");
	return True_GetTempFileNameW(lpPathName, lpPrefixString, uUnique, lpTempFileName);
}

BOOL Detoured_CreateDirectoryExW(LPCWSTR lpTemplateDirectory, LPCWSTR lpNewDirectory, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	DETOURED_CALL(CreateDirectoryExW);
	DEBUG_LOG_TRUE(L"CreateDirectoryExW", L"");
	return True_CreateDirectoryExW(lpTemplateDirectory, lpNewDirectory, lpSecurityAttributes);
}

BOOL Detoured_DecryptFileW(LPCWSTR lpFileName, DWORD dwReserved)
{
	DETOURED_CALL(DecryptFileW);
	DEBUG_LOG_TRUE(L"DecryptFileW", L"");
	return True_DecryptFileW(lpFileName, dwReserved);
}

BOOL Detoured_DecryptFileA(LPCSTR lpFileName, DWORD dwReserved)
{
	DETOURED_CALL(DecryptFileA);
	DEBUG_LOG_TRUE(L"DecryptFileA", L"");
	return True_DecryptFileA(lpFileName, dwReserved);
}

BOOL Detoured_EncryptFileW(LPCWSTR lpFileName)
{
	DETOURED_CALL(EncryptFileW);
	DEBUG_LOG_TRUE(L"EncryptFileW", L"");
	return True_EncryptFileW(lpFileName);
}

BOOL Detoured_EncryptFileA(LPCSTR lpFileName)
{
	DETOURED_CALL(EncryptFileA);
	DEBUG_LOG_TRUE(L"EncryptFileA", L"");
	return True_EncryptFileA(lpFileName);
}

DWORD Detoured_OpenEncryptedFileRawW(LPCWSTR lpFileName, ULONG ulFlags, PVOID* pvContext)
{
	DETOURED_CALL(OpenEncryptedFileRawW);
	DEBUG_LOG_TRUE(L"OpenEncryptedFileRawW", L"");
	return True_OpenEncryptedFileRawW(lpFileName, ulFlags, pvContext);
}

DWORD Detoured_OpenEncryptedFileRawA(LPCSTR lpFileName, ULONG ulFlags, PVOID* pvContext)
{
	DETOURED_CALL(OpenEncryptedFileRawA);
	DEBUG_LOG_TRUE(L"OpenEncryptedFileRawA", L"");
	return True_OpenEncryptedFileRawA(lpFileName, ulFlags, pvContext);
}

HANDLE Detoured_OpenFileById(HANDLE hFile, LPFILE_ID_DESCRIPTOR lpFileID, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwFlags)
{
	DETOURED_CALL(OpenFileById);
	DEBUG_LOG_TRUE(L"OpenFileById", L"");
	UBA_ASSERT(!isDetouredHandle(hFile));
	return True_OpenFileById(hFile, lpFileID, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwFlags);
}

HANDLE Detoured_CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName)
{
	DETOURED_CALL(CreateEvent);
	if (lpName)
	{
		DEBUG_LOG_TRUE(L"CreateEvent", L"%ls", lpName);
	}
	return True_CreateEventW(lpEventAttributes, bManualReset, bInitialState, lpName);
}

HANDLE Detoured_CreateEventExW(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	DETOURED_CALL(CreateEventEx);
	if (lpName)
	{
		DEBUG_LOG_TRUE(L"CreateEventEx", L"%ls", lpName);
	}
	return True_CreateEventExW(lpEventAttributes, lpName, dwFlags, dwDesiredAccess);
}

HANDLE Detoured_CreateMutexExW(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	DETOURED_CALL(CreateMutexEx);
	if (lpName)
	{
		DEBUG_LOG_TRUE(L"CreateMutexEx", L"%ls", lpName);
	}
	return True_CreateMutexExW(lpMutexAttributes, lpName, dwFlags, dwDesiredAccess);
}

HANDLE Detoured_CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	DETOURED_CALL(CreateWaitableTimerExW);
	if (lpTimerName)
	{
		DEBUG_LOG_TRUE(L"CreateWaitableTimerExW", L"%ls", lpTimerName);
	}
	return True_CreateWaitableTimerExW(lpTimerAttributes, lpTimerName, dwFlags, dwDesiredAccess);
}

HANDLE Detoured_CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort, ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads)
{
	DETOURED_CALL(CreateIoCompletionPort);
	DEBUG_LOG_TRUE(L"CreateIoCompletionPort", L"%llu %llu", u64(FileHandle), u64(ExistingCompletionPort));
	HANDLE trueHandle = FileHandle;
	if (isDetouredHandle(FileHandle))
		trueHandle = asDetouredHandle(FileHandle).trueHandle;
	return True_CreateIoCompletionPort(trueHandle, ExistingCompletionPort, CompletionKey, NumberOfConcurrentThreads);
}

BOOL Detoured_CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize)
{
	DETOURED_CALL(CreatePipe);
	DEBUG_LOG_TRUE(L"CreatePipe", L"");
	return True_CreatePipe(hReadPipe, hWritePipe, lpPipeAttributes, nSize);
}

HANDLE Detoured_CreateNamedPipeW(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances, DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	DETOURED_CALL(CreateNamedPipeW);
	HANDLE h = True_CreateNamedPipeW(lpName, dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize, nInBufferSize, nDefaultTimeOut, lpSecurityAttributes);
	DEBUG_LOG_TRUE(L"CreateNamedPipeW", L"%ls -> %llu", lpName, u64(h));
	return h;
}

BOOL Detoured_PeekNamedPipe(HANDLE hNamedPipe, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesRead, LPDWORD lpTotalBytesAvail, LPDWORD lpBytesLeftThisMessage)
{
	UBA_ASSERT(!isDetouredHandle(hNamedPipe));
	return True_PeekNamedPipe(hNamedPipe, lpBuffer, nBufferSize, lpBytesRead, lpTotalBytesAvail, lpBytesLeftThisMessage);
}

BOOL Detoured_GetKernelObjectSecurity(HANDLE Handle, SECURITY_INFORMATION RequestedInformation, PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded)
{
	HANDLE trueHandle = Handle;
	if (isDetouredHandle(Handle))
		trueHandle = asDetouredHandle(Handle).trueHandle;
	return True_GetKernelObjectSecurity(trueHandle, RequestedInformation, pSecurityDescriptor, nLength, lpnLengthNeeded);
}

BOOL Detoured_ImpersonateNamedPipeClient(HANDLE hNamedPipe)
{
	UBA_ASSERT(!isDetouredHandle(hNamedPipe));
	return True_ImpersonateNamedPipeClient(hNamedPipe);
}

BOOL Detoured_TransactNamedPipe(HANDLE hNamedPipe, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesRead, LPOVERLAPPED lpOverlapped)
{
	UBA_ASSERT(!isDetouredHandle(hNamedPipe));
	return True_TransactNamedPipe(hNamedPipe, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesRead, lpOverlapped);
}

BOOL Detoured_SetNamedPipeHandleState(HANDLE hNamedPipe, LPDWORD lpMode, LPDWORD lpMaxCollectionCount, LPDWORD lpCollectDataTimeout)
{
	UBA_ASSERT(!isDetouredHandle(hNamedPipe));
	return True_SetNamedPipeHandleState(hNamedPipe, lpMode, lpMaxCollectionCount, lpCollectDataTimeout);
}

BOOL Detoured_GetNamedPipeInfo(HANDLE hNamedPipe, LPDWORD lpFlags, LPDWORD lpOutBufferSize, LPDWORD lpInBufferSize, LPDWORD lpMaxInstances)
{
	UBA_ASSERT(!isDetouredHandle(hNamedPipe));
	return True_GetNamedPipeInfo(hNamedPipe, lpFlags, lpOutBufferSize, lpInBufferSize, lpMaxInstances);
}

BOOL Detoured_GetNamedPipeHandleStateW(HANDLE hNamedPipe, LPDWORD lpState, LPDWORD lpCurInstances, LPDWORD lpMaxCollectionCount, LPDWORD lpCollectDataTimeout, LPWSTR lpUserName, DWORD nMaxUserNameSize)
{
	UBA_ASSERT(!isDetouredHandle(hNamedPipe));
	return True_GetNamedPipeHandleStateW(hNamedPipe, lpState, lpCurInstances, lpMaxCollectionCount, lpCollectDataTimeout, lpUserName, nMaxUserNameSize);
}

BOOL Detoured_GetNamedPipeServerProcessId(HANDLE Pipe, PULONG ServerProcessId)
{
	UBA_ASSERT(!isDetouredHandle(Pipe));
	return True_GetNamedPipeServerProcessId(Pipe, ServerProcessId);
}

BOOL Detoured_GetNamedPipeServerSessionId(HANDLE Pipe, PULONG ServerSessionId)
{
	UBA_ASSERT(!isDetouredHandle(Pipe));
	return True_GetNamedPipeServerSessionId(Pipe, ServerSessionId);
}


HANDLE Detoured_OpenFileMappingA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
{
	DETOURED_CALL(OpenFileMappingA);
	DEBUG_LOG_TRUE(L"OpenFileMappingA", L"");
	return True_OpenFileMappingA(dwDesiredAccess, bInheritHandle, lpName);
}

DWORD Detoured_GetMappedFileNameW(HANDLE hProcess, LPVOID lpv, LPWSTR lpFilename, DWORD nSize)
{
	DETOURED_CALL(GetMappedFileNameW);
	DEBUG_LOG_TRUE(L"GetMappedFileNameW", L"");
	return True_GetMappedFileNameW(hProcess, lpv, lpFilename, nSize);
}

BOOL Detoured_IsProcessorFeaturePresent(DWORD ProcessorFeature)
{
	DETOURED_CALL(IsProcessorFeaturePresent);
	auto res = True_IsProcessorFeaturePresent(ProcessorFeature);
	DEBUG_LOG_TRUE(L"IsProcessorFeaturePresent", L"%u -> %ls", res, ToString(res));
	return res;
}

// These functions require a very new version of win api (1-1-7).. so skip detouring these for now
//HANDLE Detoured_CreateFileMapping2(HANDLE File, SECURITY_ATTRIBUTES* SecurityAttributes, ULONG DesiredAccess, ULONG PageProtection, ULONG AllocationAttributes, ULONG64 MaximumSize, PCWSTR Name, MEM_EXTENDED_PARAMETER* ExtendedParameters, ULONG ParameterCount)
//{
//	DEBUG_LOG_TRUE(L"CreateFileMapping2", L"(%ls)", HandleToName(File));
//	UBA_ASSERT(!isDetouredHandle(File));
//	return True_CreateFileMapping2(File, SecurityAttributes, DesiredAccess, PageProtection, AllocationAttributes, MaximumSize, Name, ExtendedParameters, ParameterCount);
//}

//HANDLE Detoured_CreateFileMappingNumaW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName, DWORD nndPreferred)
//{
//	DEBUG_LOG_TRUE(L"CreateFileMappingNumaW", L"(%ls)", HandleToName(hFile));
//	UBA_ASSERT(!isDetouredHandle(hFile));
//	return True_CreateFileMappingNumaW(hFile, lpFileMappingAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName, nndPreferred);
//}

BOOL Detoured_FreeLibrary(HMODULE hModule)
{
	DETOURED_CALL(FreeLibrary);
	BOOL res = True_FreeLibrary(hModule);
	DEBUG_LOG_TRUE(L"FreeLibrary", L"%llu -> %ls", uintptr_t(hModule), ToString(res));
	return res;
}

LSTATUS Detoured_RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	DETOURED_CALL(RegOpenKeyExW);
	//UBA_NOT_IMPLEMENTED();
	SuppressCreateFileDetourScope cfs;
	auto res = True_RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
	DEBUG_LOG_TRUE(L"RegOpenKeyExW", L"(%ls) -> %ls", lpSubKey, ToString(res));
	return res;
}

LSTATUS Detoured_RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	DETOURED_CALL(RegOpenKeyExA);
	auto res = True_RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
	//UBA_NOT_IMPLEMENTED();
	DEBUG_LOG_TRUE(L"RegOpenKeyExA", L"%llu (%hs) -> %ls", uintptr_t(*phkResult), lpSubKey, ToString(res));
	return res;
}

LSTATUS Detoured_RegCloseKey(HKEY hKey)
{
	DETOURED_CALL(RegCloseKey);
	return True_RegCloseKey(hKey);
}

HANDLE Detoured_CreateConsoleScreenBuffer(DWORD dwDesiredAccess, DWORD dwShareMode, const SECURITY_ATTRIBUTES* lpSecurityAttributes, DWORD dwFlags, LPVOID lpScreenBufferData)
{
	DETOURED_CALL(CreateConsoleScreenBuffer);
	DEBUG_LOG_TRUE(L"CreateConsoleScreenBuffer", L"");
	return True_CreateConsoleScreenBuffer(dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwFlags, lpScreenBufferData);
}

HRESULT WINAPI Detoured_CreatePseudoConsole(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC)
{
	DETOURED_CALL(CreatePseudoConsole);
	DEBUG_LOG_TRUE(L"CreatePseudoConsole", L"");
	return True_CreatePseudoConsole(size, hInput, hOutput, dwFlags, phPC);
}

BOOL Detoured_CreateProcessAsUserW(HANDLE hToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	DETOURED_CALL(CreateProcessAsUserW);
	DEBUG_LOG_DETOURED(L"CreateProcessAsUserW", L"%ls %ls %u", lpApplicationName, lpCommandLine ? lpCommandLine : L"", dwCreationFlags);
	return True_CreateProcessAsUserW(hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

BOOL WINAPI Detoured_SetConsoleCtrlHandler(PHANDLER_ROUTINE HandlerRoutine, BOOL Add)
{
	DETOURED_CALL(SetConsoleCtrlHandler);
	DEBUG_LOG_DETOURED(L"SetConsoleCtrlHandler", L"");
	return TRUE;
}

UINT Detoured_GetConsoleOutputCP()
{
	DETOURED_CALL(GetConsoleOutputCP);
	DEBUG_LOG_DETOURED(L"GetConsoleOutputCP", L"");
	return 437;
	//auto res = True_GetConsoleOutputCP();
	//return res;
}

BOOL Detoured_ReadConsoleInputA(HANDLE hConsoleInput, PINPUT_RECORD lpBuffer, DWORD nLength, LPDWORD lpNumberOfEventsRead)
{
	DETOURED_CALL(ReadConsoleInput);
	DEBUG_LOG_DETOURED(L"ReadConsoleInput", L"");
	return FALSE;// True_ReadConsoleInput(hConsoleInput, lpBuffer, nLength, lpNumberOfEventsRead);
}

HWND Detoured_GetConsoleWindow()
{
	DETOURED_CALL(GetConsoleWindow);
	HWND res = True_GetConsoleWindow();
	DEBUG_LOG_TRUE(L"GetConsoleWindow", L"-> %llu", uintptr_t(res));
	return res;
}

BOOL Detoured_SetConsoleCursorPosition(HANDLE hConsoleOutput, COORD dwCursorPosition)
{
	DETOURED_CALL(SetConsoleCursorPosition);
	DEBUG_LOG_DETOURED(L"SetConsoleCursorPosition", L"");
	return True_SetConsoleCursorPosition(hConsoleOutput, dwCursorPosition);
}


BOOL Detoured_GetConsoleScreenBufferInfo(HANDLE hConsoleOutput, PCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo)
{
	DETOURED_CALL(GetConsoleScreenBufferInfo);
	DEBUG_LOG_DETOURED(L"GetConsoleScreenBufferInfo", L"");

	return True_GetConsoleScreenBufferInfo(hConsoleOutput, lpConsoleScreenBufferInfo);
	/*
	// We make these up just to make clang output errors with proper line breaks..
	lpConsoleScreenBufferInfo->dwSize = COORD{ 120,200 };
	lpConsoleScreenBufferInfo->dwCursorPosition = COORD{ 0,0 };
	lpConsoleScreenBufferInfo->wAttributes = 0;
	lpConsoleScreenBufferInfo->srWindow = SMALL_RECT{ 0,0,120,200 };
	lpConsoleScreenBufferInfo->dwMaximumWindowSize = COORD{120,8000};
	return true;
	*/
}

BOOL Detoured_ScrollConsoleScreenBufferW(HANDLE hConsoleOutput, const SMALL_RECT* lpScrollRectangle, const SMALL_RECT* lpClipRectangle, COORD dwDestinationOrigin, const CHAR_INFO* lpFill)
{
	DETOURED_CALL(ScrollConsoleScreenBufferW);
	DEBUG_LOG_DETOURED(L"ScrollConsoleScreenBufferW", L"");
	return True_ScrollConsoleScreenBufferW(hConsoleOutput, lpScrollRectangle, lpClipRectangle, dwDestinationOrigin, lpFill);
}

BOOL Detoured_FillConsoleOutputAttribute(HANDLE hConsoleOutput, WORD wAttribute, DWORD nLength, COORD dwWriteCoord, LPDWORD lpNumberOfAttrsWritten)
{
	DETOURED_CALL(FillConsoleOutputAttribute);
	DEBUG_LOG_DETOURED(L"FillConsoleOutputAttribute", L"");
	return True_FillConsoleOutputAttribute(hConsoleOutput, wAttribute, nLength, dwWriteCoord, lpNumberOfAttrsWritten);
}

BOOL Detoured_FillConsoleOutputCharacterW(HANDLE hConsoleOutput, TCHAR cCharacter, DWORD nLength, COORD dwWriteCoord, LPDWORD lpNumberOfCharsWritten)
{
	DETOURED_CALL(FillConsoleOutputCharacterW);
	DEBUG_LOG_DETOURED(L"FillConsoleOutputCharacterW", L"");
	return True_FillConsoleOutputCharacterW(hConsoleOutput, cCharacter, nLength, dwWriteCoord, lpNumberOfCharsWritten);
}

BOOL Detoured_FlushConsoleInputBuffer(HANDLE hConsoleInput)
{
	DETOURED_CALL(FlushConsoleInputBuffer);
	DEBUG_LOG_DETOURED(L"FlushConsoleInputBuffer", L"");
	return True_FlushConsoleInputBuffer(hConsoleInput);
}

BOOL Detoured_SetConsoleTextAttribute(HANDLE hConsoleOutput, WORD wAttributes)
{
	DETOURED_CALL(SetConsoleTextAttribute);
	DEBUG_LOG_DETOURED(L"SetConsoleTextAttribute", L"%llu %i", u64(hConsoleOutput), wAttributes);
	return True_SetConsoleTextAttribute(hConsoleOutput, wAttributes);
}

BOOL Detoured_SetConsoleTitleW(LPCTSTR lpConsoleTitle)
{
	DETOURED_CALL(SetConsoleTitleW);
	DEBUG_LOG_DETOURED(L"SetConsoleTitleW", L"");
	return true;
}

int Detoured_GetLocaleInfoEx(LPCWSTR lpLocaleName, LCTYPE LCType, LPWSTR lpLCData, int cchData)
{
	DETOURED_CALL(GetLocaleInfoEx);
	//DEBUG_LOG_TRUE(L"GetLocaleInfoEx", L"(%ls)", lpLocaleName);
	auto res = True_GetLocaleInfoEx(lpLocaleName, LCType, lpLCData, cchData);
	//UBA_ASSERTF(false, L"%i %ls %u %ls %i", res, lpLocaleName, LCType, lpLCData, cchData);
	return res;
}

int Detoured_GetUserDefaultLocaleName(LPWSTR lpLocaleName, int cchLocaleName)
{
	DETOURED_CALL(GetUserDefaultLocaleName);
	//wcscpy_s(lpLocaleName, cchLocaleName, L"en-US");
	//int res = 6;
	int res = True_GetUserDefaultLocaleName(lpLocaleName, cchLocaleName);
	DEBUG_LOG_TRUE(L"GetUserDefaultLocaleName", L"(%ls) -> %u", lpLocaleName, res);
	return res;
}

BOOL Detoured_IsValidCodePage(UINT CodePage)
{
	DETOURED_CALL(IsValidCodePage);
	BOOL res = True_IsValidCodePage(CodePage);
	DEBUG_LOG_TRUE(L"IsValidCodePage", L"-> %u", res);
	return res;
}

UINT Detoured_GetACP()
{
	DETOURED_CALL(GetACP);
	auto res = True_GetACP();
	DEBUG_LOG_TRUE(L"GetACP", L"-> %u", res);
	return res;
}

LPCWSTR Detoured_PathFindFileNameW(LPCWSTR pszPath) // This is called by Ps4SymbolTool.exe
{
	UBA_ASSERTF(!g_runningRemote, L"%ls", pszPath);
	auto res = True_PathFindFileNameW(pszPath);
	DEBUG_LOG_TRUE(L"PathFindFileNameW", L"(%ls) -> %ls", pszPath, res);
	return res;
}

BOOL Detoured_PathIsRelativeW(LPCWSTR pszPath)
{
	//UBA_ASSERTF(!g_runningRemote, L"%ls", pszPath); // intel compiler uses PathIsRelativeW.. don't know if this function touches file system but will comment out this assert for now
	auto res = True_PathIsRelativeW(pszPath);
	DEBUG_LOG_TRUE(L"PathIsRelativeW", L"(%ls) -> %ls", pszPath, res);
	return res;
}
BOOL Detoured_PathIsDirectoryEmptyW(LPCWSTR pszPath)
{
	UBA_ASSERTF(!g_runningRemote, L"%ls", pszPath);
	auto res = True_PathIsDirectoryEmptyW(pszPath);
	DEBUG_LOG_TRUE(L"PathIsDirectoryEmptyW", L"(%ls) -> %ls", pszPath, res);
	return res;
}

HRESULT Detoured_SHCreateStreamOnFileW(LPCWSTR pszFile, DWORD grfMode, IStream** ppstm)
{
	UBA_ASSERTF(!g_runningRemote, L"%ls", pszFile);
	return True_SHCreateStreamOnFileW(pszFile, grfMode, ppstm);
}

BOOL Detoured_PathFileExistsW(LPCWSTR pszPath)
{
	DEBUG_LOG_DETOURED(L"PathFileExistsW", L"CALLING GetFileAttributesW (%s)", pszPath);
	DWORD attributes = Detoured_GetFileAttributesW(pszPath);
	return attributes != INVALID_FILE_ATTRIBUTES;

	//UBA_ASSERTF(!g_runningRemote, L"%ls", pszPath);
	//auto res = True_PathFileExistsW(pszPath);
	//DEBUG_LOG_TRUE(L"PathFileExistsW", L"(%ls) -> %ls", pszPath, res);
	//return res;
}

#endif // DETOURED_INCLUDE_DEBUG

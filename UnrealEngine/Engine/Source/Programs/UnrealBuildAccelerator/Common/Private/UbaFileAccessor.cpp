// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaFileAccessor.h"
#include "UbaFile.h"
#include "UbaLogger.h"
#include "UbaStats.h"

#if PLATFORM_LINUX
#include <sys/sendfile.h>
#elif PLATFORM_MAC
#include <copyfile.h>
#endif

#define UBA_USE_WRITE_THROUGH 0

namespace uba
{
	#if PLATFORM_WINDOWS
	void GetProcessHoldingFile(StringBufferBase& out, const tchar* fileName);
	HANDLE asHANDLE(FileHandle fh);
	#else
	int asFileDescriptor(FileHandle fh);
	Atomic<u32> g_tempFileCounter;
	#endif
		
#if PLATFORM_WINDOWS
	bool SetDeleteOnClose(Logger& logger, const tchar* fileName, FileHandle& handle, bool value)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().setFileInfo);
		FILE_DISPOSITION_INFO info;
		info.DeleteFile = value;
		if (!::SetFileInformationByHandle(asHANDLE(handle), FileDispositionInfo, &info, sizeof(info)))
			return logger.Error(TC("SetFileInformationByHandle (FileDispositionInfo) failed on %llu %s (%s)"), uintptr_t(handle), fileName, LastErrorToText().data);
		return true;
	}
#endif

	FileAccessor::FileAccessor(Logger& logger, const tchar* fileName)
	:	m_logger(logger)
	,	m_fileName(fileName)
	{
	}

	FileAccessor::~FileAccessor()
	{
		InternalClose(false, nullptr);
	}

	bool FileAccessor::CreateWrite(bool allowRead, u32 flagsAndAttributes, u64 fileSize, const tchar* tempPath)
	{
		UBA_ASSERT(flagsAndAttributes != 0);
		m_size = fileSize;

		const tchar* realFileName = m_fileName;

		#if !PLATFORM_WINDOWS
		m_tempPath = tempPath;
		StringBuffer<> tempFile;
		if (tempPath)
		{
			m_tempFileIndex = g_tempFileCounter++;
			realFileName = tempFile.Append(tempPath).Append("Temp_").AppendValue(m_tempFileIndex).data;
		}
		#endif

		#if UBA_USE_WRITE_THROUGH
		flagsAndAttributes |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
		#endif

		u32 createDisp = CREATE_ALWAYS;
		u32 dwDesiredAccess = GENERIC_WRITE | DELETE;
		if (allowRead)
			dwDesiredAccess |= GENERIC_READ;
		u32 dwShareMode = 0;// FILE_SHARE_READ | FILE_SHARE_WRITE;
		u32 retryCount = 0;
		StringBuffer<256> additionalInfo;
		while (true)
		{
			m_fileHandle = uba::CreateFileW(realFileName, dwDesiredAccess, dwShareMode, createDisp, flagsAndAttributes);
			if (m_fileHandle != InvalidFileHandle)
			{
				if (retryCount)
					m_logger.Warning(TC("Had to retry %u times to open file %s for write (because is was being used%s)"), retryCount, realFileName, additionalInfo.data);
				break;
			}
			u32 lastError = GetLastError();
			#if PLATFORM_WINDOWS
			if (lastError == ERROR_SHARING_VIOLATION)
			{
				if (retryCount == 0)
					GetProcessHoldingFile(additionalInfo, m_fileName);

				if (retryCount < 5)
				{
					Sleep(1000);
					++retryCount;
					continue;
				}
			}
			#endif
			return m_logger.Error(TC("ERROR opening file %s for write (%s%s)"), realFileName, LastErrorToText(lastError).data, additionalInfo.data);
		}

#if PLATFORM_WINDOWS
		if (!SetDeleteOnClose(m_logger, m_fileName, m_fileHandle, true))
			return false;

		if (flagsAndAttributes & FILE_FLAG_OVERLAPPED)
		{
			if (fileSize != ~u64(0))
			{
				DWORD dwTemp;
				if (!::DeviceIoControl(asHANDLE(m_fileHandle), FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &dwTemp, NULL))
				{
					DWORD lastError = GetLastError();
					if (lastError != ERROR_INVALID_FUNCTION) // Some file systems don't support this
						return m_logger.Error(TC("Failed to make file %s sparse (%s)"), realFileName, LastErrorToText(lastError).data);
				}
				SetEndOfFile(m_logger, realFileName, m_fileHandle, fileSize);
			}
			(u64&)m_fileHandle |= OverlappedIoFlag;
		}
#endif
		m_isWrite = true;
		return true;
	}

	bool FileAccessor::CreateMemoryWrite(bool allowRead, u32 flagsAndAttributes, u64 size, const tchar* tempPath)
	{
		#if PLATFORM_WINDOWS
		allowRead = true; // It is not possible to have write only access to file mappings it seems
		#endif

		m_size = size;

		UBA_ASSERT(flagsAndAttributes != 0);
		if (!CreateWrite(allowRead, flagsAndAttributes, size, tempPath))
			return false;

		m_mappingHandle = uba::CreateFileMappingW(m_fileHandle, PAGE_READWRITE, size, m_fileName);
		if (!m_mappingHandle.IsValid())
			return m_logger.Error(TC("Failed to create memory map %s (%s)"), m_fileName, LastErrorToText().data);

		m_data = MapViewOfFile(m_mappingHandle, FILE_MAP_WRITE, 0, size);
		if (!m_data)
			return m_logger.Error(TC("Failed to map view of file %s with size %llu, for write (%s)"), m_fileName, size, LastErrorToText().data);

		return true;
	}

	bool FileAccessor::Close(u64* lastWriteTime)
	{
		return InternalClose(true, lastWriteTime);
	}

	bool FileAccessor::Write(const void* data, u64 dataLen, u64 offset)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().writeFile);

		if (!m_isWrite)
			return false;

		#if UBA_USE_WRITE_THROUGH
		bool useWriteThrough = true;
		u8 writeThroughBuffer[4 * 1024];
		bool setFileSize = false;
		#endif

#if PLATFORM_WINDOWS
		if ((u64)m_fileHandle & OverlappedIoFlag)
		{
			constexpr u64 BlockSize = 1024 * 1024;
			constexpr u64 BlockCount = 256;
			OVERLAPPED ol[BlockCount];
			Event ev[BlockCount];
			u64 writeLeft = dataLen;
			u8* pos = (u8*)data;
			u64 i = 0;

			auto WaitAndCheckError = [&](u64 index)
				{
					if (!ev[index].IsSet())
						return m_logger.Error(L"Overlapped I/O WriteFile FAILED on waiting for event!");
					u32 error = u32(ol[index].Internal);
					if (error != ERROR_SUCCESS)
						return m_logger.Error(L"Overlapped I/O WriteFile FAILED!: %s", LastErrorToText(error).data);
					return true;
				};

			auto eg = MakeGuard([&]()
				{
					u64 index = i % BlockCount;
					if (i > BlockCount)
						for (u64 j = index; j != BlockCount; ++j)
							if (!WaitAndCheckError(j))
								return false;
					for (u64 j = 0; j != index; ++j)
						if (!WaitAndCheckError(j))
							return false;
					return true;
				});

			while (writeLeft)
			{
				u64 index = i % BlockCount;

				if (i < BlockCount)
					ev[i].Create(false);
				else
				{
					if (!WaitAndCheckError(index))
						return false;
				}

				u64 toWrite = Min(writeLeft, BlockSize);
				u64 toActuallyWrite = toWrite;

				#if UBA_USE_WRITE_THROUGH
				if (useWriteThrough)
				{
					if (toWrite < BlockSize)
					{
						toActuallyWrite = (toWrite / 4096) * 4096;
						if (!toActuallyWrite)
						{
							memcpy(writeThroughBuffer, pos, toWrite);
							toActuallyWrite = 4096;
							setFileSize = true;
						}
						else
							toWrite = toActuallyWrite;
					}
				}
				#endif

				ol[index] = {};
				ol[index].hEvent = ev[index].GetHandle();
				ol[index].Offset = ToLow(offset + i * BlockSize);
				ol[index].OffsetHigh = ToHigh(offset + i * BlockSize);

				if (!::WriteFile(asHANDLE(m_fileHandle), pos, u32(toActuallyWrite), NULL, ol + index))
				{
					u32 lastError = GetLastError();
					if (lastError != ERROR_IO_PENDING)
						return m_logger.Error(L"FAILED!: %s", LastErrorToText(lastError).data);
				}
				++i;
				pos += toWrite;
				writeLeft -= toWrite;
			}

			if (!eg.Execute())
				return false;

			#if UBA_USE_WRITE_THROUGH
			if (setFileSize)
				SetEndOfFile(m_logger, m_fileName, m_fileHandle, dataLen);
			#endif

			return true;
		}
#endif

		u64 writeLeft = dataLen;
		u8* pos = (u8*)data;
		while (writeLeft)
		{
			u32 toWrite = u32(Min(writeLeft, 256llu * 1024 * 1024));
			u32 toActuallyWrite = toWrite;

			#if UBA_USE_WRITE_THROUGH
			if (useWriteThrough)
			{
				toActuallyWrite = (toWrite / 4096) * 4096;
				if (!toActuallyWrite)
				{
					memcpy(writeThroughBuffer, pos, toWrite);
					toActuallyWrite = 4096;
					setFileSize = true;
				}
			}
			#endif

#if PLATFORM_WINDOWS
			DWORD written;
			if (!::WriteFile(asHANDLE(m_fileHandle), pos, toActuallyWrite, &written, NULL))
			{
				DWORD lastError = GetLastError();
				m_logger.Error(TC("ERROR writing file %s writing %u bytes (%llu bytes written out of %llu) (%s)"), m_fileName, toWrite, (dataLen - writeLeft), dataLen, LastErrorToText(lastError).data);

				if (lastError == ERROR_DISK_FULL)
					ExitProcess(ERROR_DISK_FULL);

				return false;
			}
			if (written > toWrite)
				written = toWrite;
#else
			ssize_t written = write(asFileDescriptor(m_fileHandle), pos, toActuallyWrite);
			if (written == -1)
			{
				UBA_ASSERTF(false, TC("WriteFile error handling not implemented for %i (%s)"), errno, strerror(errno));
				return false;
			}
#endif
			writeLeft -= written;
			pos += written;
		}

		#if UBA_USE_WRITE_THROUGH
		if (setFileSize)
			SetEndOfFile(m_logger, m_fileName, m_fileHandle, dataLen);
		#endif

		return true;
	}

	bool FileAccessor::OpenRead()
	{
		UBA_ASSERT(false);
		return false;
	}

	bool FileAccessor::OpenMemoryRead(u64 offset)
	{
		if (!OpenFileSequentialRead(m_logger, m_fileName, m_fileHandle))
			return m_logger.Error(TC("Failed to open file %s for read"), m_fileName);

		FileInformation info;
		if (!GetFileInformationByHandle(info, m_logger, m_fileName, m_fileHandle))
			return m_logger.Error(TC("GetFileInformationByHandle failed on %s"), m_fileName);

		m_size = info.size;
#if PLATFORM_WINDOWS
		if (m_size)
			m_mappingHandle = uba::CreateFileMappingW(m_fileHandle, PAGE_READONLY, m_size, m_fileName);
		else
			m_mappingHandle = uba::CreateFileMappingW(InvalidFileHandle, PAGE_READONLY, 1, m_fileName);

		if (!m_mappingHandle.IsValid())
			return m_logger.Error(TC("Failed to create mapping handle for %s (%s)"), m_fileName, LastErrorToText().data);
#else
		m_mappingHandle = { asFileDescriptor(m_fileHandle) };
		if (offset == m_size)
			return true;
#endif
		m_data = MapViewOfFile(m_mappingHandle, FILE_MAP_READ, offset, m_size);
		if (!m_data)
			return m_logger.Error(TC("%s - MapViewOfFile failed (%s)"), m_fileName, LastErrorToText().data);

		return true;
	}


	bool FileAccessor::InternalClose(bool success, u64* lastWriteTime)
	{
		if (m_data)
		{
			if (!UnmapViewOfFile(m_data, m_size, m_fileName))
				return m_logger.Error(TC("Failed to unmap memory for %s (%s)"), m_fileName, LastErrorToText().data);
			m_data = nullptr;
		}

		if (m_mappingHandle.IsValid())
		{
			if (!CloseFileMapping(m_mappingHandle))
				return m_logger.Error(TC("Failed to close file mapping for %s (%s)"), m_fileName, LastErrorToText().data);
			m_mappingHandle = {};
		}

		if (m_fileHandle != InvalidFileHandle)
		{
			const tchar* realFileName = m_fileName;
			StringBuffer<> tempFile;

			if (m_isWrite)
			{
				#if !PLATFORM_WINDOWS
				if (m_tempPath)
					realFileName = tempFile.Append(m_tempPath).Append("Temp_").AppendValue(m_tempFileIndex).data;
				#endif

				if (success)
				{
					#if PLATFORM_WINDOWS
					if (!SetDeleteOnClose(m_logger, realFileName, m_fileHandle, false))
						return m_logger.Error(TC("Failed to remove delete on close for file %s (%s)"), realFileName, LastErrorToText().data);
					#else
					if (m_tempPath && rename(realFileName, m_fileName) == -1)
					{
						if (errno != EXDEV)
							return m_logger.Error(TC("Failed to rename temporary file %s to %s (%s)"), realFileName, m_fileName, strerror(errno));

						// Need to copy, can't rename over devices
						int targetFd = open(m_fileName, O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC);
						auto g = MakeGuard([targetFd]() { close(targetFd); });
						if (targetFd == -1)
							return m_logger.Error(TC("Failed to create file %s for move from temporary file %s (%s)"), m_fileName, realFileName, strerror(errno));
						
						#if PLATFORM_MAC
						if (fcopyfile(asFileDescriptor(m_fileHandle), targetFd, 0, COPYFILE_ALL) == -1)
							return m_logger.Error(TC("Failed to do fcopyfile from temporary %s to file %s (%s)"), realFileName, m_fileName, strerror(errno));
						#else
						int sourceFd = asFileDescriptor(m_fileHandle);
						if (lseek(sourceFd, 0, SEEK_SET) == -1)
							return m_logger.Error(TC("Failed to do lseek to beginning for sendfile (%s)"), strerror(errno));
						if (sendfile(targetFd, sourceFd, NULL, m_size) != m_size)
							return m_logger.Error(TC("Failed to do sendfile from temporary %s to file %s (%s)"), realFileName, m_fileName, strerror(errno));
						#endif

						remove(realFileName); // Remove real file now when we have copied it over
					}
					#endif

					if (lastWriteTime)
					{
						*lastWriteTime = 0;
						if (!GetFileLastWriteTime(*lastWriteTime, m_fileHandle))
							m_logger.Warning(TC("Failed to get file time for %s (%s)"), realFileName, LastErrorToText().data);
					}
				}
				else
				{
					#if !PLATFORM_WINDOWS
					if (m_tempPath && remove(realFileName) == -1)
						return m_logger.Error(TC("Failed to remove temporary file %s (%s)"), realFileName, strerror(errno));
					#endif
				}
			}
			if (!CloseFile(realFileName, m_fileHandle))
				return m_logger.Error(TC("Failed to close file %s (%s)"), realFileName, LastErrorToText().data);
			m_fileHandle = InvalidFileHandle;
		}
		return true;
	}
}

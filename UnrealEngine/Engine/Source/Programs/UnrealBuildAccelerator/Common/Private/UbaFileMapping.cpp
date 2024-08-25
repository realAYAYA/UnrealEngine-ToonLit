// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaFileMapping.h"
#include "UbaBottleneck.h"
#include "UbaDirectoryIterator.h"
#include "UbaFile.h"
#include "UbaPlatform.h"
#include "UbaProcessStats.h"
#include "UbaTimer.h"
#include "UbaWorkManager.h"

#if !PLATFORM_WINDOWS
#include <sys/file.h>
#endif

namespace uba
{
#if PLATFORM_WINDOWS
	Bottleneck& g_createFileHandleBottleneck = *new Bottleneck(8); // Allocated and leaked just to prevent shutdown asserts in debug

	//Atomic<u64> g_fileMappingCount;

	HANDLE asHANDLE(FileHandle fh);

	HANDLE InternalCreateFileMappingW(HANDLE hFile, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName)
	{
		//++g_fileMappingCount;

		if (flProtect != PAGE_READWRITE)
			return ::CreateFileMappingW(hFile, NULL, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);

		// Experiment to try to prevent lock happening on AWS servers when lots of helpers are sending back obj files.
		BottleneckScope bs(g_createFileHandleBottleneck);
		return ::CreateFileMappingW(hFile, NULL, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
	}
#else
	int asFileDescriptor(FileHandle fh);
#endif

	ReaderWriterLock g_mappingUidCounterLock;
	Atomic<u64> g_mappingUidCounter;

	FileMappingHandle CreateMemoryMappingW(Logger& logger, u32 flProtect, u64 maxSize, const tchar* name)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().createFileMapping);
#if PLATFORM_WINDOWS
		return { InternalCreateFileMappingW(INVALID_HANDLE_VALUE, flProtect, (DWORD)ToHigh(maxSize), ToLow(maxSize), name) };
#else
		UBA_ASSERT(!name);
		UBA_ASSERT((flProtect & (~u32(PAGE_READWRITE | SEC_RESERVE))) == 0);

		// Since we need to not leak file mappings we use files as a trick to know which ones are used and not
		StringBuffer<64> lockDir;
		lockDir.Append("/tmp/uba_shm_locks");

		SCOPED_WRITE_LOCK(g_mappingUidCounterLock, lock);
		if (!g_mappingUidCounter)
		{
			// Create dir
			if (mkdir(lockDir.data, 0777) == -1)
				if (errno != EEXIST)
				{
					UBA_ASSERTF(false, "Failed to create %s (%s)", lockDir.data, strerror(errno));
					return {};
				}

			// Clear out all orphaned shm_open
			TraverseDir(logger, lockDir.data,
				[&](const DirectoryEntry& e)
				{
					u32 uid = strtoul(e.name, nullptr, 10);

					StringBuffer<128> lockFile;
					lockFile.Append(lockDir).EnsureEndsWithSlash().Append(e.name);
					int lockFd = open(lockFile.data, O_RDWR, S_IRUSR | S_IWUSR | O_CLOEXEC);
					if (lockFd == -1)
					{
						if (errno == EPERM)
						{
							g_mappingUidCounter = uid;
							return;
						}
						UBA_ASSERTF(false, "Failed to open %s (%s)", lockFile.data, strerror(errno));
					}

					if (flock(lockFd, LOCK_EX | LOCK_NB) == 0)
					{
						StringBuffer<64> uidName;
						GetMappingHandleName(uidName, uid);
						if (shm_unlink(uidName.data) == 0)
							;// logger.Info("Removed old shared memory %s", uidName.data);
						remove(lockFile.data);
					}
					else
					{
						g_mappingUidCounter = uid;
					}
					close(lockFd);
				});

			if (g_mappingUidCounter)
				logger.Info("Starting shared memory files at %u", g_mappingUidCounter.load());
		}
		lock.Leave();

		// Let's find a free shm name
		StringBuffer<128> lockFile;
		u64 uid;
		int shmFd;
		int lockFd;

		while (true)
		{
			uid = ++g_mappingUidCounter;

			lockFile.Clear().Append(lockDir).EnsureEndsWithSlash().AppendValue(uid);

			lockFd = open(lockFile.data, O_CREAT | O_RDWR | O_NOFOLLOW | O_EXCL, S_IRUSR | S_IWUSR | O_CLOEXEC);
			if (lockFd == -1)
			{
				if (errno == EEXIST)
					continue;
				UBA_ASSERTF(false, "Failed to open/create %s (%s)", lockFile.data, strerror(errno));
				continue;
			}

			if (flock(lockFd, LOCK_EX | LOCK_NB) != 0) // Some other process is using this one
			{
				close(lockFd);
				continue;
			}

			StringBuffer<64> uidName;
			GetMappingHandleName(uidName, uid);

			int oflags = O_CREAT | O_RDWR | O_NOFOLLOW | O_EXCL;
			shmFd = shm_open(uidName.data, oflags, S_IRUSR | S_IWUSR);
			if (shmFd != -1)
				break;

			shm_unlink(uidName.data);
			shmFd = shm_open(uidName.data, oflags, S_IRUSR | S_IWUSR);
			if (shmFd != -1)
				break;

			remove(lockFile.data);
			close(lockFd);
			SetLastError(errno);
			UBA_ASSERTF(false, "Failed to create shm %s after getting lock-file %s (%s)", uidName.data, lockFile.data, strerror(errno));
			return {};
		}

		if (maxSize != 0)
		{
			if (ftruncate(shmFd, (s64)maxSize) == -1)
			{
				SetLastError(errno);
				close(shmFd);
				remove(lockFile.data);
				close(lockFd);
				//UBA_ASSERTF(false, "Failed to truncate file mapping '%s' to size %llu (%s)", name, maxSize, strerror(errno));
				return {};
			}
		}
		return { shmFd, lockFd, uid };
#endif
	}

	FileMappingHandle CreateFileMappingW(FileHandle file, u32 protect, u64 maxSize, const tchar* hint)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().createFileMapping);
#if PLATFORM_WINDOWS
		return { InternalCreateFileMappingW(asHANDLE(file), protect, (DWORD)ToHigh(maxSize), ToLow(maxSize), NULL) };
#else
		FileMappingHandle h;
		int fd = asFileDescriptor(file);
		if (maxSize && (protect & (~PAGE_READONLY)) != 0)
		{
#if PLATFORM_MAC // For some reason lseek+write does not work on apple silicon platform
			if (ftruncate(fd, maxSize) == -1)
			{
				UBA_ASSERTF(false, "ftruncate to %llu on fd %i failed for %s: %s\n", maxSize, fd, hint, strerror(errno));
				return h;
			}
#else
			if (lseek(fd, maxSize - 1, SEEK_SET) != maxSize - 1)
			{
				UBA_ASSERTF(false, "lseek to %llu failed for %s: %s\n", maxSize - 1, hint, strerror(errno));
				return h;
			}

			errno = 0;
			int res = write(fd, "", 1);
			if (res != 1)
			{
				UBA_ASSERTF(false, "write one byte at %llu on fd %i (%s) failed (res: %i): %s\n", maxSize - 1, fd, hint, res, strerror(errno));
				return h;
			}
#endif
		}

		h.shmFd = fd;
		return h;
#endif
	}

	u8* MapViewOfFile(FileMappingHandle fileMappingObject, u32 desiredAccess, u64 offset, u64 bytesToMap)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().mapViewOfFile);
#if PLATFORM_WINDOWS
		return (u8*)::MapViewOfFile(fileMappingObject.handle, desiredAccess, (DWORD)ToHigh(offset), ToLow(offset), bytesToMap);
#else
		int prot = 0;
		if (desiredAccess & FILE_MAP_READ)
			prot |= PROT_READ;
		if (desiredAccess & FILE_MAP_WRITE)
			prot |= PROT_WRITE;
		UBA_ASSERT(fileMappingObject.IsValid());
		int shmFd = fileMappingObject.shmFd;
		void* rptr = mmap(NULL, bytesToMap, prot, MAP_SHARED, shmFd, s64(offset));
		if (rptr != MAP_FAILED)
			return (u8*)rptr;
		//UBA_ASSERTF(false, "Failed to map file with fd %i, desiredAccess %u offset %llu, bytesToMap %llu (%s)", fd, desiredAccess, offset, bytesToMap, strerror(errno));
		SetLastError(errno);
		return nullptr;
#endif
	}

	bool MapViewCommit(void* address, u64 size)
	{
#if PLATFORM_WINDOWS
		return ::VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE);
#else
		return true;
#endif
	}

	bool UnmapViewOfFile(const void* lpBaseAddress, u64 bytesToUnmap, const tchar* hint)
	{
		ExtendedTimerScope ts(SystemStats::GetCurrent().unmapViewOfFile);
#if PLATFORM_WINDOWS
		(void)bytesToUnmap; return ::UnmapViewOfFile(lpBaseAddress);
#else
		UBA_ASSERTF(bytesToUnmap, TC("bytesToUnmap is zero unmapping %p (%s)"), lpBaseAddress, hint);
		if (munmap((void*)lpBaseAddress, bytesToUnmap) == 0)
			return true;
		UBA_ASSERT(false);
		return false;
#endif
	}

	bool CloseFileMapping(FileMappingHandle h)
	{
#if PLATFORM_WINDOWS
		//if (h.handle)
		//{
		//	--g_fileMappingCount;
		//	StringBuffer<256> sb;
		//	sb.Appendf(TC("FILEMAPPING COUNT: %llu\r\n"), g_fileMappingCount.load());
		//	OutputDebugStringW(sb.data);
		//}

		return CloseHandle(h.handle);
#else
		if (h.shmFd == -1)
			return true;
		if (h.uid == ~u64(0))
			return true;
		if (close(h.shmFd) != 0)
			UBA_ASSERT(false);

		StringBuffer<64> uidName;
		GetMappingHandleName(uidName, h.uid);
		if (shm_unlink(uidName.data) != 0)
		{
			SetLastError(errno);
			UBA_ASSERTF(false, "Failed to unlink %s (%s)", uidName.data, strerror(errno));
			return false;
		}

		StringBuffer<128> lockFile;
		lockFile.Append("/tmp/uba_shm_locks").EnsureEndsWithSlash().AppendValue(h.uid);
		remove(lockFile.data);
		close(h.lockFd);
		return true;
#endif
	}

	bool DuplicateFileMapping(ProcHandle hSourceProcessHandle, FileMappingHandle hSourceHandle, ProcHandle hTargetProcessHandle, FileMappingHandle* lpTargetHandle, u32 dwDesiredAccess, bool bInheritHandle, u32 dwOptions)
	{
#if PLATFORM_WINDOWS
		return DuplicateHandle((HANDLE)hSourceProcessHandle, hSourceHandle.handle, (HANDLE)hTargetProcessHandle, &lpTargetHandle->handle, dwDesiredAccess, bInheritHandle, dwOptions);
#else
		UBA_ASSERT(false);
		return false;
#endif
	}

	FileMappingBuffer::FileMappingBuffer(Logger& logger, WorkManager* workManager)
	:	m_logger(logger)
	,	m_workManager(workManager)
	{
		m_pageSize = 64*1024;
	}

	FileMappingBuffer::~FileMappingBuffer()
	{
		CloseMappingStorage(m_storage[MappedView_Transient]);
		CloseMappingStorage(m_storage[MappedView_Persistent]);
	}

	bool FileMappingBuffer::AddTransient(const tchar* name)
	{
		MappingStorage& storage = m_storage[MappedView_Transient];
		for (u32 i = 0; i != sizeof_array(storage.files); ++i)
		{
			File& file = storage.files[sizeof_array(storage.files) - (++storage.fileCount)];
			file.name = name;
			storage.availableFiles[storage.availableFilesCount++] = &file;
		}
		return true;
	}

	bool FileMappingBuffer::AddPersistent(const tchar* name, FileHandle fileHandle, u64 size, u64 capacity)
	{
		FileMappingHandle sparseMemoryHandle = uba::CreateFileMappingW(fileHandle, PAGE_READWRITE, capacity, name);
		if (!sparseMemoryHandle.IsValid())
		{
			m_logger.Error(TC("Failed to create file mapping (%s)"), LastErrorToText().data);
			return false;
		}


		MappingStorage& storage = m_storage[MappedView_Persistent];
		File& f = storage.files[storage.fileCount++];
		f.name = name;
		f.file = fileHandle;
		f.handle = sparseMemoryHandle;
		f.size = size;
		f.capacity = capacity;

		PushFile(storage, &f);
		return true;
	}

	void FileMappingBuffer::CloseDatabase()
	{
		CloseMappingStorage(m_storage[MappedView_Persistent]);
	}

	MappedView FileMappingBuffer::AllocAndMapView(FileMappingType type, u64 size, u64 alignment, const tchar* hint, bool allowShrink)
	{
		MappingStorage& storage = m_storage[type];
		File* file = PopFile(storage, size, alignment);

		if (allowShrink)
			return AllocAndMapViewNoLock(*file, size, alignment, hint);

		MappedView res = AllocAndMapViewNoLock(*file, size, alignment, hint);

		PushFile(storage, file);
		return res;
	}

	MappedView FileMappingBuffer::AllocAndMapViewNoLock(File& f, u64 size, u64 alignment, const tchar* hint)
	{
		MappedView res;

		u64 offset = AlignUp(f.size, alignment);
		u64 alignedOffsetStart = AlignUp(offset - (m_pageSize - 1), m_pageSize);

		u64 newOffset = offset + size;
		u64 alignedOffsetEnd = AlignUp(newOffset, m_pageSize);
		
		if (alignedOffsetEnd > f.capacity)
		{
			m_logger.Error(TC("%s - AllocAndMapView has reached max capacity %llu trying to allocate %llu for %s"), f.name, f.capacity, size, hint);
			return res;
		}

		u64 mapSize = alignedOffsetEnd - alignedOffsetStart;
		u8* data = MapViewOfFile(f.handle, FILE_MAP_WRITE, alignedOffsetStart, mapSize);
		if (!data)
		{
			m_logger.Error(TC("%s - AllocAndMapView failed to map view of file for %s with size %llu and offset %llu (%s)"), f.name, hint, size, alignment, LastErrorToText().data);
			return res;
		}

		u64 committedBefore = AlignUp(offset, m_pageSize);
		u64 commitedAfter = AlignUp(newOffset, m_pageSize);

		if (f.commitOnAlloc && committedBefore != commitedAfter)
		{
			u64 commitStart = committedBefore - alignedOffsetStart;
			u64 commitSize = commitedAfter - committedBefore;
			if (!MapViewCommit(data + commitStart, commitSize))
			{
				UnmapViewOfFile(data, mapSize, hint);
				m_logger.Error(TC("%s - Failed to allocate memory for %s (%s)"), f.name, hint, LastErrorToText().data);
				return res;
			}

			PrefetchVirtualMemory(data + commitStart, commitSize);
		}

		f.size = newOffset;
		++f.activeMapCount;

		res.handle = f.handle;
		res.offset = offset;
		res.size = size;
		res.memory = data + (offset - alignedOffsetStart);
		return res;
	}

	FileMappingBuffer::File& FileMappingBuffer::GetFile(FileMappingHandle handle, u8& outStorageIndex)
	{
		for (u8 storageI = 0; storageI != 2; ++storageI)
		{
			MappingStorage& storage = m_storage[storageI];
			for (u32 i = 0; i != storage.fileCount; ++i)
				if (storage.files[i].handle == handle)
				{
					outStorageIndex = storageI;
					return storage.files[i];
				}
		}
		UBA_ASSERT(false);
		static FileMappingBuffer::File error;
		return error;
	}

	FileMappingBuffer::File* FileMappingBuffer::PopFile(MappingStorage& storage, u64 size, u64 alignment)
	{
		while (true)
		{
			SCOPED_WRITE_LOCK(storage.availableFilesLock, lock);
			for (u32 i = storage.availableFilesCount; i!=0; --i)
			{
				u32 index = i - 1;
				auto file = storage.availableFiles[index];
				
				if (!file->handle.IsValid())
				{
					UBA_ASSERT(&storage == &m_storage[MappedView_Transient]);
					u64 capacity = (IsWindows ? 32ull : 8ull) * 1024 * 1024 * 1024; // Linux can't have larger than 8gb
					file-> handle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE | SEC_RESERVE, capacity);
					if (!file->handle.IsValid())
					{
						m_logger.Error(TC("%s - Failed to create memory map (%s)"), file->name, LastErrorToText().data);
						return nullptr;
					}
					file->commitOnAlloc = true;
					file->capacity = capacity;
				}
				else
				{
					u64 newSize = AlignUp(file->size, alignment) + size;
					if (newSize > file->capacity)
						continue;
				}

				storage.availableFiles[index] = storage.availableFiles[storage.availableFilesCount - 1];
				--storage.availableFilesCount;
				return file;
			}
			lock.Leave();
			storage.availableFilesEvent.IsSet();
		}
		return nullptr;
	}

	void FileMappingBuffer::PushFile(MappingStorage& storage, File* file)
	{
		SCOPED_WRITE_LOCK(storage.availableFilesLock, lock);
		storage.availableFiles[storage.availableFilesCount++] = file;
		storage.availableFilesEvent.Set();
	}

	void FileMappingBuffer::CloseMappingStorage(MappingStorage& storage)
	{
		u32 locksTaken = 0;
		while (locksTaken < storage.fileCount)
		{
			SCOPED_WRITE_LOCK(storage.availableFilesLock, lock);
			if (storage.availableFilesCount == 0)
			{
				lock.Leave();
				storage.availableFilesEvent.IsSet();
			}
			++locksTaken;
		}

		for (u32 i=0; i!=storage.fileCount; ++i)
		{
			//UBA_ASSERT(!storage.files[i].activeMapCount);
			CloseFileMapping(storage.files[i].handle);
			CloseFile(nullptr, storage.files[i].file);
		}
		storage.fileCount = 0;
		storage.availableFilesCount = 0;
	}

	MappedView FileMappingBuffer::MapView(FileMappingHandle handle, u64 offset, u64 size, const tchar* hint)
	{
		UBA_ASSERT(handle.IsValid());// && (handle == m_files[0].handle || handle == m_files[1].handle));
		u8 storageIndex = 255;
		File& file = GetFile(handle, storageIndex);

		u64 alignedOffsetStart = AlignUp(offset - (m_pageSize - 1), m_pageSize);

		u64 newOffset = offset + size;
		u64 alignedOffsetEnd = AlignUp(newOffset, m_pageSize);

		MappedView res;

		u8* data = MapViewOfFile(handle, FILE_MAP_WRITE, alignedOffsetStart, alignedOffsetEnd - alignedOffsetStart);
		if (!data)
		{
			m_logger.Error(TC("%s - MapView failed to map view of file for %s with size %llu and offset %llu (%s)"), file.name, hint, size, offset, LastErrorToText().data);
			return res;
		}

		++file.activeMapCount;

		res.handle = handle;
		res.offset = offset;
		res.size = size;
		res.memory = data + (offset - alignedOffsetStart);
		return res;
	}

	void FileMappingBuffer::UnmapView(MappedView view, const tchar* hint_, u64 newSize)
	{
		if (!view.handle.IsValid())
			return;

		auto unmap = [=](const tchar* hint)
			{
				u8 storageIndex = 255;
				File& file = GetFile(view.handle, storageIndex);

				u64 alignedOffsetStart = AlignUp(view.offset - (m_pageSize - 1), m_pageSize);
				u64 alignedOffsetEnd = AlignUp(view.offset + view.size, m_pageSize);

				u8* memory = view.memory - (view.offset - alignedOffsetStart);
				u64 mapSize = alignedOffsetEnd - alignedOffsetStart;
				if (!UnmapViewOfFile(memory, mapSize, hint))
				{
					m_logger.Error(TC("%s - Failed to unmap view on address %llx (offset %llu) - %s (%s)"), file.name, u64(memory), view.offset, hint, LastErrorToText().data);
				}

				if (newSize != InvalidValue)
				{
					if (newSize != view.size)
					{
						UBA_ASSERT(!file.commitOnAlloc);
						UBA_ASSERTF(newSize < view.size, TC("%s - Reserved too little memory. Reserved %llu, needed %llu for %s"), file.name, view.size, newSize, hint);
						file.size -= view.size - newSize;
					}

					MappingStorage& storage = m_storage[storageIndex];
					PushFile(storage, &file);
				}

				--file.activeMapCount;
			};
		if (m_workManager && newSize == InvalidValue)
			m_workManager->AddWork([=, h = TString(hint_)]() { unmap(h.c_str()); }, 1, TC("UnmapView"));
		else
			unmap(hint_);
	}

	void FileMappingBuffer::GetSizeAndCount(FileMappingType type, u64& outSize, u32& outCount)
	{
		MappingStorage& storage = m_storage[type];
		outSize = 0;
		outCount = 0;
		for (u32 i = 0; i != storage.fileCount; ++i)
		{
			if (storage.files[i].handle.IsValid())
				++outCount;
			outSize += storage.files[i].size;
		}
	}








	FileMappingAllocator::FileMappingAllocator(Logger& logger, const tchar* name)
	:	m_logger(logger)
	,	m_name(name)
	{
	}

	FileMappingAllocator::~FileMappingAllocator()
	{
		if (m_mappingHandle.IsValid())
			CloseFileMapping(m_mappingHandle);
	}

	bool FileMappingAllocator::Init(u64 blockSize, u64 capacity)
	{
		m_mappingHandle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE|SEC_RESERVE, capacity);
		if (!m_mappingHandle.IsValid())
			return m_logger.Error(TC("%s - Failed to create memory map (%s)"), m_name, LastErrorToText().data);

		m_blockSize = blockSize;
		m_pageSize = 64*1024;
		m_capacity = capacity;
		return true;
	}

	FileMappingAllocator::Allocation FileMappingAllocator::Alloc(const tchar* hint)
	{
		SCOPED_WRITE_LOCK(m_mappingLock, lock);

		u64 index = m_mappingCount;
		bool needCommit = false;
		if (!m_availableBlocks.empty())
		{
			auto it = m_availableBlocks.begin();
			index = *it;
			m_availableBlocks.erase(it);
		}
		else
		{
			++m_mappingCount;
			needCommit = true;
		}
		lock.Leave();

		u64 offset = index*m_blockSize;
		u8* data = MapViewOfFile(m_mappingHandle, FILE_MAP_READ|FILE_MAP_WRITE, offset, m_blockSize);
		if (!data)
		{
			if (m_capacity < m_mappingCount*m_blockSize)
				m_logger.Error(TC("%s - Out of capacity (%llu) need to bump capacity for %s (%s)"), m_name, m_capacity, hint, LastErrorToText().data);
			else
				m_logger.Error(TC("%s - Alloc failed to map view of file for %s (%s)"), m_name, hint, LastErrorToText().data);
			return { {}, 0, 0 };
		}

		if (needCommit)
		{
			if (!MapViewCommit(data, m_blockSize))
			{
				m_logger.Error(TC("%s - Failed to allocate memory for %s (%s)"), m_name, hint, LastErrorToText().data);
				return { {}, 0, 0 };
			}
		}
		return {m_mappingHandle, offset, data};
	}

	void FileMappingAllocator::Free(Allocation allocation)
	{
		UBA_ASSERT(allocation.handle == m_mappingHandle);
		if (!UnmapViewOfFile(allocation.memory, m_blockSize, m_name))
			m_logger.Error(TC("%s - Failed to unmap view of file (%s)"), m_name, LastErrorToText().data);
		u64 index = allocation.offset / m_blockSize;
		SCOPED_WRITE_LOCK(m_mappingLock, lock);
		m_availableBlocks.insert(index);
	}
}

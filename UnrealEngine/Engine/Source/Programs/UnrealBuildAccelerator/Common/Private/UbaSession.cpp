// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaSession.h"
#include "UbaBottleneck.h"
#include "UbaFileAccessor.h"
#include "UbaProcess.h"
#include "UbaStorage.h"
#include "UbaDirectoryIterator.h"
#include "UbaApplicationRules.h"
#include "UbaPathUtils.h"
#include "UbaProtocol.h"
#include "UbaWorkManager.h"

#if PLATFORM_WINDOWS
#include "UbaWinBinDependencyParser.h"
#include <powerbase.h>
#pragma comment(lib, "Powrprof.lib")
#endif

// Used to get Memory Information
#if PLATFORM_MAC
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/mach.h>
extern char **environ;
#endif

//////////////////////////////////////////////////////////////////////////////

#if PLATFORM_WINDOWS
typedef struct _PROCESSOR_POWER_INFORMATION {
    ULONG  Number;
    ULONG  MaxMhz;
    ULONG  CurrentMhz;
    ULONG  MhzLimit;
    ULONG  MaxIdleState;
    ULONG  CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;
#endif

namespace uba
{
	bool g_dummy;

	ProcessStartInfo::ProcessStartInfo() = default;
	ProcessStartInfo::~ProcessStartInfo() = default;
	ProcessStartInfo::ProcessStartInfo(const ProcessStartInfo&) = default;

	ProcessHandle::ProcessHandle()
	:	m_process(nullptr)
	{
	}
	ProcessHandle::~ProcessHandle()
	{
		if (m_process)
			m_process->Release();
	}

	ProcessHandle::ProcessHandle(const ProcessHandle& o)
	{
		m_process = o.m_process;
		if (m_process)
			m_process->AddRef();
	}

	ProcessHandle::ProcessHandle(ProcessHandle&& o) noexcept
	{
		m_process = o.m_process;
		o.m_process = nullptr;
	}

	ProcessHandle& ProcessHandle::operator=(const ProcessHandle& o)
	{
		if (&o == this)
			return *this;
		if (o.m_process)
			o.m_process->AddRef();
		if (m_process)
			m_process->Release();
		m_process = o.m_process;
		return *this;
	}

	ProcessHandle& ProcessHandle::operator=(ProcessHandle&& o) noexcept
	{
		if (&o == this)
			return *this;
		if (o.m_process == m_process)
		{
			if (o.m_process)
			{
				o.m_process->Release();
				o.m_process = nullptr;
			}
			return *this;
		}
		if (m_process)
			m_process->Release();
		m_process = o.m_process;
		o.m_process = nullptr;
		return *this;
	}

	const ProcessStartInfo& ProcessHandle::GetStartInfo() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetStartInfo();
	}
	u32 ProcessHandle::GetId() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetId();
	}
	u32 ProcessHandle::GetExitCode() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetExitCode();
	}
	bool ProcessHandle::HasExited() const
	{
		UBA_ASSERT(m_process);
		return m_process->HasExited();
	}
	bool ProcessHandle::WaitForExit(u32 millisecondsTimeout) const
	{
		UBA_ASSERT(m_process);
		return m_process->WaitForExit(millisecondsTimeout);
	}
	const Vector<ProcessLogLine>& ProcessHandle::GetLogLines() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetLogLines();
	}
	const Vector<u8>& ProcessHandle::GetTrackedInputs() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetTrackedInputs();
	}
	u64 ProcessHandle::GetTotalProcessorTime() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetTotalProcessorTime();
	}
	u64 ProcessHandle::GetTotalWallTime() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetTotalWallTime();
	}
	void ProcessHandle::Cancel(bool terminate) const
	{
		UBA_ASSERT(m_process);
		return m_process->Cancel(terminate);
	}
	const tchar* ProcessHandle::GetExecutingHost() const
	{
		UBA_ASSERT(m_process);
		return m_process->GetExecutingHost();
	}
	bool ProcessHandle::IsRemote() const
	{
		UBA_ASSERT(m_process);
		return m_process->IsRemote();
	}
	bool ProcessHandle::IsDetoured() const
	{
		UBA_ASSERT(m_process);
		return m_process->IsDetoured();
	}
	ProcessHandle::ProcessHandle(Process* process)
	{
		m_process = process;
		process->AddRef();
	}

	void Session::AddEnvironmentVariableNoLock(const tchar* key, const tchar* value)
	{
		m_environmentVariables.insert(m_environmentVariables.end(), key, key + TStrlen(key));
		m_environmentVariables.push_back('=');
		m_environmentVariables.insert(m_environmentVariables.end(), value, value + TStrlen(value));
		m_environmentVariables.push_back(0);
	}

	bool Session::WriteDirectoryEntriesInternal(DirectoryTable::Directory& dir, const StringKey& dirKey, const tchar* dirPath, bool isRefresh, u32& outTableOffset)
	{
		if (dir.tableOffset != InvalidTableOffset && !isRefresh)
		{
			isRefresh = true;
		}

		auto& dirTable = m_directoryTable;

		u32 volumeSerial = 0;
		u32 dirAttributes = 0;
		u64 fileIndex = 0;

		Vector<u8> memoryBlock;
		memoryBlock.resize(4096);
		u64 written = 0;
		u32 itemCount = 0;

		StringKeyHasher hasher;
		u32 dirPathLen = TStrlen(dirPath);
		if (dirPathLen)
		{
			StringBuffer<> forHash;
			forHash.Append(dirPath, dirPathLen);
			if (CaseInsensitiveFs)
				forHash.MakeLower();
			hasher.Update(forHash.data, forHash.count);
		}

		#if UBA_DEBUG_LOGGER
		g_debugLogger.BeginScope();
		auto dg = MakeGuard([]() { g_debugLogger.EndScope(); });
		g_debugLogger.Info(TC("TRACKDIR %s\n"), dirPath);
		#endif


		BinaryWriter memoryWriter(memoryBlock.data(), 0, memoryBlock.size());

		bool res = TraverseDir(m_logger, dirPathLen ? dirPath : TC("/"),
			[&](const DirectoryEntry& e)
			{
				StringBuffer<256> fileNameForHash;
				fileNameForHash.Append(PathSeparator).Append(e.name, e.nameLen);
				if (CaseInsensitiveFs)
					fileNameForHash.MakeLower();

				StringKey fileKey = ToStringKey(hasher, fileNameForHash.data, fileNameForHash.count);
				auto res = dir.files.try_emplace(fileKey, ~0u);
				if (!res.second)
					return;
				UBA_ASSERT(e.attributes);
				memoryWriter.WriteString(e.name, e.nameLen);

				#if UBA_DEBUG_LOGGER
				g_debugLogger.Info(TC("    %s (Size: %llu, Key: %s, Id: %llu)\n"), e.name, e.size, KeyToString(fileKey).data, e.id);
				#endif

				u64 id = e.id;
				if (id == 0xffffffffffffffffllu) // When using projfs we might not have the file yet and in that case we need to make this up.
					id = ++m_fileIndexCounter;

				res.first->second = u32(memoryWriter.GetPosition()); // Temporary offset that will be used further down to calculate the real offset
				memoryWriter.WriteU64(e.lastWritten);
				memoryWriter.WriteU32(e.attributes);
				memoryWriter.WriteU32(e.volumeSerial);
				memoryWriter.WriteU64(id);
				memoryWriter.WriteU64(e.size);

				FileEntryAdded(res.first->first, e.lastWritten, e.size);


				++itemCount;
				if (memoryWriter.GetPosition() > memoryBlock.size() - MaxPath)
				{
					memoryBlock.resize(memoryBlock.size() * 2);
					memoryWriter.ChangeData(memoryBlock.data(), memoryBlock.size());
				}
			}, true,
			[&](const DirectoryInfo& e)
			{
				volumeSerial = e.volumeSerial;
				dirAttributes = e.attributes;
				fileIndex = e.id;
			});
		if (!res)
			return false;

		written = memoryWriter.GetPosition();

		u64 storageSize = sizeof(StringKey) + Get7BitEncodedCount(dir.tableOffset) + Get7BitEncodedCount(itemCount) + written;

		u32 tableOffset;

		SCOPED_WRITE_LOCK(dirTable.m_memoryLock, memoryLock);
		u32 writePos = dirTable.m_memorySize;
		BinaryWriter tableWriter(dirTable.m_memory + dirTable.m_memorySize);

		if (isRefresh)
		{
			tableWriter.Write7BitEncoded(storageSize);
			tableWriter.WriteStringKey(dirKey);
			tableOffset = writePos + u32(tableWriter.GetPosition());
			tableWriter.Write7BitEncoded(dir.tableOffset);
		}
		else
		{
			storageSize += sizeof(u32)*2 + sizeof(u64);
			tableWriter.Write7BitEncoded(storageSize);
			tableWriter.WriteStringKey(dirKey);
			tableOffset = writePos + u32(tableWriter.GetPosition());
			tableWriter.Write7BitEncoded(dir.tableOffset);
			UBA_ASSERT(dirAttributes != 0);
			tableWriter.WriteU32(dirAttributes);
			tableWriter.WriteU32(volumeSerial);
			tableWriter.WriteU64(fileIndex);
		}

		tableWriter.Write7BitEncoded(itemCount);
		u32 filesOffset = writePos + u32(tableWriter.GetPosition());
		tableWriter.WriteBytes(memoryBlock.data(), written);
		dirTable.m_memorySize += u32(tableWriter.GetPosition());
		UBA_ASSERT(dirTable.m_memorySize < DirTableMemSize);
		
		memoryLock.Leave();

		// Update offsets to be relative to full memory
		for (auto& kv : dir.files)
			kv.second = filesOffset + kv.second;


		outTableOffset = tableOffset;
		dir.tableOffset = tableOffset;
		return true;
	}

	void Session::WriteDirectoryEntriesRecursive(const StringKey& dirKey, tchar* dirPath, u32& outTableOffset)
	{
		auto& dirTable = m_directoryTable;
		SCOPED_WRITE_LOCK(dirTable.m_lookupLock, lookupLock);
		auto res = dirTable.m_lookup.try_emplace(dirKey, dirTable.m_memoryBlock);
		DirectoryTable::Directory& dir = res.first->second;
		lookupLock.Leave();

		SCOPED_WRITE_LOCK(dir.lock, dirLock);

		if (dir.parseOffset == 1)
		{
			outTableOffset = dir.tableOffset;
			return;
		}

		if (!WriteDirectoryEntriesInternal(dir, dirKey, dirPath, false, outTableOffset))
		{
			outTableOffset = InvalidTableOffset;
			dir.parseOffset = 2;
		}
		else
		{
			dir.parseOffset = 1;
		}

		u64 dirlen = TStrlen(dirPath);

		if (!dirlen) // This is for non-windows.. '/' is actually empty to get hashes correct
			return;

		// scan backwards first
		tchar* rit = (tchar*)dirPath + dirlen - 2;
		while (rit > dirPath)
		{
			if (*rit != PathSeparator)
			{
				--rit;
				continue;
			}
			break;
		}

		if (rit <= dirPath) // There are no path separators left, this is the drive
		{
			*(dirPath) = 0;
			return;
		}

		*(rit) = 0;

		StringBuffer<> parentDirForHash;
		parentDirForHash.Append(dirPath, u64(rit - dirPath));
		if (CaseInsensitiveFs)
			parentDirForHash.MakeLower();
		StringKey parentKey = ToStringKey(parentDirForHash);

		// Traverse through ancestors and populate them, this is an optimization
		u32 parentOffset;
		WriteDirectoryEntriesRecursive(parentKey, dirPath, parentOffset);
	}

	u32 Session::WriteDirectoryEntries(const StringKey& dirKey, tchar* dirPath, u32& outTableOffset)
	{
		auto& dirTable = m_directoryTable;
		WriteDirectoryEntriesRecursive(dirKey, dirPath, outTableOffset);
		SCOPED_READ_LOCK(dirTable.m_memoryLock, memoryLock);
		return dirTable.m_memorySize;
	}

	u32 Session::AddFileMapping(StringKey fileNameKey, const tchar* fileName, const tchar* newFileName, u64 fileSize)
	{
		UBA_ASSERT(fileNameKey != StringKeyZero);
		SCOPED_WRITE_LOCK(m_fileMappingTableLookupLock, lookupLock);
		auto insres = m_fileMappingTableLookup.try_emplace(fileNameKey);
		FileMappingEntry& entry = insres.first->second;
		lookupLock.Leave();

		SCOPED_WRITE_LOCK(entry.lock, entryCs);

		if (entry.handled)
		{
			entryCs.Leave();
			SCOPED_READ_LOCK(m_fileMappingTableMemLock, lookupCs2);
			return entry.success ? m_fileMappingTableSize : 0;
		}

		entry.size = fileSize;
		entry.isDir = false;
		entry.success = true;
		entry.mapping = {};
		entry.handled = true;

		SCOPED_WRITE_LOCK(m_fileMappingTableMemLock, lock);
		BinaryWriter writer(m_fileMappingTableMem, m_fileMappingTableSize);
		writer.WriteStringKey(fileNameKey);
		writer.WriteString(newFileName);
		writer.Write7BitEncoded(fileSize);
		u32 newSize = (u32)writer.GetPosition();
		m_fileMappingTableSize = (u32)newSize;
		return newSize;
	}

	bool Session::CreateMemoryMapFromFile(MemoryMap& out, StringKey fileNameKey, const tchar* fileName, bool isCompressed, u64 alignment)
	{
		TimerScope ts(Stats().waitMmapFromFile);

		SCOPED_WRITE_LOCK(m_fileMappingTableLookupLock, lookupLock);
		auto insres = m_fileMappingTableLookup.try_emplace(fileNameKey);
		FileMappingEntry& entry = insres.first->second;
		lookupLock.Leave();

		SCOPED_WRITE_LOCK(entry.lock, entryCs);

		if (entry.handled)
		{
			entryCs.Leave();
			if (!entry.success)
				return false;
			out.size = entry.size;
			if (entry.mapping.IsValid())
				Storage::GetMappingString(out.name, entry.mapping, entry.mappingOffset);
			else
				out.name.Append(entry.isDir ? TC("$d") : TC("$f"));
			return true;
		}

		ts.Cancel();
		TimerScope ts2(Stats().createMmapFromFile);

		out.size = 0;

		entry.handled = true;

		bool isDir = false;
		FileHandle fileHandle = uba::CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, DefaultAttributes());
		if (fileHandle == InvalidFileHandle)
		{
			u32 error = GetLastError();
			if (error == ERROR_ACCESS_DENIED || error == ERROR_PATH_NOT_FOUND) // Probably directory? .. path not found can be returned if path is the drive ('e:\' etc)
			{
				fileHandle = uba::CreateFileW(fileName, 0, 0x00000007, 0x00000003, FILE_FLAG_BACKUP_SEMANTICS);
				if (fileHandle == InvalidFileHandle)
					return m_logger.Error(TC("Failed to open file %s (%s)"), fileName, LastErrorToText().data);

				isDir = true;
			}
			else
				return m_logger.Error(TC("Failed to open file %s (%u)"), fileName, error);
		}
		auto _ = MakeGuard([&](){ uba::CloseFile(fileName, fileHandle); });

		u64 size = 0;

		if (!isDir)
		{
			if (isCompressed)
			{
				if (!ReadFile(m_logger, fileName, fileHandle, &size, sizeof(u64)))
					return m_logger.Error(TC("Failed to read first bytes from file %s (%s)"), fileName, LastErrorToText().data);
			}
			else
			{
				FileInformation info;
				if (!GetFileInformationByHandle(info, m_logger, fileName, fileHandle))
					return false;

				size = info.size;
			}
		}

		if (isDir || size == 0)
		{
			if (isDir)
				out.name.Append(TC("$d"));
			else
				out.name.Append(TC("$f"));
		}
		else
		{
			auto mappedView = m_fileMappingBuffer.AllocAndMapView(MappedView_Transient, size, alignment, fileName, false);
			auto unmapGuard = MakeGuard([&](){ m_fileMappingBuffer.UnmapView(mappedView, fileName); });

			if (isCompressed)
			{
				if (!m_storage.DecompressFileToMemory(fileName, fileHandle, mappedView.memory, size))
					return false;
			}
			else
			{
				if (!ReadFile(m_logger, fileName, fileHandle, mappedView.memory, size))
					return false;
			}

			unmapGuard.Execute();

			entry.mappingOffset = mappedView.offset;
			Storage::GetMappingString(out.name, mappedView.handle, mappedView.offset);
			entry.mapping = mappedView.handle;
		}

		entry.success = true;

		{
			SCOPED_WRITE_LOCK(m_fileMappingTableMemLock, lock);
			BinaryWriter writer(m_fileMappingTableMem, m_fileMappingTableSize);
			writer.WriteStringKey(fileNameKey);
			writer.WriteString(out.name);
			writer.Write7BitEncoded(size);
			m_fileMappingTableSize = (u32)writer.GetPosition();
		}

		entry.isDir = isDir;
		entry.size = size;
		
		out.size = size;
		return true;
	}

	bool Session::CreateMemoryMapFromView(MemoryMap& out, StringKey fileNameKey, const tchar* fileName, const CasKey& casKey, u64 alignment)
	{
		//StringBuffer<> workName;
		//u32 len = TStrlen(fileName);
		//workName.Append(TC("MM:")).Append(len > 30 ? fileName + (len - 30) : fileName);
		//TrackWorkScope tws(*m_workManager, workName.data);

		SCOPED_WRITE_LOCK(m_fileMappingTableLookupLock, lookupLock);
		auto insres = m_fileMappingTableLookup.try_emplace(fileNameKey);
		FileMappingEntry& entry = insres.first->second;
		lookupLock.Leave();

		SCOPED_WRITE_LOCK(entry.lock, entryCs);

		if (entry.handled)
		{
			entryCs.Leave();
			if (!entry.success)
				return false;
			out.size = entry.size;
			if (entry.mapping.IsValid())
				Storage::GetMappingString(out.name, entry.mapping, entry.mappingOffset);
			else
				out.name.Append(entry.isDir ? TC("$d") : TC("$f"));
			return true;
		}

		out.size = 0;

		entry.handled = true;

		MappedView mappedViewRead = m_storage.MapView(casKey, fileName);
		if (!mappedViewRead.handle.IsValid())
			return false;

		u64 size = InvalidValue;

		if (mappedViewRead.isCompressed)
		{
			auto mvrg = MakeGuard([&](){ m_storage.UnmapView(mappedViewRead, fileName); });
			u8* readMemory = mappedViewRead.memory;
			size = *(u64*)readMemory;
			readMemory += 8;

			if (size == 0)
			{
				out.name.Append(TC("$f"));
			}
			else
			{
				auto mappedViewWrite = m_fileMappingBuffer.AllocAndMapView(MappedView_Transient, size, alignment, fileName);
				auto unmapGuard = MakeGuard([&](){ m_fileMappingBuffer.UnmapView(mappedViewWrite, fileName); });

				if (!m_storage.DecompressMemoryToMemory(readMemory, mappedViewWrite.memory, size, fileName))
					return false;
				unmapGuard.Execute();

				entry.mappingOffset = mappedViewWrite.offset;
				Storage::GetMappingString(out.name, mappedViewWrite.handle, mappedViewWrite.offset);
				entry.mapping = mappedViewWrite.handle;
			}
			mvrg.Execute();
		}
		else
		{
			UBA_ASSERT(mappedViewRead.memory == nullptr);
			entry.mappingOffset = mappedViewRead.offset;
			Storage::GetMappingString(out.name, mappedViewRead.handle, mappedViewRead.offset);
			entry.mapping = mappedViewRead.handle;
			size = mappedViewRead.size;
		}
		entry.success = true;

		{
			SCOPED_WRITE_LOCK(m_fileMappingTableMemLock, lock);
			BinaryWriter writer(m_fileMappingTableMem, m_fileMappingTableSize);
			writer.WriteStringKey(fileNameKey);
			writer.WriteString(out.name);
			writer.Write7BitEncoded(size);
			m_fileMappingTableSize = (u32)writer.GetPosition();
		}

		entry.isDir = false;
		entry.size = size;
		
		out.size = size;
		return true;
	}

	bool GetDirKey(StringKey& outDirKey, StringBufferBase& outDirName, const tchar*& outLastSlash, const tchar* fileName)
	{
		outLastSlash = TStrrchr(fileName, PathSeparator);
		UBA_ASSERTF(outLastSlash, TC("Can't get dir key for path %s"), fileName);
		if (!outLastSlash)
			return false;

		u64 dirLen = u64(outLastSlash - fileName);
		outDirName.Append(fileName, dirLen);
		outDirKey = CaseInsensitiveFs ? ToStringKeyLower(outDirName) : ToStringKey(outDirName);
		return true;
	}


	bool Session::RegisterCreateFileForWrite(StringKey fileNameKey, const tchar* fileName, u64 fileNameLen, bool registerRealFile, u64 fileSize, u64 lastWriteTime)
	{
		// Remote is not updating its own directory table
		if (m_runningRemote)
			return true;

		auto& dirTable = m_directoryTable;

		StringKey dirKey;
		const tchar* lastSlash;
		StringBuffer<> dirName;
		if (!GetDirKey(dirKey, dirName, lastSlash, fileName))
			return true;
			
		#if 0//_DEBUG  // Bring this back, turned off right now because a few lines above the call to this method we add a mapping
		{
			SCOPED_WRITE_LOCK(m_fileMappingTableLookupLock, lookupLock);
			auto findIt = m_fileMappingTableLookup.find(fileNameKey);
			if (findIt != m_fileMappingTableLookup.end())
			{
				FileMappingEntry& entry = findIt->second;
				lookupLock.Leave();
				SCOPED_WRITE_LOCK(entry.lock, entryCs);
				UBA_ASSERT(!entry.mapping);
			}
		}
		#endif

		bool shouldWriteToDisk = registerRealFile && ShouldWriteToDisk(fileName, fileNameLen);

		// When not writing to disk we need to populate lookup before adding non-written files.. otherwise they will be lost once lookup is actually populated
		if (!shouldWriteToDisk)
		{
			u32 offset;
			u32 res = WriteDirectoryEntries(dirKey, dirName.data, offset);
			UBA_ASSERT(res); (void)res;
		}

		SCOPED_READ_LOCK(dirTable.m_lookupLock, lookupCs);
		auto findIt = dirTable.m_lookup.find(dirKey);
		if (findIt == dirTable.m_lookup.end())
			return true;

		DirectoryTable::Directory& dir = findIt->second;
		lookupCs.Leave();

		SCOPED_WRITE_LOCK(dir.lock, dirLock);

		// To prevent race where code creating dir manage to add to lookup but then got here later than this thread.
		while (dir.parseOffset == 0)
		{
			dirLock.Leave();
			Sleep(1);
			dirLock.Enter();
		}

		// Directory was attempted to be added when it didn't exist. It is still added to dirtable lookup but we set parseOffset to 2.
		// If adding a file, clearly it does exist.. so let's reparse it.
		if (dir.parseOffset == 2)
		{
			u32 offset;
			dirLock.Leave();
			u32 res = WriteDirectoryEntries(dirKey, dirName.data, offset);
			UBA_ASSERT(res); (void)res;
			dirLock.Enter();
		}
		UBA_ASSERT(dir.parseOffset == 1);

		if (fileNameKey == StringKeyZero)
		{
			StringBuffer<> forKey;
			forKey.Append(fileName, fileNameLen);
			if (CaseInsensitiveFs)
				forKey.MakeLower();
			fileNameKey = ToStringKey(forKey);
		}
		auto insres = dir.files.try_emplace(fileNameKey, ~u32(0));

		u64 fileIndex = InvalidValue;
		u32 attributes = 0;
		u32 volumeSerial = 0;

		if (shouldWriteToDisk)
		{
			FileInformation info;
			if (!GetFileInformation(info, m_logger, fileName))
				return m_logger.Error(TC("Failed to get file information for %s while checking file added for write. This should not happen! (%s)"), fileName, LastErrorToText().data);

			attributes = info.attributes;
			volumeSerial = info.volumeSerialNumber;
			lastWriteTime = info.lastWriteTime;
			if (IsDirectory(attributes))
				fileSize = 0;
			else
				fileSize = info.size;
			fileIndex = info.index;
		}
		else
		{
			// TODO: Do we need more code here?
			attributes = DefaultAttributes();
			volumeSerial = 1;
			fileIndex = ++m_fileIndexCounter;
		}

		// Check if new write is actually a write. The file might just have been open with write permissions and then actually never written to.
		// We check this by using lastWriteTime. If it hasn't change, directory table is already up-to-date
		if (!insres.second && insres.first->second != ~u32(0))
		{
			BinaryReader reader(dirTable.m_memory + insres.first->second);
			u64 oldLastWriteTime = reader.ReadU64();

			if (lastWriteTime == oldLastWriteTime)
			{
				#if !PLATFORM_WINDOWS
				reader.Skip(sizeof(u32) * 2);
				u64 oldFileIndex = reader.ReadU64();
				UBA_ASSERT(oldFileIndex == fileIndex); // Checking so it is really the same file
				#endif
				return true;
			}
		}

		FileEntryAdded(fileNameKey, lastWriteTime, fileSize);

		u8 temp[1024];
		u64 written;
		u64 entryPos;
		{
			BinaryWriter writer(temp, 0, sizeof(temp));
			writer.WriteStringKey(dirKey);
			writer.Write7BitEncoded(dir.tableOffset); // Previous entry for same directory
			writer.Write7BitEncoded(1); // Count
			writer.WriteString(lastSlash + 1);
			entryPos = writer.GetPosition();
			writer.WriteU64(lastWriteTime);
			writer.WriteU32(attributes);
			writer.WriteU32(volumeSerial);
			writer.WriteU64(fileIndex);
			writer.WriteU64(fileSize);
			written = writer.GetPosition();
		}

		#if UBA_DEBUG_LOGGER
		g_debugLogger.Info(TC("TRACKADD    %s (Size: %llu, Key: %s, Id: %llu)\n"), fileName, fileSize, KeyToString(fileNameKey).data, fileIndex);
		#endif


		SCOPED_WRITE_LOCK(dirTable.m_memoryLock, memoryLock);
		u8* startPos = dirTable.m_memory + dirTable.m_memorySize;
		BinaryWriter writer(startPos);
		writer.Write7BitEncoded(written); // Storage size
		insres.first->second = dirTable.m_memorySize + u32(writer.GetPosition() + entryPos); // Storing position to last write time
		u32 tableOffset = u32(writer.GetPosition()) + sizeof(StringKey);
		writer.WriteBytes(temp, written);
		dir.tableOffset = dirTable.m_memorySize + tableOffset;
		dirTable.m_memorySize += u32(writer.GetPosition());
		UBA_ASSERT(dirTable.m_memorySize < DirTableMemSize);
		return true;
	}

	u32 Session::RegisterDeleteFile(StringKey fileNameKey, const tchar* fileName)
	{
		// Remote is not updating its own directory table
		if (m_runningRemote)
			return GetDirectoryTableSize();

		auto& dirTable = m_directoryTable;

		StringKey dirKey;
		const tchar* lastSlash;
		StringBuffer<> dirName;
		if (!GetDirKey(dirKey, dirName, lastSlash, fileName))
			return InvalidTableOffset;

		SCOPED_READ_LOCK(dirTable.m_lookupLock, lookupCs);
		auto res = dirTable.m_lookup.find(dirKey);
		if (res == dirTable.m_lookup.end())
			return 0;
		DirectoryTable::Directory& dir = res->second;
		lookupCs.Leave();
		SCOPED_WRITE_LOCK(dir.lock, dirLock);

		while (dir.parseOffset == 0)
		{
			dirLock.Leave();
			Sleep(1);
			dirLock.Enter();
		}
		UBA_ASSERT(dir.parseOffset == 1);

		if (fileNameKey == StringKeyZero)
		{
			StringBuffer<> forKey;
			forKey.Append(fileName);
			if (CaseInsensitiveFs)
				forKey.MakeLower();
			fileNameKey = ToStringKey(forKey);
		}
		
		// Does not exist, no need adding to file table
		if (dir.files.erase(fileNameKey) == 0)
			return 0;

		u8 temp[1024];
		u64 written;
		{
			BinaryWriter writer(temp, 0, sizeof(temp));
			writer.WriteStringKey(dirKey);
			writer.Write7BitEncoded(dir.tableOffset); // Previous entry for same directory
			writer.Write7BitEncoded(1); // Count
			writer.WriteString(lastSlash + 1);
			writer.WriteU32(0);
			writer.WriteU32(0);
			writer.WriteU64(0);
			writer.WriteU64(0);
			writer.WriteU64(0);
			written = writer.GetPosition();
		}

		#if UBA_DEBUG_LOGGER
		g_debugLogger.Info(TC("TRACKDEL    %s (Key: %s)\n"), fileName, KeyToString(fileNameKey).data);
		#endif

		SCOPED_WRITE_LOCK(dirTable.m_memoryLock, memoryLock);
		u8* startPos = dirTable.m_memory + dirTable.m_memorySize;
		BinaryWriter writer(startPos);
		writer.Write7BitEncoded(written); // Storage size
		u32 tableOffset = u32(writer.GetPosition()) + sizeof(StringKey);
		writer.WriteBytes(temp, written);
		dir.tableOffset = dirTable.m_memorySize + tableOffset;
		dirTable.m_memorySize += u32(writer.GetPosition());
		UBA_ASSERT(dirTable.m_memorySize < DirTableMemSize);
		return dirTable.m_memorySize;
	}

	bool Session::CopyImports(Vector<BinaryModule>& out, const tchar* library, tchar* applicationDir, tchar* applicationDirEnd, UnorderedSet<TString>& handledImports)
	{
		#if PLATFORM_WINDOWS
		if (!handledImports.insert(library).second)
			return true;
		swprintf_s(applicationDirEnd, 512 - (applicationDirEnd - applicationDir), TC("%s"), library);
		const tchar* applicationName = applicationDir;
		u32 attr = GetFileAttributesW(applicationName); // TODO: Use attributes table
		tchar temp[512];
		tchar temp2[512];
		if (attr == INVALID_FILE_ATTRIBUTES)
		{
			if (!SearchPathW(NULL, library, NULL, 512, temp, NULL))
				return true; // TODO: We have to return true here because there are scenarios where failing is actually ok (it seems it can return false on crt shim libraries such as api-ms-win-crt*)

			applicationName = temp;
			attr = DefaultAttributes();

			tchar* lastSlash = TStrrchr(temp, '\\');
			UBA_ASSERT(lastSlash);
			u64 applicationDirLen = u64(lastSlash + 1 - temp);
			memcpy(temp2, temp, applicationDirLen * sizeof(tchar));
			applicationDir = temp2;
			applicationDirEnd = temp2 + applicationDirLen;
		}

		StringBuffer<512> temp3;
		FixPath(applicationName, nullptr, 0, temp3);

		bool isSystem = StartsWith(applicationName, m_systemPath.data);
		if (isSystem && IsKnownSystemFile(applicationName))
			return true;


		out.push_back({ library, temp3.data, attr, isSystem });

		bool result = true;
		FindImports(applicationName, [&](const tchar* importName, bool isKnown)
			{
				if (result && !isKnown)
					result = CopyImports(out, importName, applicationDir, applicationDirEnd, handledImports);
			});
		return result;
		#else
		return true;
		#endif
	}

	Session::Session(const SessionCreateInfo& info, const tchar* logPrefix, bool runningRemote, WorkManager* workManager)
	:	m_storage(info.storage)
	,	m_logger(info.logWriter, logPrefix)
	,	m_workManager(workManager)
	,	m_directoryTable(&m_directoryTableMemory)
	,	m_fileMappingBuffer(m_logger, workManager)
	,	m_processCommunicationAllocator(m_logger, TC("CommunicationAllocator"))
	,	m_trace(info.logWriter)
	{
		if (info.useUniqueId)
		{
			time_t rawtime;
			time(&rawtime);
			tm ti;
			localtime_s(&ti, &rawtime);
			m_id.Appendf(TC("%02i%02i%02i_%02i%02i%02i"), ti.tm_year - 100,ti.tm_mon+1,ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
		}
		else
		{
			m_id.Append(TC("Debug"));
		}

		UBA_ASSERTF(info.rootDir && *info.rootDir, TC("No root dir set when creating session"));
		m_rootDir.count = GetFullPathNameW(info.rootDir, m_rootDir.capacity, m_rootDir.data, NULL);
		m_rootDir.Replace('/', PathSeparator).EnsureEndsWithSlash();
		m_uid = u32(HashString()(m_rootDir.data));

		m_runningRemote = runningRemote;
		m_disableCustomAllocator = info.disableCustomAllocator;
		m_allowMemoryMaps = info.allowMemoryMaps;
		if (!info.allowMemoryMaps)
			m_keepOutputFileMemoryMapsThreshold = 0;
		else
			m_keepOutputFileMemoryMapsThreshold = info.keepOutputFileMemoryMapsThreshold;
		m_shouldWriteToDisk = info.shouldWriteToDisk;
		UBA_ASSERTF(m_shouldWriteToDisk || m_allowMemoryMaps, TC("Can't disable both should write to disk and allow memory maps"));

		m_detailedTrace = info.detailedTrace;
		m_logToFile = info.logToFile;
		if (info.extraInfo)
			m_extraInfo = info.extraInfo;

		if (info.deleteSessionsOlderThanSeconds)
		{
			StringBuffer<> sessionsDir;
			sessionsDir.Append(m_rootDir).Append(TC("sessions"));

			u64 systemTimeAsFileTime = GetSystemTimeAsFileTime();

			TraverseDir(m_logger, sessionsDir.data,
				[&](const DirectoryEntry& e)
				{
					u64 seconds = GetFileTimeAsSeconds(systemTimeAsFileTime - e.lastWritten);
					if (seconds <= info.deleteSessionsOlderThanSeconds)
						return;

					if (IsDirectory(e.attributes)) // on macos we get a ".ds_store" file created by the os
					{
						StringBuffer<> sessionDir(sessionsDir);
						sessionDir.EnsureEndsWithSlash().Append(e.name);
						DeleteAllFiles(m_logger, sessionDir.data);
					}
				});
		}

		m_sessionDir.Append(m_rootDir).Append(TC("sessions")).Append(PathSeparator).Append(m_id).Append(PathSeparator);
		m_sessionBinDir.Append(m_sessionDir).Append(TC("bin"));
		m_sessionOutputDir.Append(m_sessionDir).Append(TC("output"));
		m_sessionLogDir.Append(m_sessionDir).Append(TC("log"));

		if (m_runningRemote)
		{
			m_storage.CreateDirectory(m_sessionBinDir.data);
			m_storage.CreateDirectory(m_sessionOutputDir.data);
		}

		m_tempPath.Append(m_sessionDir).Append(TC("temp"));
		m_storage.CreateDirectory(m_tempPath.data);
		m_tempPath.EnsureEndsWithSlash();

		m_sessionBinDir.EnsureEndsWithSlash();
		m_sessionOutputDir.EnsureEndsWithSlash();

		m_storage.CreateDirectory(m_sessionLogDir.data);
		m_sessionLogDir.EnsureEndsWithSlash();

		if (info.traceOutputFile)
			m_traceOutputFile.Append(info.traceOutputFile);
	}

	bool Session::Create(const SessionCreateInfo& info)
	{
		#if UBA_DEBUG_LOGGER
		StartDebugLogger(m_logger, StringBuffer<512>().Append(m_sessionDir).Append(TC("SessionDebug.log")).data);
		#endif

		#if PLATFORM_WINDOWS
		m_systemPath.count = GetEnvironmentVariableW(TC("SystemRoot"), m_systemPath.data, m_systemPath.capacity);
		#else
		m_systemPath.Append(TC("/nonexistingpath"));
		#endif

		m_fileMappingTableHandle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE, FileMappingTableMemSize);
		UBA_ASSERT(m_fileMappingTableHandle.IsValid());
		m_fileMappingTableMem = MapViewOfFile(m_fileMappingTableHandle, FILE_MAP_WRITE, 0, FileMappingTableMemSize);
		UBA_ASSERT(m_fileMappingTableMem);

		m_directoryTableHandle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE, DirTableMemSize);
		UBA_ASSERT(m_directoryTableHandle.IsValid());
		m_directoryTableMem = MapViewOfFile(m_directoryTableHandle, FILE_MAP_WRITE, 0, DirTableMemSize);
		UBA_ASSERT(m_directoryTableMem);

		m_directoryTable.m_memory = m_directoryTableMem;
		m_directoryTable.m_lookup.reserve(30000);
		m_fileMappingTableLookup.reserve(70000);

		m_fileMappingBuffer.AddTransient(TC("FileMappings"));

		u64 reserveSize = CommunicationMemSize * 512;
		if (!m_processCommunicationAllocator.Init(CommunicationMemSize, reserveSize))
			return false;

		if (!CreateProcessJobObject())
			return false;

		// Environment variables that should stay local when building remote (not replicated)
		#if PLATFORM_WINDOWS
		m_localEnvironmentVariables.insert(TC("SystemRoot"));
		m_localEnvironmentVariables.insert(TC("SystemDrive"));
		m_localEnvironmentVariables.insert(TC("NUMBER_OF_PROCESSORS"));
		m_localEnvironmentVariables.insert(TC("PROCESSOR_ARCHITECTURE"));
		m_localEnvironmentVariables.insert(TC("PROCESSOR_IDENTIFIER"));
		m_localEnvironmentVariables.insert(TC("PROCESSOR_LEVEL"));
		m_localEnvironmentVariables.insert(TC("PROCESSOR_REVISION"));
		#endif

		StringBuffer<> traceName;
		if (info.traceName && *info.traceName)
			traceName.Append(info.traceName);
		else if (info.launchVisualizer || !m_traceOutputFile.IsEmpty() || info.traceEnabled)
			traceName.Append(m_id);

		if (!traceName.IsEmpty())
			StartTrace(IsWindows ? traceName.data : nullptr); // non-windows named shared memory not implemented (only needed for UbaVisualizer which you can't run on linux either way)

		#if PLATFORM_WINDOWS
		if (info.launchVisualizer)
		{
			HMODULE currentModule = GetModuleHandle(NULL);
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&g_dummy, &currentModule);
			tchar fileName[512];
			GetModuleFileNameW(currentModule, fileName, 512);
			uba::StringBuffer<> launcherCmd;
			launcherCmd.Append(TC("\""));
			launcherCmd.AppendDir(fileName);
			launcherCmd.Append(TC("\\UbaVisualizer.exe\""));
			launcherCmd.Append(TC(" -named=")).Append(traceName);
			STARTUPINFOW si;
			memset(&si, 0, sizeof(si));
			PROCESS_INFORMATION pi;
			m_logger.Info(TC("Starting visualizer: %s"), launcherCmd.data);
			DWORD creationFlags = DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;
			CreateProcessW(NULL, launcherCmd.data, NULL, NULL, false, creationFlags, NULL, NULL, &si, &pi);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
		#endif

		return true;
	}

	Session::~Session()
	{
		StopTrace(m_traceOutputFile.data);

		CancelAllProcessesAndWait();
		FlushDeadProcesses();

		//for (auto& i : m_fileMappingTableLookup)
		//	CloseHandle(i.second.mapping);

		UnmapViewOfFile(m_fileMappingTableMem, FileMappingTableMemSize, TC("FileMappingTable"));
		CloseFileMapping(m_fileMappingTableHandle);

		UnmapViewOfFile(m_directoryTableMem, DirTableMemSize, TC("DirectoryTable"));
		CloseFileMapping(m_directoryTableHandle);

		//#if !UBA_DEBUG
		//u32 count;
		//DeleteAllFiles(m_logger, m_sessionDir.data, count);
		//#endif

		#if UBA_DEBUG_LOGGER
		StopDebugLogger();
		#endif
	}

	void Session::CancelAllProcessesAndWait(bool terminate)
	{
		m_logger.isMuted = true;

		bool isEmpty = false;
		bool isFirst = true;
		while (!isEmpty)
		{
			Vector<ProcessHandle> processes;
			{
				SCOPED_WRITE_LOCK(m_processesLock, lock);
				isEmpty = m_processes.empty();
				processes.reserve(m_processes.size());
				for (auto& pair : m_processes)
					processes.push_back(pair.second);
			}

			if (isFirst)
			{
				isFirst = false;
				if (!processes.empty())
					m_logger.Info(TC("Cancelling %llu processes and wait for them to exit"), processes.size());
			}

			for (auto& process : processes)
				process.Cancel(true);

			#if PLATFORM_WINDOWS
			if (m_processJobObject != NULL)
			{
				SCOPED_WRITE_LOCK(m_processJobObjectLock, lock);
				CloseHandle(m_processJobObject);
				m_processJobObject = NULL;
			}
			#endif

			for (auto& process : processes)
				process.WaitForExit(100000);
		}

		m_logger.isMuted = false;
	}

	ProcessHandle Session::RunProcess(const ProcessStartInfo& startInfo, bool async, bool enableDetour)
	{
		FlushDeadProcesses();
		ValidateStartInfo(startInfo);
		return InternalRunProcess(startInfo, async, nullptr, enableDetour);
	}

	void Session::ValidateStartInfo(const ProcessStartInfo& startInfo)
	{
		UBA_ASSERTF(startInfo.workingDir && *startInfo.workingDir, TC("Working dir must be set when spawning process"));
		UBA_ASSERTF(!TStrchr(startInfo.application, '~'), TC("Application path must use long name (%s)"), startInfo.application);
		UBA_ASSERTF(!TStrchr(startInfo.workingDir, '~'), TC("WorkingDir path must use long name (%s)"), startInfo.workingDir);
	}

	ProcessHandle Session::InternalRunProcess(const ProcessStartInfo& startInfo, bool async, ProcessImpl* parent, bool enableDetour)
	{
		StringBuffer<> realApplication(startInfo.application);
		const tchar* realWorkingDir = startInfo.workingDir;

		if (!PrepareProcess(startInfo, parent != nullptr, realApplication, realWorkingDir))
			return {};

		auto& si = const_cast<ProcessStartInfo&>(startInfo);
		si.useCustomAllocator &= !m_disableCustomAllocator;
		const tchar* originalLogFile = si.logFile;
		
		StringBuffer<> logFile;
		if (si.logFile && *si.logFile)
		{
			if (TStrchr(si.logFile, PathSeparator) == nullptr)
			{
				logFile.Append(m_sessionLogDir).Append(si.logFile);
				si.logFile = logFile.data;
			}
		}
		else if (m_logToFile)
		{
			logFile.Append(m_sessionLogDir);
			GetNameFromArguments(logFile, startInfo.arguments, true);
			logFile.Append(TC(".log"));
			si.logFile = logFile.data;
		}

		void* env = GetProcessEnvironmentVariables();
		u32 id = ++m_processIdCounter;
		auto process = new ProcessImpl(*this, id, parent);
		ProcessHandle h(process);
		process->Start(startInfo, realApplication.data, realWorkingDir, m_runningRemote, env, async, enableDetour);

		si.logFile = originalLogFile;
		return h;
	}

	void Session::RefreshDirectory(const tchar* dirName)
	{
		UBA_ASSERT(!m_runningRemote);

		StringBuffer<> dirPath;
		FixPath2(dirName, nullptr, 0, dirPath.data, dirPath.capacity, &dirPath.count);
		StringKey dirKey = CaseInsensitiveFs ? ToStringKeyLower(dirPath) : ToStringKey(dirPath);

		auto& dirTable = m_directoryTable;
		SCOPED_READ_LOCK(dirTable.m_lookupLock, lookupLock);
		auto res = dirTable.m_lookup.find(dirKey);
		if (res == dirTable.m_lookup.end())
			return;
		DirectoryTable::Directory& dir = res->second;
		lookupLock.Leave();
		SCOPED_WRITE_LOCK(dir.lock, dirLock);

		while (dir.parseOffset == 0)
		{
			dirLock.Leave();
			Sleep(1);
			dirLock.Enter();
		}
		UBA_ASSERT(dir.parseOffset == 1);

		StringKeyHasher hasher;
		hasher.Update(dirPath.data, TStrlen(dirPath.data));
		m_directoryTable.PopulateDirectory(hasher, dir);

		u32 tableOffset;
		WriteDirectoryEntriesInternal(dir, dirKey, dirPath.data, true, tableOffset);
	}

	StringKey GetKeyAndFixedName(StringBuffer<>& fixedFilePath, const tchar* filePath)
	{
		FixPath2(filePath, nullptr, 0, fixedFilePath.data, fixedFilePath.capacity, &fixedFilePath.count);

		StringKey dirKey;
		StringBuffer<> dirNameForHash;
		const tchar* baseFileName;
		GetDirKey(dirKey, dirNameForHash, baseFileName, fixedFilePath.data);

		if (CaseInsensitiveFs)
			dirNameForHash.MakeLower();

		StringKeyHasher hasher;
		hasher.Update(dirNameForHash.data, dirNameForHash.count);

		StringBuffer<128> baseFileNameForHash;
		baseFileNameForHash.Append(baseFileName);
		if (CaseInsensitiveFs)
			baseFileNameForHash.MakeLower();
		return ToStringKey(hasher, baseFileNameForHash.data, baseFileNameForHash.count);
	}

	void Session::RegisterNewFile(const tchar* filePath)
	{
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedFilePath;
		auto key = GetKeyAndFixedName(fixedFilePath, filePath);
		RegisterCreateFileForWrite(key, fixedFilePath.data, fixedFilePath.count, true);
	}

	void Session::RegisterDeleteFile(const tchar* filePath)
	{
		UBA_ASSERT(!m_runningRemote);
		StringBuffer<> fixedFilePath;
		auto key = GetKeyAndFixedName(fixedFilePath, filePath);
		RegisterDeleteFile(key, fixedFilePath.data);
	}

	void Session::RegisterCustomService(CustomServiceFunction&& function)
	{
		m_customServiceFunction = function;
	}

	void Session::RegisterGetNextProcess(GetNextProcessFunction&& function)
	{
		m_getNextProcessFunction = function;
	}

	const tchar* Session::GetId() { return m_id.data; }
	Storage& Session::GetStorage() { return m_storage; }
	Logger& Session::GetLogger() { return m_logger; }
	LogWriter& Session::GetLogWriter() { return m_logger.m_writer; }

	void Session::ProcessAdded(Process& process, u32 sessionId)
	{
		u32 processId = process.GetId();

		if (!process.IsChild())
			m_trace.ProcessAdded(sessionId, processId, process.GetStartInfo().description);

		SCOPED_WRITE_LOCK(m_processesLock, lock);
		m_processes.try_emplace(processId, ProcessHandle(&process));
	}

	void Session::ProcessExited(ProcessImpl& process, u64 executionTime)
	{
		const tchar* application = process.GetStartInfo().application;
		StringBuffer<> applicationName;
		applicationName.AppendFileName(application);
		if (applicationName.count > 21)
			applicationName[21] = 0;

		u32 id = process.GetId();

		if (!process.IsChild())
		{
			StackBinaryWriter<1024> writer;
			process.m_processStats.Write(writer);
			u32 exitCode = process.GetExitCode();
			Vector<ProcessLogLine> emptyLines;
			auto& logLines = (exitCode != 0 || m_detailedTrace) ? process.m_logLines : emptyLines;
			m_trace.ProcessExited(id, exitCode, writer.GetData(), writer.GetPosition(), logLines);
			SCOPED_WRITE_LOCK(m_processStatsLock, lock);
			m_processStats.Add(process.m_processStats);
			m_stats.Add(process.m_sessionStats);
		}

		SCOPED_WRITE_LOCK(m_processesLock, lock);
		m_deadProcesses.emplace_back(&process); // Here to prevent Process thread call trigger a delete of Process which causes a deadlock
		auto& stats = m_applicationStats[applicationName.data];
		stats.count++;
		stats.time += executionTime;
		m_processes.erase(id);
	}

	void Session::FlushDeadProcesses()
	{
		SCOPED_WRITE_LOCK(m_processesLock, lock);
		Vector<ProcessHandle> deadProcesses;
		deadProcesses.swap(m_deadProcesses);
		lock.Leave();
	}

	bool Session::GetInitResponse(InitResponse& out, const InitMessage& msg)
	{
		out.directoryTableHandle = m_directoryTableHandle.ToU64();
		{
			SCOPED_READ_LOCK(m_directoryTable.m_memoryLock, l);
			out.directoryTableSize = (u32)m_directoryTable.m_memorySize;
		}
		{
			SCOPED_READ_LOCK(m_directoryTable.m_lookupLock, l);
			out.directoryTableCount = (u32)m_directoryTable.m_lookup.size();
		}
		out.mappedFileTableHandle = m_fileMappingTableHandle.ToU64();
		{
			SCOPED_READ_LOCK(m_fileMappingTableMemLock, l);
			out.mappedFileTableSize = m_fileMappingTableSize;
		}
		{
			SCOPED_READ_LOCK(m_fileMappingTableLookupLock, l);
			out.mappedFileTableCount = (u32)m_fileMappingTableLookup.size();
		}
		return true;
	}

	u32 Session::GetDirectoryTableSize()
	{
		SCOPED_READ_LOCK(m_directoryTable.m_memoryLock, lock);
		return m_directoryTable.m_memorySize;
	}

	u32 Session::GetFileMappingSize()
	{
		SCOPED_READ_LOCK(m_fileMappingTableMemLock, lock);
		return m_fileMappingTableSize;
	}

	SessionStats& Session::Stats()
	{
		if (SessionStats* s = SessionStats::GetCurrent())
			return *s;
		return m_stats;
	}

	u32 Session::GetActiveProcessCount()
	{
		SCOPED_READ_LOCK(m_processesLock, cs);
		return u32(m_processes.size());
	}

	void Session::PrintProcessStats(ProcessStats& stats, const tchar* logName)
	{
		m_logger.Info(TC("  -- %s --"), logName);
		stats.Print(m_logger);
	}

	void Session::StartTrace(const tchar* traceName)
	{
		if (traceName)
			LoggerWithWriter(m_logger.m_writer).Info(TC("---- Starting trace: %s ----"), traceName);
		else
			LoggerWithWriter(m_logger.m_writer).Info(TC("---- Starting trace ----"));

		m_trace.StartWrite(traceName, m_detailedTrace ? 512*1024*1024 : 128*1024*1024);

		tchar buf[256];
		if (!GetComputerNameW(buf, sizeof_array(buf)))
			TStrcpy_s(buf, 256, TC("LOCAL"));
		StringBuffer<> systemInfo;
		GetSystemInfo(systemInfo);
		m_trace.SessionAdded(0, {}, buf, systemInfo.data);
		m_traceThreadEvent.Create(true);
		m_traceThread.Start([this]() { ThreadTraceLoop(); return 0; });
	}

	bool Session::StopTrace(const tchar* writeFile)
	{
		StopTraceThread();
		return m_trace.StopWrite(writeFile);
	}

	void Session::StopTraceThread()
	{
		m_traceThreadEvent.Set();
		m_traceThread.Wait();
	}

	void Session::PrintSummary(Logger& logger)
	{
		logger.BeginScope();
		logger.Info(TC("  ----- Uba process stats summary -----"));
		m_processStats.Print(logger);
		logger.Info(TC(""));

		MultiMap<u64, std::pair<const TString*, u32>> sortedApps;
		for (auto& pair : m_applicationStats)
			sortedApps.insert({pair.second.time, {&pair.first, pair.second.count}});
		for (auto i=sortedApps.rbegin(), e=sortedApps.rend(); i!=e; ++i)
		{
			const TString& name = *i->second.first;
			u64 time = i->first;
			u32 count = i->second.second;
			logger.Info(TC("  %-21s %5u %9s"), name.c_str(), count, TimeToText(time).str);
		}
		logger.Info(TC(""));

		logger.Info(TC("  ----- Uba session stats summary -----"));

		PrintSessionStats(logger);
		logger.EndScope();
	}

	bool Session::GetBinaryModules(Vector<BinaryModule>& out, const tchar* application)
	{
		const tchar* applicationName = application;

		if (tchar* lastSlash = TStrrchr((tchar*)application, PathSeparator))
			applicationName = lastSlash + 1;

		u64 applicationDirLen = u64(applicationName - application);
		tchar applicationDir[512];
		UBA_ASSERT(applicationDirLen < 512);
		memcpy(applicationDir, application, applicationDirLen * sizeof(tchar));
		tchar* applicationDirEnd = applicationDir + applicationDirLen;

#if PLATFORM_WINDOWS

		UnorderedSet<TString> handledImports;
		CopyImports(out, applicationName, applicationDir, applicationDirEnd, handledImports);
#else
		// TODO: This. Does non-windows have dlls that needs to be downloaded here?
		out.push_back({ TString(applicationName), TString(application), S_IRUSR | S_IWUSR | S_IXUSR });
		
		// This code is needed if application is compiled with tsan
		//strcpy(applicationDirEnd, "libclang_rt.tsan.so");
		//out.push_back({ "libclang_rt.tsan.so", applicationDir, S_IRUSR | S_IWUSR });
#endif
		return true;
	}

	void Session::Free(Vector<BinaryModule>& v)
	{
		v.resize(0);
		v.shrink_to_fit();
	}

	bool Session::IsRarelyRead(ProcessImpl& process, const StringBufferBase& fileName) const
	{
		return GetApplicationRules()[process.m_rulesIndex].rules->IsRarelyRead(fileName);
	}

	bool Session::IsRarelyReadAfterWritten(ProcessImpl& process, const tchar* fileName, u64 fileNameLen) const
	{
		return GetApplicationRules()[process.m_rulesIndex].rules->IsRarelyReadAfterWritten(fileName, fileNameLen);
	}

	bool Session::IsKnownSystemFile(const tchar* applicationName)
	{
#if PLATFORM_WINDOWS
		return uba::IsKnownSystemFile(applicationName);
#else
		return false;
#endif
	}

	bool Session::ShouldWriteToDisk(const tchar* fileName, u64 fileNameLen)
	{
		if (m_shouldWriteToDisk)
			return true;
		return EndsWith(fileName, fileNameLen, TC(".h"));
	}

	bool Session::PrepareProcess(const ProcessStartInfo& startInfo, bool isChild, StringBufferBase& outRealApplication, const tchar*& outRealWorkingDir)
	{
		return true;
	}

	u32 Session::GetMemoryMapAlignment(const tchar* fileName, u64 fileNameLen) const
	{
		// It is not necessarily better to make mem maps of everything.. only things that are read more than once in the build.
		// Reason is because there is additional overhead to use memory mappings.
		// Upside is that all things that are memory mapped can be stored compressed in cas storage so it saves space.

		if (EndsWith(fileName, fileNameLen, TC(".pch")))
			return 64 * 1024; // pch needs 64k alignment
		if (EndsWith(fileName, fileNameLen, TC(".h")) || EndsWith(fileName, fileNameLen, TC(".inl")) || EndsWith(fileName, fileNameLen, TC(".gch")))
			return 4 * 1024; // clang seems to need 4k alignment? Is it a coincidence it works or what is happening inside the code? (msvc works with alignment 1byte here)
		if (EndsWith(fileName, fileNameLen, TC(".h.obj")))
			return 4 * 1024;
		//if (EndsWith(fileName, fileNameLen, TC(".lib")))
		//	return 4 * 1024;
		//if (EndsWith(fileName, fileNameLen, TC(".rc2.res")))
		//	return 64;
		//if (EndsWith(fileName, fileNameLen, TC(".rsp")))
		//	return 4 * 1024; // rsp is read over and over again
		//if (EndsWith(fileName, fileNameLen, TC(".h.obj")) || EndsWith(fileName, fileNameLen, TC(".lib")))
		//	return 4 * 1024; // rsp is read over and over again
		return 0;
	}

	void* Session::GetProcessEnvironmentVariables()
	{
		SCOPED_WRITE_LOCK(m_environmentVariablesLock, lock);
		if (!m_environmentVariables.empty())
			return m_environmentVariables.data();

#if PLATFORM_WINDOWS
		auto strs = GetEnvironmentStringsW();
		for (auto it = strs; *it; it += TStrlen(it) + 1)
		{
			StringBuffer<> varName;
			varName.Append(it, TStrchr(it, '=') - it);
			const tchar* varValue = it + varName.count + 1;

			if (m_runningRemote && varName.Equals(TC("PATH")))
			{
				AddEnvironmentVariableNoLock(TC("PATH"), TC("c:\\noenvironment"));
				continue;
			}
			if (varName.Equals(TC("TEMP")) || varName.Equals(TC("TMP")))
			{
				AddEnvironmentVariableNoLock(varName.data, m_tempPath.data);
				continue;
			}
			if (varName.Equals(TC("_CL_")) || varName.Equals(TC("CL")))
			{
				continue;
			}

			AddEnvironmentVariableNoLock(varName.data, varValue);
		}

		FreeEnvironmentStrings(strs);

		AddEnvironmentVariableNoLock(TC("MSBUILDDISABLENODEREUSE"), TC("1")); // msbuild will reuse existing helper nodes but since those are not detoured we can't let that happen
		AddEnvironmentVariableNoLock(TC("DOTNET_CLI_USE_MSBUILD_SERVER"), TC("0")); // Disable msbuild server
		AddEnvironmentVariableNoLock(TC("DOTNET_CLI_TELEMETRY_OPTOUT"), TC("1")); // Stop talking to telemetry service
#else
		int i = 0;
		while (char* env = environ[i++])
		{
			if (StartsWith(env, "TMPDIR="))
				continue;

			if (!StartsWith(env, "PATH="))
			{
				m_environmentVariables.insert(m_environmentVariables.end(), env, env + TStrlen(env) + 1);
				continue;
			}

			TString paths;

			const char* start = env + 5;
			const char* it = start;
			while (*it)
			{
				if (*it != ':')
				{
					++it;
					continue;
				}

				const char* s = start;
				const char* e = it;
				start = ++it;

				if (StartsWith(s, "/mnt/"))
					continue;
				if (!paths.empty())
					paths += ":";
				paths.append(s, e);
			}
			AddEnvironmentVariableNoLock("PATH", paths.c_str());
		}
		AddEnvironmentVariableNoLock("TMPDIR", m_tempPath.data);
#endif
		m_environmentVariables.push_back(0);
		return m_environmentVariables.data();
	}

	bool Session::CreateFile(CreateFileResponse& out, const CreateFileMessage& msg)
	{
		const StringBufferBase& fileName = msg.fileName;
		const StringKey& fileNameKey = msg.fileNameKey;

		auto tableSizeGuard = MakeGuard([&]()
			{
				out.mappedFileTableSize = GetFileMappingSize();
				out.directoryTableSize = GetDirectoryTableSize();
			});

		if ((msg.access & ~FileAccess_Read) == 0)
		{
			if (!IsWindows)
			{
				out.fileName.Append(fileName);
				return true;
			}
		
			if (fileName.EndsWith(TC(".dll")) || fileName.EndsWith(TC(".exe")))
			{
				UBA_ASSERTF(fileName[1] == ':', TC("Got bad filename from process %s"), fileName.data);
				AddFileMapping(fileNameKey, fileName.data, TC("#"));
				out.fileName.Append(TC("#"));
				return true;
			}
			
			if (m_allowMemoryMaps)
			{
				if (u64 alignment = GetMemoryMapAlignment(fileName.data, fileName.count))
				{
					MemoryMap map;
					if (CreateMemoryMapFromFile(map, fileNameKey, fileName.data, false, alignment))
					{
						out.size = map.size;
						out.fileName.Append(map.name);
					}
					else
					{
						out.fileName.Append(fileName);
					}
					return true;
				}
			}

			if (!IsRarelyRead(msg.process, fileName))
			{
				AddFileMapping(fileNameKey, fileName.data, TC("#"));
				out.fileName.Append(TC("#"));
				return true;
			}

			out.fileName.Append(fileName);
			return true;
		}
		
		// if ((message.Access & FileAccess.Write) != 0)
		m_storage.ReportFileWrite(fileName.data);

		if (m_runningRemote && !fileName.StartsWith(m_tempPath.data))
		{
			SCOPED_WRITE_LOCK(m_outputFilesLock, lock);
			auto insres = m_outputFiles.try_emplace(fileName.data);
			if (insres.second)
			{
				out.fileName.Append(m_sessionOutputDir).Append(KeyToString(fileNameKey));
				insres.first->second = out.fileName.data;
			}
			else
			{
				out.fileName.Append(insres.first->second.c_str());
			}
		}
		else
		{
			out.fileName.Append(fileName);
		}

		SCOPED_WRITE_LOCK(m_activeFilesLock, lock);
		u32 wantsOnCloseId = m_wantsOnCloseIdCounter++;
		out.closeId = wantsOnCloseId;
		auto insres = m_activeFiles.try_emplace(wantsOnCloseId);
		if (!insres.second)
			return m_logger.Error(TC("TRYING TO ADD %s twice!"), out.fileName.data);

		insres.first->second.name = fileName.data;
		insres.first->second.nameKey = fileNameKey;
		return true;
	}

	void RemoveWrittenFile(ProcessImpl& process, const TString& name)
	{
		SCOPED_WRITE_LOCK(process.m_writtenFilesLock, writtenLock);
		auto& writtenFiles = process.m_writtenFiles;
		auto findIt = writtenFiles.find(name);
		if (findIt == writtenFiles.end())
			return;
		FileMappingHandle h = findIt->second.mappingHandle;
		writtenFiles.erase(findIt);
		writtenLock.Leave();

		if (h.IsValid())
			CloseFileMapping(h);
	}

	bool Session::CloseFile(CloseFileResponse& out, const CloseFileMessage& msg)
	{
		SCOPED_WRITE_LOCK(m_activeFilesLock, lock);
		auto findIt = m_activeFiles.find(msg.closeId);
		if (findIt == m_activeFiles.end())
			return m_logger.Error(TC("This should not happen. Got unknown closeId %u - %s"), msg.closeId, msg.fileName.data);

		ActiveFile file = findIt->second;
		m_activeFiles.erase(msg.closeId);
		lock.Leave();

		bool registerRealFile = true;
		u64 fileSize = 0;
		u64 lastWriteTime = 0;

		if (!msg.success)
		{
			return true;
		}
		if (msg.deleteOnClose)
		{
			RemoveWrittenFile(msg.process, file.name);
		}
		else
		{
			StringKey key = file.nameKey;
			const tchar* name = file.name.c_str();
			const tchar* msgName = msg.fileName.data;
			if (!msg.newName.IsEmpty())
			{
				UBA_ASSERT(!msg.deleteOnClose);
				RemoveWrittenFile(msg.process, file.name);
				name = msg.newName.data;
				key = msg.newNameKey;
				if (!m_runningRemote)
					msgName = msg.newName.data;
			}

			SCOPED_WRITE_LOCK(msg.process.m_writtenFilesLock, writtenLock);
			auto insres = msg.process.m_writtenFiles.try_emplace(name);
			WrittenFile& writtenFile = insres.first->second;

			UBA_ASSERT(writtenFile.owner == nullptr || writtenFile.owner == &msg.process);
			writtenFile.owner = &msg.process;
			writtenFile.attributes = msg.attributes;

			auto GetNowFileTime = []()
				{
					#if PLATFORM_WINDOWS
					FILETIME ft;
					SYSTEMTIME st;
					GetSystemTime(&st);
					SystemTimeToFileTime(&st, &ft);
					return (u64&)ft;
					#else
					return 0ull;
					#endif
				};

			bool addMapping = true;
			if (!insres.second)
			{
				if (writtenFile.name != msgName)
				{
					UBA_ASSERT(msg.mappingHandle == 0 && !writtenFile.mappingHandle.IsValid());
					writtenFile.name = msgName;
				}

				if (!msg.mappingHandle || msg.mappingHandle == writtenFile.originalMappingHandle)
				{
					if (msg.mappingWritten)
					{
						writtenFile.mappingWritten = msg.mappingWritten;
						writtenFile.lastWriteTime = GetNowFileTime();
					}
					addMapping = false;
				}
				else
				{
					if (writtenFile.mappingHandle.IsValid())
					{
						CloseFileMapping(writtenFile.mappingHandle);
						writtenFile.mappingHandle = {};
					}
				}
			}

			if (addMapping)
			{
				writtenFile.name = msgName;
				FileMappingHandle mappingHandle;
				if (msg.mappingHandle)
				{
					FileMappingHandle source;
					source.FromU64(msg.mappingHandle);
					if (!DuplicateFileMapping(msg.process.m_nativeProcessHandle, source, GetCurrentProcessHandle(), &mappingHandle, FILE_MAP_READ, false, 0))
						return m_logger.Error(TC("Failed to duplicate file mapping handle for %s"), name);
				}
				writtenFile.key = key;
				writtenFile.mappingHandle = mappingHandle;
				writtenFile.mappingWritten = msg.mappingWritten;
				writtenFile.originalMappingHandle = msg.mappingHandle;
				writtenFile.lastWriteTime = GetNowFileTime();
			}

			if (writtenFile.mappingHandle.IsValid())
			{
				registerRealFile = false;
				fileSize = writtenFile.mappingWritten;
				lastWriteTime = writtenFile.lastWriteTime;
			}
		}

		if (!msg.newName.IsEmpty())
		{
			RegisterDeleteFile(file.nameKey, file.name.c_str());
			RegisterCreateFileForWrite(msg.newNameKey, msg.newName.data, msg.newName.count, registerRealFile, fileSize, lastWriteTime);
		}
		else if (msg.deleteOnClose)
			RegisterDeleteFile(file.nameKey, file.name.c_str());
		else
			RegisterCreateFileForWrite(file.nameKey, file.name.c_str(), file.name.size(), registerRealFile, fileSize, lastWriteTime);

		out.directoryTableSize = GetDirectoryTableSize();
		return true;
	}

	bool Session::DeleteFile(DeleteFileResponse& out, const DeleteFileMessage& msg)
	{
		if (msg.closeId != 0)
		{
			SCOPED_WRITE_LOCK(m_activeFilesLock, lock);
			m_activeFiles.erase(msg.closeId);
		}

		{
			SCOPED_WRITE_LOCK(m_outputFilesLock, lock);
			m_outputFiles.erase(msg.fileName.data);
		}

		RemoveWrittenFile(msg.process, msg.fileName.data);

		out.result = uba::DeleteFileW(msg.fileName.data);
		out.errorCode = GetLastError();
		out.directoryTableSize = RegisterDeleteFile(msg.fileNameKey, msg.fileName.data);
		return true;
	}

	bool Session::CopyFile(CopyFileResponse& out, const CopyFileMessage& msg)
	{
		out.fromName.Append(msg.fromName);
		out.toName.Append(msg.toName);

		SCOPED_WRITE_LOCK(m_activeFilesLock, lock);
		u32 closeId = m_wantsOnCloseIdCounter++;
		if (!m_activeFiles.try_emplace(closeId, ActiveFile{ msg.toName.data, msg.toKey }).second)
		{
			m_logger.Error(TC("SHOULD NOT HAPPEN"));
		}
		out.closeId = closeId;
		return true;
	}

	bool Session::MoveFile(MoveFileResponse& out, const MoveFileMessage& msg)
	{
		out.result = MoveFileExW(msg.fromName.data, msg.toName.data, msg.flags);
		out.errorCode = ERROR_SUCCESS;
		if (!out.result)
			out.errorCode = GetLastError();
		RegisterCreateFileForWrite(msg.toKey, msg.toName.data, msg.toName.count, true);
		out.directoryTableSize = RegisterDeleteFile(msg.fromKey, msg.fromName.data);
		return true;
	}

	bool Session::Chmod(ChmodResponse& out, const ChmodMessage& msg)
	{
		#if PLATFORM_WINDOWS
		UBA_ASSERT(false); // This is not used
		#else
		out.errorCode = 0;
		if (chmod(msg.fileName.data, (mode_t)msg.fileMode) == 0)
		{
			RegisterCreateFileForWrite(msg.fileNameKey, msg.fileName.data, msg.fileName.count, true);
			return true;
		}
		out.errorCode = errno;
		#endif
		return true;
	}

	bool Session::CreateDirectory(CreateDirectoryResponse& out, const CreateDirectoryMessage& msg)
	{
		out.result = uba::CreateDirectoryW(msg.name.data);
		if (out.result)
			RegisterCreateFileForWrite(msg.nameKey, msg.name.data, msg.name.count, true);
		out.errorCode = GetLastError();
		return true;
	}

	bool Session::GetFullFileName(GetFullFileNameResponse& out, const GetFullFileNameMessage& msg)
	{
		UBA_ASSERTF(false, TC("SHOULD NOT HAPPEN (only remote)"));
		return SearchPathForFile(m_logger, out.fileName, msg.fileName.data, msg.process.m_virtualApplicationDir.c_str());
	}

	bool Session::GetListDirectoryInfo(ListDirectoryResponse& out, tchar* dirName, const StringKey& dirKey)
	{
		u32 tableOffset;
		u32 tableSize = WriteDirectoryEntries(dirKey, dirName, tableOffset);
		out.tableOffset = tableOffset;
		out.tableSize = tableSize;
		return true;
	}

	bool Session::WriteFilesToDisk(ProcessImpl& process, WrittenFile** files, u32 fileCount)
	{

		auto writeFile = [&](WrittenFile& file)
		{
			if (ShouldWriteToDisk(file.name.c_str(), file.name.size()))
			{
				// This is to kill I/O when writing lots of pdb/dlls in parallel
				#if PLATFORM_WINDOWS
				constexpr u32 bottleneckMax = 32;
				bool shouldBottleneck = false;//useOverlap;
				static Bottleneck bottleneck(bottleneckMax);
				auto bng = MakeGuard([&]() { if (shouldBottleneck) bottleneck.Leave(); });
				#endif

				#if PLATFORM_WINDOWS
				shouldBottleneck = true;
				bottleneck.Enter();
				#endif

				u64 fileSize = file.mappingWritten;
				u8* mem = MapViewOfFile(file.mappingHandle, FILE_MAP_READ, 0, fileSize);
				if (!mem)
					return m_logger.Error(TC("Failed to map view of filehandle for read %s (%s)"), file.name.c_str(), LastErrorToText().data);

				//PrefetchVirtualMemory(mem, fileSize);

				auto memClose = MakeGuard([&](){ UnmapViewOfFile(mem, fileSize, file.name.c_str()); });

				// Seems like best combo (for windows at least) is to use writes with overlap and max 16 at the same time.
				// On one machine we get twice as fast without overlap if no bottleneck. On another machine (ntfs compression on) we get twice as slow without overlap
				// Both machines behaves well with overlap AND bottleneck. Both machine are 128 logical core thread rippers.
				bool useFileMapForWrite = fileSize > 0; // ::CreateFileMappingW does not work for zero-length files
				bool useOverlap = false;//fileSize > 8 * 1024 * 1024;


				u32 attributes = DefaultAttributes();
				if (useOverlap)
					attributes |= FILE_FLAG_OVERLAPPED;

				FileAccessor destinationFile(m_logger, file.name.c_str());

				if (useFileMapForWrite)
				{
					if (!destinationFile.CreateMemoryWrite(false, attributes, fileSize, m_tempPath.data))
						return false;
					memcpy(destinationFile.GetData(), mem, fileSize);
				}
				else
				{
					if (!destinationFile.CreateWrite(false, attributes, fileSize, m_tempPath.data))
						return false;

					#if PLATFORM_WINDOWS
					//shouldBottleneck = fileSize > 64 * 1024 * 1024;
					//if (shouldBottleneck)
					//	bottleneck.Enter();
					#endif

					if (!destinationFile.Write(mem, fileSize))
						return false;
				}
				if (u64 time = file.lastWriteTime)
					if (!SetFileLastWriteTime(destinationFile.GetHandle(), time))
						return m_logger.Error(TC("Failed to set file time on filehandle for %s"), file.name.c_str());

				if (!destinationFile.Close())
					return false;
			}
			else
			{
				// Delete existing file to make sure it is not picked up (since it is out of date)
				uba::DeleteFileW(file.name.c_str());
			}

			if (IsRarelyReadAfterWritten(process, file.name.c_str(), file.name.size()) || file.mappingWritten > m_keepOutputFileMemoryMapsThreshold)
			{
				m_workManager->AddWork([mh = file.mappingHandle]()
					{
						CloseFileMapping(mh);
					}, 1, TC("CFM"));
				//CloseFileMapping(file.mappingHandle);
			}
			else
			{
				StringBuffer<> name;
				Storage::GetMappingString(name, file.mappingHandle, 0);
				SCOPED_WRITE_LOCK(m_fileMappingTableLookupLock, lookupLock);
				auto insres = m_fileMappingTableLookup.try_emplace(file.key);
				FileMappingEntry& entry = insres.first->second;
				lookupLock.Leave();
				SCOPED_WRITE_LOCK(entry.lock, entryCs);
				entry.handled = true;
				entry.mapping = file.mappingHandle;
				entry.mappingOffset = 0;
				entry.size = file.mappingWritten;
				entry.isDir = false;
				entry.success = true;

				SCOPED_WRITE_LOCK(m_fileMappingTableMemLock, lock);
				BinaryWriter writer(m_fileMappingTableMem, m_fileMappingTableSize);
				writer.WriteStringKey(file.key);
				writer.WriteString(name);
				writer.Write7BitEncoded(file.mappingWritten);
				u32 newSize = (u32)writer.GetPosition();
				m_fileMappingTableSize = (u32)newSize;
			}
			file.mappingHandle = {};
			return true;
		};

		for (u32 i=0; i!=fileCount; ++i)
			if (!writeFile(*files[i]))
				return false;
		/*
		Event events[32];
		UBA_ASSERT(fileCount < 32);


		for (u32 i=0; i!=fileCount; ++i)
		{
			events[i].Create(true);

			m_workManager->AddWork([&, ii = i]()
				{
					writeFile(*files[ii]);
					events[ii].Set();
				}, 1, TC(""));
		}
		for (u32 i=0; i!=fileCount; ++i)
			events[i].IsSet();
		*/
		return true;
	}

	bool Session::AllocFailed(Process& process, const tchar* allocType, u32 error)
	{
		m_logger.Warning(TC("Allocation failed in %s (%s).. process will sleep and try again"), allocType, LastErrorToText(error).data);
		return true;
	}

	bool Session::GetNextProcess(Process& process, bool& outNewProcess, NextProcessInfo& outNextProcess, u32 prevExitCode, BinaryReader& statsReader)
	{
		if (!m_getNextProcessFunction)
		{
			outNewProcess = false;
			return true;
		}

		outNewProcess = m_getNextProcessFunction(process, outNextProcess, prevExitCode);
		if (!outNewProcess)
			return true;

		m_trace.ProcessEnvironmentUpdated(process.GetId(), outNextProcess.description.c_str(), statsReader.GetPositionData(), statsReader.GetLeft());

		return true;
	}

	bool Session::CustomMessage(Process& process, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 recvSize = reader.ReadU32();
		u32* sendSize = (u32*)writer.AllocWrite(4);
		void* sendData = writer.GetData() + writer.GetPosition();
		u32 written = 0;
		if (m_customServiceFunction)
			written = m_customServiceFunction(process, reader.GetPositionData(), recvSize, sendData, u32(writer.GetCapacityLeft()));
		*sendSize = written;
		writer.AllocWrite(written);
		return true;
	}

	void Session::FileEntryAdded(StringKey fileNameKey, u64 lastWritten, u64 size)
	{
	}

	bool Session::FlushWrittenFiles(ProcessImpl& process)
	{
		return true;
	}

	bool Session::UpdateEnvironment(ProcessImpl& process, const tchar* reason, bool resetStats)
	{
		if (!resetStats)
			return true;
		StackBinaryWriter<16 * 1024> writer;
		process.m_processStats.Write(writer);
		process.m_sessionStats.Write(writer);
		process.m_storageStats.Write(writer);
		process.m_systemStats.Write(writer);
		m_trace.ProcessEnvironmentUpdated(process.GetId(), reason, writer.GetData(), writer.GetPosition());
		process.m_processStats = {};
		process.m_sessionStats = {};
		process.m_storageStats = {};
		process.m_systemStats = {};
		return true;
	}

	void Session::PrintSessionStats(Logger& logger)
	{
		u64 mappingBufferSize;
		u32 mappingBufferCount;
		m_fileMappingBuffer.GetSizeAndCount(MappedView_Transient, mappingBufferSize, mappingBufferCount);
		logger.Info(TC("  DirectoryTable      %7u %9s"), u32(m_directoryTable.m_lookup.size()), BytesToText(GetDirectoryTableSize()).str);
		logger.Info(TC("  MappingTable        %7u %9s"), u32(m_fileMappingTableLookup.size()), BytesToText(GetFileMappingSize()).str);
		logger.Info(TC("  MappingBuffer       %7u %9s"), mappingBufferCount, BytesToText(mappingBufferSize).str);
		m_stats.Print(logger);
		logger.Info(TC(""));
	}

	bool Session::CreateProcessJobObject()
	{
		#if PLATFORM_WINDOWS
		m_processJobObject = CreateJobObject(nullptr, nullptr);
		if (!m_processJobObject)
			return m_logger.Error(TC("Failed to create process job object"));
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = { };
		info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(m_processJobObject, JobObjectExtendedLimitInformation, &info, sizeof(info));
		#endif
		return true;
	}


	void Session::GetSystemInfo(StringBufferBase& out)
	{
		u32 cpuCount = GetLogicalProcessorCount();
		u32 cpuGroupCount = GetProcessorGroupCount();

		StringBuffer<128> cpuCountStr;
		if (cpuGroupCount != 1)
			cpuCountStr.AppendValue(cpuGroupCount).Append('x');
		cpuCountStr.AppendValue(cpuCount/cpuGroupCount);

		u64 totalMemoryInKilobytes = 0;
		StringBuffer<128> hzStr;

		#if PLATFORM_WINDOWS
		GetPhysicallyInstalledSystemMemory(&totalMemoryInKilobytes);

		Vector<PROCESSOR_POWER_INFORMATION> procInfos;
		procInfos.resize(cpuCount);
		if (CallNtPowerInformation(ProcessorInformation, NULL, 0, procInfos.data(), cpuCount*sizeof(PROCESSOR_POWER_INFORMATION)) == STATUS_SUCCESS)
			hzStr.Appendf(TC(" @ %.1fGHz"), float(procInfos[0].MaxMhz) / 1000.0f);
		#else
		u64 throwAway;
		GetMemoryInfo(throwAway, totalMemoryInKilobytes);
		totalMemoryInKilobytes /= 1024;
		// TODO get freq from /proc/cpuinfo
		hzStr.Append(" @ ?GHz");
		#endif

		const tchar* capacityStr = TC("NoLimit");
		u64 capacity = m_storage.GetStorageCapacity();
		BytesToText temp(capacity);
		if (capacity)
			capacityStr = temp.str;
		out.Appendf(TC("CPU:%s%s Mem:%ugb Cas:%s"), cpuCountStr.data, hzStr.data, u32(totalMemoryInKilobytes/(1024*1024)), capacityStr);

		StringBuffer<128> zone;
		if (m_storage.GetZone(zone))
			out.Append(TC(" Zone:")).Append(zone);

		#if PLATFORM_WINDOWS
		if (!IsRunningWine())
		{
			DWORD value = 0;
			DWORD valueSize = 4;
			LSTATUS res = RegGetValueW(HKEY_LOCAL_MACHINE, TC("SYSTEM\\CurrentControlSet\\Control\\FileSystem"), TC("NtfsDisableLastAccessUpdate"), RRF_RT_REG_DWORD, NULL, &value, &valueSize);
			if (res != ERROR_SUCCESS)
			{
				m_logger.Detail(TC("Failed to retreive ntfs registry key (%i)"), res);
			}
			else
			{
				u32 lastAccessSettingsValue = value & 0xf;
				if (lastAccessSettingsValue == 0 || lastAccessSettingsValue == 2)
					out.Append(TC(" NtfsLastAccessEnabled"));
			}
		}
		#endif

		if (!m_extraInfo.empty())
			out.Append(m_extraInfo);

		#if UBA_DEBUG
		out.Append(TC(" DEBUG"));
		#endif
	}

	bool Session::GetMemoryInfo(u64& outAvailable, u64& outTotal)
	{
#if PLATFORM_WINDOWS

		MEMORYSTATUSEX memStatus = { sizeof(memStatus) };
		if (!GlobalMemoryStatusEx(&memStatus))
		{
			outAvailable = 0;
			outTotal = 0;
			return m_logger.Error(TC("Failed to get global memory status (%s)"), LastErrorToText().data);
		}

		// Page file can grow and we want to use the absolute max size to figure out when we need to wait to start new processes
		if (m_maxPageSize == ~u64(0))
		{
			m_maxPageSize = 0;
			wchar_t str[1024];
			DWORD strBytes = sizeof(str);
			LSTATUS res = RegGetValueW(HKEY_LOCAL_MACHINE, TC("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management"), TC("PagingFiles"), RRF_RT_REG_MULTI_SZ, NULL, str, &strBytes);
			if (res == ERROR_SUCCESS)
			{
				wchar_t* line = str;
				for (; size_t lineLen = wcslen(line); line += lineLen + 1)
				{
					if (lineLen < 3)
						continue;

					u64 maxSizeMb = 0;

					StringBuffer<8> drive;
					drive.Append(line, 3); // Get drive root path

					if (drive[0] == '?') // Drive '?' can exist when "Automatically manage paging file size for all drives".. 
					{
						// We can use ExistingPageFiles registry key to figure out which drive...
						// This key can contain multiple page files normally.. have no idea if it can contain multiple when drive is '?'.. but for now, just use the first
						wchar_t str2[1024];
						DWORD str2Bytes = sizeof(str2);
						res = RegGetValueW(HKEY_LOCAL_MACHINE, TC("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management"), TC("ExistingPageFiles"), RRF_RT_REG_MULTI_SZ, NULL, str2, &str2Bytes);
						if (res != ERROR_SUCCESS)
							continue;

						auto colon = wcschr(str2, ':'); // Path is something like \??\C:\pagefile.sys or similar.. let's search for : and use character in front of it.
						if (colon == nullptr || colon == str2)
							continue;

						drive[0] = colon[-1];
					}
					else
					{
						const wchar_t* maxSizeStr = wcsrchr(line, ' ');
						
						if (!maxSizeStr || !StringBuffer<32>(maxSizeStr + 1).Parse(maxSizeMb))
						{
							m_logger.Warning(TC("Unrecognized page file information format (please report): %s"), line);
							continue;
						}

						if (maxSizeMb) // Custom set page file size
						{
							m_maxPageSize += maxSizeMb * 1024 * 1024;
							continue;
						}
					}

					// Max possible system-managed page file
					maxSizeMb = Max(u64(memStatus.ullTotalPhys) * 3, 4ull * 1024 * 1024 * 1024);

					// Check if disk is limiting factor of system-managed page file
					// Page file can be max 1/8 of volume size and ofc not more than free space
					ULARGE_INTEGER totalNumberOfBytes;
					ULARGE_INTEGER totalNumberOfFreeBytes;
					if (!GetDiskFreeSpaceExW(drive.data, NULL, &totalNumberOfBytes, &totalNumberOfFreeBytes))
						return m_logger.Error(TC("GetDiskFreeSpaceExW failed to get information about %s (%s)"), drive.data, LastErrorToText().data);

					u64 maxDiskPageFileSize = Min(totalNumberOfBytes.QuadPart / 8, totalNumberOfFreeBytes.QuadPart);
					m_maxPageSize += Min(maxDiskPageFileSize, maxSizeMb);
				}
			}
		}

		u64 currentPageSize = memStatus.ullTotalPageFile - memStatus.ullTotalPhys;
		if (currentPageSize < m_maxPageSize)
		{
			outTotal = memStatus.ullTotalPhys + m_maxPageSize;
			outAvailable = memStatus.ullAvailPageFile + (m_maxPageSize - currentPageSize);
		}
		else
		{
			outTotal = memStatus.ullTotalPageFile;
			outAvailable = memStatus.ullAvailPageFile;
		}
#elif PLATFORM_MAC
        vm_size_t page_size;
            mach_port_t mach_port;
            mach_msg_type_number_t count;
            vm_statistics64_data_t vm_stats;

            mach_port = mach_host_self();
            count = sizeof(vm_stats) / sizeof(natural_t);
            if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
                KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO,
                                                (host_info64_t)&vm_stats, &count))
            {
                outAvailable = (int64_t)vm_stats.free_count * (int64_t)page_size;
                outTotal = vm_stats.wire_count + vm_stats.active_count + vm_stats.inactive_count + vm_stats.free_count;
//                long long used_memory = ((int64_t)vm_stats.active_count +
//                                         (int64_t)vm_stats.inactive_count +
//                                         (int64_t)vm_stats.wire_count) *  (int64_t)page_size;
            }

#else
		static long mem = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE);
		outTotal = mem;
		outAvailable = mem;
#endif
		return true;
	}

	void Session::WriteSummary(BinaryWriter& writer, const Function<void(Logger& logger)>& summaryFunc)
	{
		struct SummaryLogWriter : public LogWriter
		{
			SummaryLogWriter(BinaryWriter& w) : writer(w) {}
			virtual void BeginScope() override {}
			virtual void EndScope() override {}
			virtual void Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen) override
			{
				writer.WriteString(str, strLen);
				++count;
			}

			BinaryWriter& writer;
			u32 count = 0;
		};
		u32* lineCount = (u32*)writer.AllocWrite(4);

		SummaryLogWriter logWriter(writer);
		LoggerWithWriter logger(logWriter, TC(""));

		summaryFunc(logger);

		*lineCount = logWriter.count;
	}

	float Session::UpdateCpuLoad()
	{
		u64 idleTime = 0;
		u64 totalTime = 0;

#if PLATFORM_WINDOWS
		u64 kernelTime, userTime;
		if (!GetSystemTimes((FILETIME*)&idleTime, (FILETIME*)&kernelTime, (FILETIME*)&userTime))
			return m_cpuLoad;
		totalTime = kernelTime + userTime;
#elif PLATFORM_LINUX
		int fd = open("/proc/stat", O_RDONLY | O_CLOEXEC);
		if (fd != -1)
		{
			char buffer[512];
			int size = read(fd, buffer, sizeof_array(buffer) - 1);
			if (size != -1)
			{
				buffer[size] = 0;
				if (char* endl = strchr(buffer, '\n'))
				{
					u64 values[16];
					u32 valueCount = 0;
					*endl = 0;
					char* parsePos = buffer;
					char* space = nullptr;
					do
					{
						space = strchr(parsePos, ' ');
						if (space)
							*space = 0;
						values[valueCount++] = strtoull(parsePos, nullptr, 10);

						parsePos = space + 1;
					} while (space);

					// user: normal processes executing in user mode
					// nice: niced processes executing in user mode
					// system: processes executing in kernel mode
					// idle: twiddling thumbs
					// iowait: waiting for I/O to complete
					// irq: servicing interrupts
					// softirq: servicing softirqs
					if (valueCount > 6)
					{
						u64 work = values[0] + values[1] + values[2];
						idleTime = values[3] + values[4] + values[5] + values[6];
						totalTime = work + idleTime;
					}
				}
			}
			close(fd);
		}
		// TODO: Read "/proc/stat" to get cpu cycles
#else // PLATFORM_MAC
        mach_msg_type_number_t  CpuMsgCount = 0;
        processor_flavor_t CpuInfoType = PROCESSOR_CPU_LOAD_INFO;;
        natural_t CpuCount = 0;
        processor_cpu_load_info_t CpuData;
        host_t host = mach_host_self();

        int res = 0;
        u64 work = 0;
        idleTime = 0;

        res = host_processor_info(host, CpuInfoType, &CpuCount, (processor_info_array_t *)&CpuData, &CpuMsgCount);
		if(res != KERN_SUCCESS)
        {
				return m_logger.Error(TC("Kernel error: %s"), mach_error_string(res));
        }

        for(int i = 0; i < (int)CpuCount; i++)
        {
                work += CpuData[i].cpu_ticks[CPU_STATE_SYSTEM];
                work += CpuData[i].cpu_ticks[CPU_STATE_USER];
                work += CpuData[i].cpu_ticks[CPU_STATE_NICE];
                idleTime += CpuData[i].cpu_ticks[CPU_STATE_IDLE];
        }

		totalTime = work + idleTime;
#endif

		u64 totalTimeSinceLastTime = totalTime - m_previousTotalCpuTime;
		u64 idleTimeSinceLastTime = idleTime - m_previousIdleCpuTime;

		float cpuLoad = 1.0f - ((totalTimeSinceLastTime > 0) ? (float(idleTimeSinceLastTime) / float(totalTimeSinceLastTime)) : 0);

		m_previousTotalCpuTime = totalTime;
		m_previousIdleCpuTime = idleTime;

		// TODO: This is the wrong solution.. but can't repro the bad values some people get
		if (cpuLoad >= 0 && cpuLoad <= 1.0f)
			m_cpuLoad = cpuLoad;

		return m_cpuLoad;
	}


	void Session::ThreadTraceLoop()
	{
		while (true)
		{
			if (m_traceThreadEvent.IsSet(500))
				break;
			TraceSessionUpdate();
		}
	}

	void Session::TraceSessionUpdate()
	{
	}

	void GetNameFromArguments(StringBufferBase& out, const tchar* arguments, bool addCounterSuffix)
	{
		const tchar* start = arguments;
		const tchar* it = arguments;
		StringBuffer<> temp;
		while (true)
		{
			if (*it != ' ' && *it != 0)
			{
				++it;
				continue;
			}
			temp.Clear();
			temp.Append(start, u64(it - start));
			if (!temp.Contains(TC(".rsp")) && !temp.Contains(TC(".bat")))
			{
				if (*it == 0)
					break;
				++it;
				start = it;
				continue;
			}
			out.AppendFileName(temp.data);
			if (out.data[out.count -1] == '"')
				out.Resize(out.count -1);
			break;
		}

		if (out.IsEmpty())
			out.Append(TC("NoGoodName"));

		static Atomic<u32> counter;
		if (addCounterSuffix)
			out.Append('_').AppendValue(counter++);
	}
}

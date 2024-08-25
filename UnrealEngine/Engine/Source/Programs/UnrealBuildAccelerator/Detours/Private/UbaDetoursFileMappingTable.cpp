// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaDetoursFileMappingTable.h"
#include "UbaDirectoryTable.h"
#include "UbaPathUtils.h"

namespace uba
{
	MappedFileTable::MappedFileTable(MemoryBlock& memoryBlock) : m_memoryBlock(memoryBlock), m_lookup(&memoryBlock)
	{
	}

	void MappedFileTable::Init(const u8* mem, u32 tableCount, u32 tableSize)
	{
		m_mem = mem;
		m_lookup.reserve(tableCount + 100);
		ParseNoLock(tableSize);
	}

	void MappedFileTable::ParseNoLock(u32 tableSize)
	{
		u32 startPosition = m_memPosition;
		if (tableSize <= startPosition)
			return;

		BinaryReader reader(m_mem, startPosition);
		while (reader.GetPosition() != tableSize)
		{
			UBA_ASSERT(reader.GetPosition() < tableSize);
			StringKey g = reader.ReadStringKey();
			StringBuffer<1024> mappedFileName;
			reader.ReadString(mappedFileName);
			u64 size = reader.Read7BitEncoded();
			auto insres = m_lookup.try_emplace(g);
			FileInfo& info = insres.first->second;
			if (!insres.second)
				continue;
			info.fileNameKey = g;
			info.name = m_memoryBlock.Strdup(mappedFileName.data);
			//info.refCount = 0;
			info.size = size;
			//info.dwDesiredAccess = GENERIC_READ;
		}
		m_memPosition = tableSize;
	}

	void MappedFileTable::Parse(u32 tableSize)
	{
		SCOPED_WRITE_LOCK(m_lookupLock, lock);
		ParseNoLock(tableSize);
	}

	void MappedFileTable::SetDeleted(const StringKey& key, const tchar* name, bool deleted)
	{
		SCOPED_WRITE_LOCK(m_lookupLock, lock);
		auto it = m_lookup.find(key);
		if (it == m_lookup.end())
			return;
		FileInfo& sourceInfo = it->second;
		sourceInfo.deleted = deleted;
		sourceInfo.lastDesiredAccess = 0;
	}

	void Rpc_CreateFileW(const tchar* fileName, const StringKey& fileNameKey, u8 access, tchar* outNewName, u64 newNameCapacity, u64& outSize, u32& outCloseId, bool lock)
	{
		TimerScope ts(g_stats.createFile);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_CreateFile);
		writer.WriteString(fileName);
		writer.WriteStringKey(fileNameKey);
		writer.WriteByte(access);
		writer.Flush();
		BinaryReader reader;
		reader.ReadString(outNewName, newNameCapacity);
		outSize = reader.ReadU64();
		outCloseId = reader.ReadU32();
		u32 mappedFileTableSize = reader.ReadU32();
		u32 directoryTableSize = u32(reader.ReadU32());
		pcs.Leave();
		DEBUG_LOG_PIPE(L"CreateFile", L"%ls (%ls)", (access == 0 ? L"ATTRIB" : ((access & GENERIC_WRITE) ? L"WRITE" : L"READ")), fileName);

		if (lock)
			g_mappedFileTable.Parse(mappedFileTableSize);
		else
			g_mappedFileTable.ParseNoLock(mappedFileTableSize);
		g_directoryTable.ParseDirectoryTable(directoryTableSize);
	}

	u32 Rpc_UpdateDirectory(const StringKey& dirKey, const tchar* dirName, u64 dirNameLen, bool lockDirTable)
	{
		u32 directoryTableSize;
		u32 tableOffset;
		{
			TimerScope ts(g_stats.listDirectory);
			SCOPED_WRITE_LOCK(g_communicationLock, pcs);
			BinaryWriter writer;
			writer.WriteByte(MessageType_ListDirectory);
			writer.WriteString(dirName, dirNameLen);
			writer.WriteStringKey(dirKey);
			writer.Flush();
			BinaryReader reader;
			directoryTableSize = u32(reader.ReadU32());
			u64 to = reader.ReadU32();
			tableOffset = to == InvalidTableOffset ? ~u32(0) : (u32)to;
			pcs.Leave();
			DEBUG_LOG_PIPE(L"ListDirectory", L"(%ls)", dirName);
		}
		if (lockDirTable)
			g_directoryTable.ParseDirectoryTable(directoryTableSize);
		else
			g_directoryTable.ParseDirectoryTableNoLock(directoryTableSize);
		return tableOffset;
	}

	void Rpc_UpdateCloseHandle(const tchar* handleName, u32 closeId, bool deleteOnClose, const tchar* newName, u64 mappingHandle, u64 mappingWritten, bool success)
	{
		u32 directoryTableSize;
		{
			TimerScope ts(g_stats.closeFile);
			SCOPED_WRITE_LOCK(g_communicationLock, pcs);
			BinaryWriter writer;
			writer.WriteByte(MessageType_CloseFile);
			writer.WriteString(handleName);
			writer.WriteU32(closeId);
			//writer.WriteU32(attributes); // TODO THIS
			writer.WriteBool(deleteOnClose);
			writer.WriteBool(success);
			writer.WriteU64(u64(mappingHandle));
			writer.WriteU64(mappingWritten);
			if (*newName)
			{
				StringBuffer<> fixedName;
				FixPath(fixedName, newName);
				StringBuffer<> forKey(fixedName);
				if (CaseInsensitiveFs)
					forKey.MakeLower();
				StringKey newNameKey = ToStringKey(forKey);
				writer.WriteStringKey(newNameKey);
				writer.WriteString(fixedName);
			}
			else
				writer.WriteStringKey(StringKeyZero);
			writer.Flush();
			BinaryReader reader;
			directoryTableSize = reader.ReadU32();
			pcs.Leave();
			DEBUG_LOG_PIPE(L"CloseFile", L"");
		}
		g_directoryTable.ParseDirectoryTable(directoryTableSize);
	}

	void Rpc_UpdateTables()
	{
		TimerScope ts(g_stats.updateTables);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_UpdateTables);
		writer.Flush();
		BinaryReader reader;
		u32 directoryTableSize = reader.ReadU32();
		pcs.Leave();
		g_directoryTable.ParseDirectoryTable(directoryTableSize);
		DEBUG_LOG_PIPE(L"UpdateTables", L"");
	}

	u32 Rpc_GetEntryOffset(const StringKey& entryNameKey, const tchar* entryName, u64 entryNameLen, bool checkIfDir)
	{
		u32 dirTableOffset = ~u32(0);
		StringBuffer<> entryNameForKey;
		entryNameForKey.Append(entryName, entryNameLen);
		if (CaseInsensitiveFs)
			entryNameForKey.MakeLower();
		else if (entryNameForKey.count == 1 && entryNameForKey[0] == '/')
			checkIfDir = true;

		CHECK_PATH(entryNameForKey.data);
		DirectoryTable::Exists exists = g_directoryTable.EntryExists(entryNameKey, entryNameForKey.data, entryNameLen, checkIfDir, &dirTableOffset);
		if (exists != DirectoryTable::Exists_Maybe)
			return dirTableOffset;

		const tchar* lastPathSeparator = TStrrchr(entryName, PathSeparator);
		UBA_ASSERTF(lastPathSeparator, TC("No path separator found in %s"), TStrlen(entryName) > 0 ? entryName : TC("(NULL)"));

		#if PLATFORM_WINDOWS
		UBA_ASSERT(wcsncmp(entryName, g_systemTemp.data, g_systemTemp.count) != 0);
		#endif

		u64 dirNameLen = lastPathSeparator - entryName;
		DirHash hash(entryNameForKey.data, dirNameLen);

		if (Rpc_UpdateDirectory(hash.key, entryName, dirNameLen) == ~u32(0))
			return ~u32(0);

		SCOPED_WRITE_LOCK(g_directoryTable.m_lookupLock, lookLock);
		auto dirFindIt = g_directoryTable.m_lookup.find(hash.key);
		if (dirFindIt == g_directoryTable.m_lookup.end())
			return ~u32(0);
		auto& dir = dirFindIt->second;

		if (checkIfDir)
			return u32(dir.tableOffset) | 0x80000000; // Use significant bit to say that this is a dir

		g_directoryTable.PopulateDirectory(hash.open, dir);

		SCOPED_READ_LOCK(dir.lock, lock);
		auto findIt = dir.files.find(entryNameKey);
		if (findIt == dir.files.end())
			return ~u32(0);
		return findIt->second;
	}

	void Rpc_GetFullFileName(const tchar*& path, u64& pathLen, StringBufferBase& tempBuf, bool useVirtualName)
	{
		StringKey fileNameKey;
		bool isAbsolute = IsWindows ? (pathLen > 1 && path[1] == ':') : (pathLen > 0 && path[0] == '/');
		if (isAbsolute)
		{
			FixPath(tempBuf, path);
			if (CaseInsensitiveFs)
				tempBuf.MakeLower();
			fileNameKey = ToStringKey(tempBuf);
			tempBuf.Clear();
		}

		u32 mappedFileTableSize;

		{
			TimerScope ts(g_stats.getFullFileName);
			SCOPED_WRITE_LOCK(g_communicationLock, pcs);
			BinaryWriter writer;
			writer.WriteByte(MessageType_GetFullFileName);
			writer.WriteString(path);
			writer.WriteStringKey(fileNameKey);
			writer.Flush();
			BinaryReader reader;
			reader.ReadString(tempBuf);
			if (useVirtualName)
			{
				tempBuf.Clear();
				reader.ReadString(tempBuf);
			}
			else
				reader.SkipString();
			mappedFileTableSize = reader.ReadU32();
			DEBUG_LOG_PIPE(L"GetFileName", L"(%ls)", tempBuf);
		}

		g_mappedFileTable.Parse(mappedFileTableSize);
		path = tempBuf.data;
		pathLen = tempBuf.count;
	}

	DirHash::DirHash(const tchar* str, u64 strLen)
	{
		CHECK_PATH(str);
		open.Update(str, strLen);
		key = ToStringKey(open);
	}
}

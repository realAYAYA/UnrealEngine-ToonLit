// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBinaryReaderWriter.h"
#include "UbaDetoursShared.h"

namespace uba
{
	struct MemoryFile;


	struct FileInfo
	{
		u64 size = InvalidValue;
		bool tracked = false;
		StringKey fileNameKey;
		const tchar* name = nullptr;
		const tchar* originalName = nullptr;

		u32 lastDesiredAccess = 0;

		MemoryFile* memoryFile = nullptr;

		// This is the Filemap handle and mapped view that represents the File HANDLE.
		// It can be used for other mappings too but doesnt' have to.
		bool isFileMap = false;
		bool deleted = false; // Only used by remote builds since directory table might not contain local temporary files
		void* trueFileMapHandle = 0;
		u64 trueFileMapOffset = 0;
		u8* fileMapMem = nullptr;
		u8* fileMapMemEnd = nullptr;

		#if PLATFORM_WINDOWS
		u32 fileMapDesiredAccess = PAGE_READONLY;
		u32 fileMapViewDesiredAccess = FILE_MAP_READ; // This is the parameter to MapViewOfFile
		#endif
	};

	class MappedFileTable
	{
	public:
		MappedFileTable(MemoryBlock& memoryBlock);

		void Init(const u8* mem, u32 tableCount, u32 tableSize);

		void ParseNoLock(u32 tableSize);
		void Parse(u32 tableSize);
		void SetDeleted(const StringKey& key, const tchar* name, bool deleted);

		MemoryBlock& m_memoryBlock;
		const u8* m_mem = 0;
		u32 m_memPosition = 0;
		ReaderWriterLock m_lookupLock;
		GrowingUnorderedMap<StringKey, FileInfo> m_lookup;
		ReaderWriterLock m_memLookupLock;
		UnorderedMap<const void*, int> m_memLookup;
	};

	void Rpc_CreateFileW(const tchar* fileName, const StringKey& fileNameKey, u8 access, tchar* outNewName, u64 newNameCapacity, u64& outSize, u32& outCloseId, bool lock);
	u32  Rpc_UpdateDirectory(const StringKey& dirKey, const tchar* dirName, u64 dirNameLen, bool lockDirTable = true);
	void Rpc_UpdateCloseHandle(const tchar* handleName, u32 closeId, bool deleteOnClose, const tchar* newName, u64 mappingHandle, u64 mappingWritten, bool success);
	void Rpc_UpdateTables();
	u32  Rpc_GetEntryOffset(const StringKey& entryNameKey, const tchar* entryName, u64 entryNameLen, bool checkIfDir = false);
	void Rpc_GetFullFileName(const tchar*& path, u64& pathLen, StringBufferBase& tempBuf, bool useVirtualName);

	struct DirHash
	{
		DirHash(const tchar* str, u64 strLen);
		StringKey key;
		StringKeyHasher open;
	};
}
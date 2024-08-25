// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#define Local_GetLongPathNameW uba::GetLongPathNameW

#include "UbaFile.h"
#include "UbaFileMapping.h"
#include "UbaLogger.h"
#include "UbaMemory.h"
#include "UbaPathUtils.h"
#include "UbaStats.h"
#include <oodle2.h>

#define UBA_USE_SPARSEFILE 0

namespace uba
{
	class FileAccessor;
	class Trace;
	class WorkManager;
	struct DirectoryEntry;
	extern CasKey EmptyFileKey;
	
	class Storage
	{
	public:
		virtual ~Storage() {}
		virtual bool StoreCompressed() const = 0;
		virtual void PrintSummary(Logger& logger) = 0;
		virtual bool Reset() = 0;
		virtual bool SaveCasTable(bool deleteIsRunningfile, bool deleteDropped = true) = 0;
		virtual u64 GetStorageCapacity() = 0;
		virtual bool GetZone(StringBufferBase& out) = 0;
		virtual bool HasProxy(u32 clientId) { return false; }

		virtual bool DecompressFileToMemory(const tchar* fileName, FileHandle fileHandle, u8* dest, u64 decompressedSize) = 0;
		virtual bool DecompressMemoryToMemory(u8* compressedData, u8* writeData, u64 decompressedSize, const tchar* readHint) = 0;
		virtual bool CreateDirectory(const tchar* dir) = 0;
		virtual bool DeleteCasForFile(const tchar* file) = 0;

		struct RetrieveResult { CasKey casKey; u64 size = 0; MappedView view; };
		virtual bool RetrieveCasFile(RetrieveResult& out, const CasKey& casKey, const tchar* hint, FileMappingBuffer* mappingBuffer = nullptr, u64 memoryMapAlignment = 1, bool allowProxy = true) = 0;

		struct CachedFileInfo { CasKey casKey; };
		virtual bool VerifyAndGetCachedFileInfo(CachedFileInfo& out, StringKey fileNameKey, u64 verifiedLastWriteTime, u64 verifiedSize) = 0;

		virtual bool StoreCasFile(CasKey& out, const tchar* fileName, const CasKey& casKeyOverride = CasKeyZero, bool deferCreation = false) = 0;
		virtual bool StoreCasFile(CasKey& out, StringKey fileNameKey, const tchar* fileName, FileMappingHandle mappingHandle, u64 mappingOffset, u64 fileSize, const tchar* hint, bool deferCreation = false, bool keepMappingInMemory = false) = 0;
		virtual bool DropCasFile(const CasKey& casKey, bool forceDelete, const tchar* hint) = 0;
		virtual bool CalculateCasKey(CasKey& out, const tchar* fileName) = 0;
		virtual bool CopyOrLink(const CasKey& casKey, const tchar* destination, u32 fileAttributes) = 0;
		virtual bool FakeCopy(const CasKey& casKey, const tchar* destination) = 0;
#if !UBA_USE_SPARSEFILE
		virtual bool GetCasFileName(StringBufferBase& out, const CasKey& casKey) = 0;
#endif
		virtual MappedView MapView(const CasKey& casKey, const tchar* hint) = 0;
		virtual void UnmapView(const MappedView& view, const tchar* hint) = 0;

		virtual void ReportFileWrite(const tchar* fileName) = 0;
		
		virtual StorageStats& Stats() = 0;
		virtual void AddStats(StorageStats& stats) = 0;

		static void GetMappingString(StringBufferBase& out, FileMappingHandle mappingHandle, u64 offset);

		virtual void SetTrace(Trace* trace, bool detailed) {}
		virtual void Ping() {}
	};

	struct StorageCreateInfo
	{
		StorageCreateInfo(const tchar* rootDir_, LogWriter& w) : writer(w), rootDir(rootDir_) {}
		LogWriter& writer;
		const tchar* rootDir;
		u64 casCapacityBytes = 20llu * 1024 * 1024 * 1024;
		u32 maxParallelCopyOrLink = 1000;
		bool storeCompressed = true;
		WorkManager* workManager = nullptr;
	};

	class StorageImpl : public Storage
	{
	public:
		StorageImpl(const StorageCreateInfo& info, const tchar* logPrefix = TC("UbaStorage"));
		virtual ~StorageImpl();

		bool LoadCasTable(bool logStats = true);
		bool CheckCasContent(u32 workerCount);

		virtual bool SaveCasTable(bool deleteIsRunningfile, bool deleteDropped = true) override;
		virtual u64 GetStorageCapacity() override;
		virtual bool GetZone(StringBufferBase& out) override;
		virtual bool Reset() override;
		bool DeleteAllCas();

		virtual bool StoreCompressed() const final { return m_storeCompressed; }
		virtual void PrintSummary(Logger& logger) override;

		virtual bool DecompressFileToMemory(const tchar* fileName, FileHandle fileHandle, u8* dest, u64 decompressedSize) override;
		virtual bool CreateDirectory(const tchar* dir) override;
		virtual bool DeleteCasForFile(const tchar* file) override;
		virtual bool RetrieveCasFile(RetrieveResult& out, const CasKey& casKey, const tchar* hint, FileMappingBuffer* mappingBuffer = nullptr, u64 memoryMapAlignment = 1, bool allowProxy = true) override;
		virtual bool VerifyAndGetCachedFileInfo(CachedFileInfo& out, StringKey fileNameKey, u64 verifiedLastWriteTime, u64 verifiedSize) override;
		virtual bool StoreCasFile(CasKey& out, const tchar* fileName, const CasKey& casKeyOverride = CasKeyZero, bool deferCreation = false) override;
		virtual bool StoreCasFile(CasKey& out, StringKey fileNameKey, const tchar* fileName, FileMappingHandle mappingHandle, u64 mappingOffset, u64 fileSize, const tchar* hint, bool deferCreation = false, bool keepMappingInMemory = false) override;
		virtual bool DropCasFile(const CasKey& casKey, bool forceDelete, const tchar* hint) override;
		virtual bool CalculateCasKey(CasKey& out, const tchar* fileName) override;
		virtual bool CopyOrLink(const CasKey& casKey, const tchar* destination, u32 fileAttributes) override;
		virtual bool FakeCopy(const CasKey& casKey, const tchar* destination) override;
#if !UBA_USE_SPARSEFILE
		virtual bool GetCasFileName(StringBufferBase& out, const CasKey& casKey) override;
#endif
		virtual MappedView MapView(const CasKey& casKey, const tchar* hint) override;
		virtual void UnmapView(const MappedView& view, const tchar* hint) override;

		virtual void ReportFileWrite(const tchar* fileName) override;
		
		virtual StorageStats& Stats() final;
		virtual void AddStats(StorageStats& stats) override;

		struct CasEntry;

		virtual bool HasCasFile(const CasKey& casKey, CasEntry** out = nullptr);
		bool EnsureCasFile(const CasKey& casKey, const tchar* fileName);
		CasKey CalculateCasKey(const tchar* fileName, FileHandle fileHandle, u64 fileSize, bool storeCompressed);
		CasKey CalculateCasKey(u8* fileMem, u64 fileSize, bool storeCompressed);

		struct WriteResult { FileMappingHandle mappingHandle; u64 size = InvalidValue; u64 offset = InvalidValue; };
		virtual bool WriteCompressed(WriteResult& out, const tchar* from, const tchar* toFile);
		bool WriteCompressed(WriteResult& out, const tchar* from, FileHandle readHandle, u8* readMem, u64 fileSize, const tchar* toFile);
		bool WriteMemToCompressedFile(FileAccessor& destination, u32 workCount, const u8* uncompressedData, u64 fileSize, u64 maxUncompressedBlock, u64& totalWritten);
		bool WriteCasFileNoCheck(WriteResult& out, const tchar* fileName, const tchar* casFile, bool storeCompressed);
		bool WriteCasFile(WriteResult& out, const tchar* fileName, const CasKey& casKey);
		bool VerifyExisting(bool& outReturnValue, ScopedWriteLock& entryLock, const CasKey& casKey, CasEntry& casEntry, StringBufferBase& casFile);
		bool AddCasFile(const tchar* fileName, const CasKey& casKey, bool deferCreation);
		void CasEntryAccessed(const CasKey& casKey);
		virtual bool IsDisallowedPath(const tchar* fileName);
		virtual bool DecompressMemoryToMemory(u8* compressedData, u8* writeData, u64 decompressedSize, const tchar* readHint) override;
		bool DecompressMemoryToFile(u8* compressedData, FileAccessor& destination, u64 decompressedSize, bool useNoBuffering);

		void CasEntryAccessed(CasEntry& entry);
		void CasEntryWritten(CasEntry& entry, u64 size);
		void CasEntryDeleted(CasEntry& entry, u64 size);
		void AttachEntry(CasEntry& entry);
		void DetachEntry(CasEntry& entry);
		void TraverseAllCasFiles(const tchar* dir, u32 recursion, const Function<void(const StringBufferBase& fullPath, const DirectoryEntry& e)>& func);
		void TraverseAllCasFiles(const Function<void(const CasKey& key)>& func);
		void CheckAllCasFiles();
		void HandleOverflow();
		bool OpenCasDataFile(u32 index, u64 size);
		bool CreateCasDataFiles();


		WorkManager* m_workManager;
		MutableLogger m_logger;

		StringBuffer<> m_rootDir;
		StringBuffer<> m_tempPath;

		ReaderWriterLock m_fileTableLookupLock;
		struct FileEntry { ReaderWriterLock lock; CasKey casKey = CasKeyZero; u64 size = 0; u64 lastWritten = 0; bool verified = false; };
		UnorderedMap<StringKey, FileEntry> m_fileTableLookup;

		ReaderWriterLock m_casLookupLock;
		struct CasEntry
		{
			ReaderWriterLock lock;
			CasKey key;
			CasEntry* prevAccessed = nullptr;
			CasEntry* nextAccessed = nullptr;
			u64 size = 0;
			bool verified = false; // This flag needs to be set for below flags to be reliable. if this is false below flags are assumptions
			bool exists = false; // File exists on disk
			bool dropped = false; // This file is not seen anymore. will be deleted during shutdown
			bool beingWritten = false; // This is set while file is being written (when coming from network)..
			bool disallowed = false; // This is set if cas is created from disallowed file

			FileMappingHandle mappingHandle;
			u64 mappingOffset = 0;
			u64 mappingSize = 0;
		};

		UnorderedMap<CasKey, CasEntry> m_casLookup;
		ReaderWriterLock m_accessLock;
		CasEntry* m_newestAccessed = nullptr;
		CasEntry* m_oldestAccessed = nullptr;
		u64 m_casTotalBytes = 0;
		u64 m_casMaxBytes = 0;
		u64 m_casCapacityBytes = 0;
		u64 m_casEvictedBytes = 0;
		u32 m_casEvictedCount = 0;
		u64 m_casDroppedBytes = 0;
		u32 m_casDroppedCount = 0;
		bool m_overflowReported = false;
		bool m_storeCompressed = false;

		u32 m_maxParallelCopyOrLink;
		ReaderWriterLock m_activeCopyOrLinkLock;
		Event m_activeCopyOrLinkEvent;
		u32 m_activeCopyOrLink = 0;

		ReaderWriterLock m_casTableLoadSaveLock;
		bool m_casTableLoaded = false;

		FileMappingBuffer m_casDataBuffer;

		ReaderWriterLock m_deferredCasCreationLookupLock;
		UnorderedMap<CasKey, TString> m_deferredCasCreationLookup;
		UnorderedMap<const tchar*, CasKey, HashString, EqualString> m_deferredCasCreationLookupByName;

		DirectoryCache m_dirCache;

		static constexpr u64 BufferSlotSize = 16*1024*1024;
		static constexpr u64 BufferSlotHalfSize = BufferSlotSize/2; // This must be three times a msg size or more.
		u8* PopBufferSlot();
		void PushBufferSlot(u8* slot);

		ReaderWriterLock m_compSlotsLock;
		Vector<u8*> m_compSlots;

		OodleLZ_Compressor m_createCasCompressor = OodleLZ_Compressor_Kraken;
		OodleLZ_CompressionLevel m_createCasCompressionLevel = OodleLZ_CompressionLevel_SuperFast;

		OodleLZ_Compressor m_sendCasCompressor = OodleLZ_Compressor_Kraken;
		OodleLZ_CompressionLevel m_sendCasCompressionLevel = OodleLZ_CompressionLevel_SuperFast;

		StorageStats m_stats;

		StorageImpl(const StorageImpl&) = delete;
		void operator=(const StorageImpl&) = delete;
	};
}

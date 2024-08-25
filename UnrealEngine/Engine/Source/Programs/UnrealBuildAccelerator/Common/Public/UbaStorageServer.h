// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaNetworkServer.h"
#include "UbaStorage.h"

namespace uba
{
	struct StorageServerCreateInfo : StorageCreateInfo
	{
		StorageServerCreateInfo(NetworkServer& s, const tchar* rootDir_, LogWriter& writer) : StorageCreateInfo(rootDir_, writer), server(s) { workManager = &server; }
		NetworkServer& server;
		const tchar* zone = TC("");
	};

	class StorageServer final : public StorageImpl
	{
	public:
		StorageServer(const StorageServerCreateInfo& info);
		~StorageServer();

		bool RegisterDisallowedPath(const tchar* path);

		virtual bool GetZone(StringBufferBase& out) override;
		virtual bool RetrieveCasFile(RetrieveResult& out, const CasKey& casKey, const tchar* hint, FileMappingBuffer* mappingBuffer = nullptr, u64 memoryMapAlignment = 1, bool allowProxy = true) override;
		virtual bool StoreCasFile(CasKey& out, StringKey fileNameKey, const tchar* fileName, FileMappingHandle mappingHandle, u64 mappingOffset, u64 fileSize, const tchar* hint, bool deferCreation = false, bool keepMappingInMemory = false) override;
		virtual bool WriteCompressed(WriteResult& out, const tchar* from, const tchar* toFile) override;
		virtual bool IsDisallowedPath(const tchar* fileName) override;
		virtual void SetTrace(Trace* trace, bool detailed) override;
		virtual bool HasProxy(u32 clientId) override;
		void OnDisconnected(u32 clientId);
		bool HandleMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer);
		bool WaitForWritten(CasEntry& casEntry, ScopedWriteLock& entryLock, const tchar* hint);

		u16 PopId();
		void PushId(u16 id);

		NetworkServer& m_server;
		bool m_traceFetch = false;
		bool m_traceStore = false;

		Guid m_uid;

		static constexpr u8 ServiceId = StorageServiceId;


		ReaderWriterLock m_waitEntriesLock;

		struct WaitEntry
		{
			WaitEntry() : Done(false) {}
			Event Done;
			bool Success = false;
			u32 refCount = 0;
		};
		UnorderedMap<CasKey, WaitEntry> m_waitEntries;


		struct ActiveStore
		{
			u32 clientId = ~0u;
			MappedView mappedView;
			CasEntry* casEntry = nullptr;
			Atomic<u64> totalWritten;
			Atomic<u64> recvCasTime;
			u64 fileSize = 0;
			u64 actualSize = 0;
			bool error = false;
		};
		ReaderWriterLock m_activeStoresLock;
		UnorderedMap<u16, ActiveStore> m_activeStores;


		struct ActiveFetch
		{
			u32 clientId = ~0u;
			Atomic<u64> left;
			CasKey casKey;
			Atomic<u64> sendCasTime;

			FileHandle readFileHandle = InvalidFileHandle;
			MappedView mappedView;
			u8* memoryBegin = nullptr;
			u8* memoryPos = nullptr;
			bool ownsMapping = false;

			void Release(StorageServer& server, const tchar* reason);
		};
		ReaderWriterLock m_activeFetchesLock;
		UnorderedMap<u16, ActiveFetch> m_activeFetches;


		ReaderWriterLock m_availableIdsLock;
		Vector<u16> m_availableIds;
		u16 m_availableIdsHigh = 1;


		struct ExternalFileMapping
		{
			FileMappingHandle mappingHandle;
			u64 mappingOffset;
			u64 fileSize;
		};
		ReaderWriterLock m_externalFileMappingsLock;
		UnorderedMap<StringKey, ExternalFileMapping> m_externalFileMappings;


		struct ProxyEntry
		{
			u32 clientId = ~0u;
			TString zone;
			TString host;
			u16 port = 0;
		};
		ReaderWriterLock m_proxiesLock;
		UnorderedMap<StringKey, ProxyEntry> m_proxies;

		TString m_zone;

		struct Info
		{
			TString zone;
			TString internalAddress;
			u64 storageSize = 0;
			u16 proxyPort = 0;
		};
		ReaderWriterLock m_connectionInfoLock;
		UnorderedMap<u32, Info> m_connectionInfo;

		ReaderWriterLock m_loadCasTableLock;

		Trace* m_trace = nullptr;

		Vector<TString> m_disallowedPaths;
	};
}

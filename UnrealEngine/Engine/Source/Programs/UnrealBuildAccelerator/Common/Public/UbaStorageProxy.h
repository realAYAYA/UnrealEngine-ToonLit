// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaFileMapping.h"
#include "UbaLogger.h"
#include "UbaNetwork.h"

namespace uba
{
    class NetworkClient;
	class NetworkServer;
	class StorageImpl;
	struct BinaryReader;
	struct BinaryWriter;
	struct ConnectionInfo;
	struct MessageInfo;

	class StorageProxy
	{
	public:
		StorageProxy(NetworkServer& server, NetworkClient& client, const Guid& storageServerUid, const tchar* name, StorageImpl* localStorage = nullptr);
		~StorageProxy();
		void PrintSummary();

	protected:
		u16 PopId();
		void PushId(u16 id);
		bool HandleMessage(const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer);

		struct MessageInFlight;
		void HandleReceivedData(MessageInFlight& mif);

		bool UpdateFetch(u32 clientId, u16 fetchId, u64 segmentSize);
		bool SendEnd(const CasKey& key);

		static constexpr u8 ServiceId = StorageServiceId;

		NetworkServer& m_server;
		NetworkClient& m_client;
		StorageImpl* m_localStorage;

		MutableLogger m_logger;

		Guid m_storageServerUid;

		TString m_name;

		Atomic<u32> m_inProcessClientId;

		struct FileEntry
		{
			ReaderWriterLock lock;
			u8* memory = nullptr;
			u64 size = 0;
			Atomic<u64> received;
			CasKey casKey;
			u32 trackId = 0;
			u16 fetchId = 0;
			bool storeCompressed = false;
			bool sendEnd = false;
			bool error = false;
			bool available = false;
			Vector<MessageInFlight*> messagesInFlight;
		};

		ReaderWriterLock m_filesLock;
		UnorderedMap<CasKey, FileEntry> m_files;

		struct ActiveFetch
		{
			FileEntry* file = nullptr;
			u64 fetchedSize = 0;
			u32 clientId = ~0u;
			u32 connectionId = 0;
		};

		ReaderWriterLock m_activeFetchesLock;
		UnorderedMap<u16, ActiveFetch> m_activeFetches;

		ReaderWriterLock m_largeFileLock;

		Vector<u16> m_availableIds;
		u16 m_availableIdsHigh = 1;
	};
}

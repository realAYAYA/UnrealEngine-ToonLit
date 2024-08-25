// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaStorageProxy.h"
#include "UbaFileAccessor.h"
#include "UbaNetworkClient.h"
#include "UbaNetworkMessage.h"
#include "UbaNetworkServer.h"
#include "UbaStorageClient.h"

namespace uba
{
	struct StorageProxy::MessageInFlight
	{
		MessageInFlight(StorageProxy& p, FileEntry& f, u8* readBuffer, u32 fi)
		:	proxy(p)
		,	file(f)
		,	message(p.m_client, ServiceId, StorageMessageType_FetchSegment, writer)
		,	reader(readBuffer, 0, SendMaxSize)
		,	fetchIndex(fi)
		{
			writer.WriteU16(file.fetchId);
			writer.WriteU32(fetchIndex + 1);
		}

		StorageProxy& proxy;
		FileEntry& file;
		StackBinaryWriter<16> writer;
		NetworkMessage message;
		BinaryReader reader;
		struct DeferredResponse { u32 clientId; u16 fetchId; MessageInfo info; };
		List<DeferredResponse> deferredResponses;
		u32 fetchIndex;
		bool done = false;
		bool error = false;
	};


	StorageProxy::StorageProxy(NetworkServer& server, NetworkClient& client, const Guid& storageServerUid, const tchar* name, StorageImpl* localStorage)
	:	m_server(server)
	,	m_client(client)
	,	m_localStorage(localStorage)
	,	m_logger(client.GetLogWriter(), TC("StorageProxy"))
	,	m_storageServerUid(storageServerUid)
	,	m_name(name)
	{
		m_server.RegisterOnClientDisconnected(0, [this](const Guid& clientUid, u32 clientId)
			{
				SCOPED_WRITE_LOCK(m_activeFetchesLock, lock);
				for (auto it=m_activeFetches.begin(); it!=m_activeFetches.end();)
				{
					if (it->second.clientId != clientId)
					{
						++it;
						continue;
					}
					PushId(it->first);
					it = m_activeFetches.erase(it);
				}
			});

		m_server.RegisterService(StorageServiceId,
			[this](const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
			{
				return HandleMessage(connectionInfo, messageInfo, reader, writer);
			},
			[](u8 messageType)
			{
				return ToString(StorageMessageType(messageType));
			}
		);

		m_client.RegisterOnDisconnected([this]() { m_logger.isMuted = true; });
	}

	StorageProxy::~StorageProxy()
	{
		m_server.UnregisterService(StorageServiceId);
		for (auto& kv : m_files)
			delete[] kv.second.memory;
	}

	void StorageProxy::PrintSummary()
	{
		LoggerWithWriter logger(m_logger.m_writer);
		logger.Info(TC("  -- Uba storage proxy stats summary --"));
		logger.Info(TC("  Total fetched           %6s"), BytesToText(0).str);
		logger.Info(TC("  Total provided          %6s"), BytesToText(0).str);
		logger.Info(TC(""));
	}

	u16 StorageProxy::PopId()
	{
		if (m_availableIds.empty())
			return m_availableIdsHigh++;
		u16 storeId = m_availableIds.back();
		m_availableIds.pop_back();
		return storeId;
	}

	void StorageProxy::PushId(u16 id)
	{
		m_availableIds.push_back(id);
	}

	bool StorageProxy::HandleMessage(const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
	{
		LoggerWithWriter logger(g_consoleLogWriter, TC(""));

		StackBinaryWriter<1024> writer2;
		StackBinaryReader<SendMaxSize> reader2;

		switch (messageInfo.type)
		{
		case StorageMessageType_Connect:
			{
				StringBuffer<> clientName;
				reader.ReadString(clientName);
				u32 clientVersion = reader.ReadU32();
				if (clientVersion != StorageNetworkVersion)
				{
					m_logger.Error(TC("Different network versions. Client: %u, Server: %u. Disconnecting"), clientVersion, StorageNetworkVersion);
					return false;
				}
				bool isInProcessClient = reader.ReadBool();
				if (isInProcessClient)
					m_inProcessClientId = connectionInfo.GetId();

				//m_logger.Info(TC("%s connected"), clientName.data);
				writer.WriteGuid(m_storageServerUid);
				return true;
			}

		case StorageMessageType_FetchBegin:
			{
				reader.ReadBool(); // Wants proxy

				CasKey casKey = reader.ReadCasKey();
				StringBuffer<> hint;
				reader.ReadString(hint);

				SCOPED_WRITE_LOCK(m_filesLock, filesLock);
				auto insres = m_files.try_emplace(casKey);
				FileEntry& file = insres.first->second;
				filesLock.Leave();

				SCOPED_WRITE_LOCK(file.lock, fileLock);

				while (true)
				{
					if (file.memory)
						break;

					bool useLocalStorage = false; // Seems like it might be deadlocks in this code.. need to revisit
					bool storeCompressed = true;
					if (useLocalStorage && m_localStorage && IsCompressed(casKey) && m_inProcessClientId && connectionInfo.GetId() != m_inProcessClientId)
					{
						// We need to leave this lock here since the in-process storage client might be asking for this file too and then we can end up in a deadlock
						fileLock.Leave();

						bool hasCas = m_localStorage->EnsureCasFile(casKey, nullptr);
						StringBuffer<> casFile;
						hasCas = hasCas && m_localStorage->GetCasFileName(casFile, casKey);

						// Enter lock again, and also check if another thread might have already handled this file while we looked if it existed in local storage
						fileLock.Enter();
						if (file.memory)
							break;

						if (hasCas)
						{
							FileAccessor sourceFile(m_logger, casFile.data);
							if (sourceFile.OpenMemoryRead())
							{
								u64 fileSize = sourceFile.GetSize();
								file.memory = new u8[fileSize];
								if (!file.memory)
									return false;
								file.size = fileSize;
								file.received = fileSize;
								memcpy(file.memory, sourceFile.GetData(), fileSize);
								file.available = true;
							}
						}
					}

					if (!file.memory)
					{
						file.trackId = m_client.TrackWorkStart(StringBuffer<512>().AppendFileName(hint.data).data);

						NetworkMessage msg(m_client, ServiceId, StorageMessageType_FetchBegin, writer2);
						writer2.WriteBool(false); // Wants proxy
						writer2.WriteCasKey(casKey);
						writer2.WriteString(hint);
						writer2.WriteBytes(reader.GetPositionData(), reader.GetLeft());

						SCOPED_READ_LOCK(m_largeFileLock, largeFileLock);
						if (!msg.Send(reader2))
						{
							file.error = true;
							return m_logger.Error(TC("FetchBegin failed for cas file %s (%s). Requested by %s"), CasKeyString(casKey).str, hint.data, GuidToString(connectionInfo.GetUid()).str);
						}
						largeFileLock.Leave();

						BinaryReader tempReader(reader2.GetPositionData(), 0, reader2.GetLeft());
						u16 fetchId = tempReader.ReadU16();
						if (fetchId == 0)
						{
							file.error = true;
							m_logger.Error(TC("FetchBegin failed for cas file %s (%s). Requested by %s"), CasKeyString(casKey).str, hint.data, GuidToString(connectionInfo.GetUid()).str);
							writer.WriteU16(0);
							return true;
						}
						u64 fileSize = tempReader.Read7BitEncoded();
						file.memory = new u8[fileSize];
						file.size = fileSize;

						u8 flags = tempReader.ReadByte();
						storeCompressed = (flags >> 0) & 1;
						bool sendEnd = (flags >> 1) & 1;
						u64 fetchedSize = tempReader.GetLeft();

						memcpy(file.memory, tempReader.GetPositionData(), fetchedSize);

						if (sendEnd && fetchedSize == fileSize)
							SendEnd(casKey);

						file.received = fetchedSize;
						file.fetchId = fetchId;
						file.sendEnd = sendEnd;
					}

					file.storeCompressed = storeCompressed;
					file.casKey = casKey;

					if (file.received < file.size)
					{
						u64 segmentSize = m_client.GetMessageMaxSize() - 5; // This is server response size - header.. TODO: Should be taken from server
						u32 segmentCount = u32(file.size / segmentSize);
						file.messagesInFlight.resize(segmentCount);
						for (u32 i=0; i!=segmentCount; ++i)
						{
							u64 offset = file.received + segmentSize * i;
							u8* memory = file.memory + offset;
							auto mif = new MessageInFlight(*this, file, memory, i);
							file.messagesInFlight[i] = mif;
						}

						// Move the additional messages to a job to be able to return this one quickly.
						m_server.AddWork([f = &file, segmentCount, this]()
							{
								SCOPED_WRITE_LOCK(m_largeFileLock, lock);
								//TrackWorkScope tws(m_client, TC("SEGMENTS"));
								auto& file = *f;
								for (u32 i=0; i!=segmentCount; ++i)
								{
									auto mif = file.messagesInFlight[i];

									bool res = mif->message.SendAsync(mif->reader, [](bool error, void* userData)
										{
											auto mif = (MessageInFlight*)userData;
											mif->error = error;
											mif->proxy.m_server.AddWork([mif]() { mif->proxy.HandleReceivedData(*mif); }, 1, TC(""));
										}, mif);
									if (!res)
									{
										// TODO: Don't leak mif
										mif->error = true;
									}
								}
							}, 1, TC(""));
					}
					else
					{
						m_client.TrackWorkEnd(file.trackId);
					}
					break;
				}

				if (file.error)
					return false;
				fileLock.Leave();

				u16 fetchId = u16(~0);

				u64 headerSize = sizeof(u16) + Get7BitEncodedCount(file.size) + sizeof(u8);
				u64 fetchedSize = Min(file.size, m_client.GetMessageMaxSize() - m_client.GetMessageReceiveHeaderSize() - headerSize);

				if (fetchedSize < file.size)
				{
					SCOPED_WRITE_LOCK(m_activeFetchesLock, lock);
					fetchId = PopId();
					auto res = m_activeFetches.try_emplace(fetchId);
					UBA_ASSERT(res.second);
					ActiveFetch& fetch = res.first->second;
					fetch.clientId = connectionInfo.GetId();
					lock.Leave();

					fetch.fetchedSize = fetchedSize;
					fetch.file = &file;
				}

				u8 flags = 0;
				flags |= u8(file.storeCompressed) << 0;

				writer.WriteU16(fetchId);
				writer.Write7BitEncoded(file.size);
				writer.WriteByte(flags);
				writer.WriteBytes(file.memory, fetchedSize);

				return true;
			}
		case StorageMessageType_FetchSegment:
			{
				u16 fetchId = reader.ReadU16();
				u32 fetchIndex = reader.ReadU32() - 1;

				SCOPED_READ_LOCK(m_activeFetchesLock, activeLock);
				auto findIt = m_activeFetches.find(fetchId);
				UBA_ASSERT(findIt != m_activeFetches.end());
				ActiveFetch& fetch = findIt->second;
				u32 clientId = fetch.clientId;
				activeLock.Leave();

				FileEntry& file = *fetch.file;
				SCOPED_WRITE_LOCK(file.lock, fileLock);
				if (file.error)
					return false;

				if (!file.available)
				{
					if (auto mif = file.messagesInFlight[fetchIndex])
					{
						UBA_ASSERT(clientId == connectionInfo.GetId());
						mif->deferredResponses.push_back({clientId, fetchId, messageInfo});
						messageInfo = {};
						return true;
					}
				}
				fileLock.Leave();

				u64 headerSize = sizeof(u16) + Get7BitEncodedCount(file.size) + sizeof(u8);
				u64 firstFetchSize = m_client.GetMessageMaxSize() - m_client.GetMessageReceiveHeaderSize() - headerSize;
				u64 segmentSize = m_client.GetMessageMaxSize() - 5; // This is server response size - header.. TODO: Should be taken from server
				u64 offset = firstFetchSize + segmentSize * (fetchIndex);
				if (offset + segmentSize > file.size)
					segmentSize = file.size - offset;
				writer.WriteBytes(file.memory + offset, segmentSize);
				return UpdateFetch(fetch.clientId, fetchId, segmentSize);
			}
		case StorageMessageType_FetchEnd:
			{
				return true;
			}
		default:
			{
				NetworkMessage msg(m_client, ServiceId, messageInfo.type, writer2);
				writer2.WriteBytes(reader.GetPositionData(), reader.GetLeft());
				if (!msg.Send(reader2))
					return false;
				writer.WriteBytes(reader2.GetPositionData(), reader2.GetLeft());
				return true;
			}
		}
	}

	void StorageProxy::HandleReceivedData(MessageInFlight& mif)
	{
		auto& file = mif.file;
		if (mif.error)
		{
			SCOPED_WRITE_LOCK(file.lock, fileLock);
			file.error = true;
		}
		else
		{
			mif.message.ProcessAsyncResults(mif.reader);
		}

		SCOPED_WRITE_LOCK(file.lock, fileLock);

		UBA_ASSERT(file.messagesInFlight[mif.fetchIndex] == &mif);
		file.messagesInFlight[mif.fetchIndex] = nullptr;
		file.received += mif.reader.GetLeft();
		bool finished = file.received == file.size;
		if (finished)
			file.available = true;
		fileLock.Leave();

		if (finished)
		{
			m_client.TrackWorkEnd(file.trackId);
			SendEnd(file.casKey);
		}


		for (auto& r : mif.deferredResponses)
		{
			if (UpdateFetch(r.clientId, r.fetchId, mif.reader.GetLeft()) && !mif.error)
				m_server.SendResponse(r.info, mif.reader.GetPositionData(), u32(mif.reader.GetLeft()));
			else
				m_server.SendResponse(r.info, nullptr, 0);
		}

		delete &mif;
	}

	bool StorageProxy::UpdateFetch(u32 clientId, u16 fetchId, u64 segmentSize)
	{
		SCOPED_WRITE_LOCK(m_activeFetchesLock, activeLock);
		auto findIt = m_activeFetches.find(fetchId);
		if (findIt == m_activeFetches.end())
		{
			// This can happen if we have async downloading and client is disconnected
			//m_logger.Info(TC("Failed to find active fetch with id %u"), fetchId);
			return false;
		}

		ActiveFetch& fetch = findIt->second;
		if (fetch.clientId != clientId)
		{
			// This can happen if we have async downloading and client is disconnected and new client have reused fetch id
			//m_logger.Info(TC("Active fetch %i has a different client id."), fetchId);
			return false;
		}

		fetch.fetchedSize += segmentSize;
		if (fetch.fetchedSize != fetch.file->size)
			return true;

		m_activeFetches.erase(findIt);
		PushId(fetchId);
		return true;
	}

	bool StorageProxy::SendEnd(const CasKey& key)
	{
		StackBinaryWriter<128> writer;
		NetworkMessage msg(m_client, ServiceId, StorageMessageType_FetchEnd, writer);
		writer.WriteCasKey(key);
		return msg.Send();
	}

}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaStorageServer.h"
#include "UbaNetworkServer.h"
#include "UbaTrace.h"

namespace uba
{
	StorageServer::StorageServer(const StorageServerCreateInfo& info)
	:	StorageImpl(info, TC("UbaStorageServer"))
	,	m_server(info.server)
	{
		m_zone = info.zone;

		if (!CreateGuid(m_uid))
			UBA_ASSERT(false);

		m_server.RegisterService(ServiceId,
			[this](const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
			{
				return HandleMessage(connectionInfo, messageInfo.type, reader, writer);
			},
			[](u8 messageType)
			{
				return ToString(StorageMessageType(messageType));
			}
		);

		m_server.RegisterOnClientConnected(ServiceId, [this](const Guid& clientUid, u32 clientId)
			{
				SCOPED_WRITE_LOCK(m_loadCasTableLock, lock);
				if (!m_casTableLoaded)
					LoadCasTable(true);
			});

		m_server.RegisterOnClientDisconnected(ServiceId, [this](const Guid& clientUid, u32 clientId)
			{
				OnDisconnected(clientId);
			});
	}

	StorageServer::~StorageServer()
	{
		UBA_ASSERT(m_waitEntries.empty());
		UBA_ASSERT(m_proxies.empty());
		m_server.UnregisterOnClientDisconnected(ServiceId);
		m_server.UnregisterService(ServiceId);
	}

	bool StorageServer::RegisterDisallowedPath(const tchar* path)
	{
		m_disallowedPaths.push_back(path);
		return true;
	}

	bool StorageServer::GetZone(StringBufferBase& out)
	{
		if (m_zone.empty())
			return false;
		out.Append(m_zone);
		return true;
	}

	bool StorageServer::RetrieveCasFile(RetrieveResult& out, const CasKey& casKey, const tchar* hint, FileMappingBuffer* mappingBuffer, u64 memoryMapAlignment, bool allowProxy)
	{
		UBA_ASSERT(!mappingBuffer);
		UBA_ASSERT(casKey != CasKeyZero);
		out.casKey = casKey;
		out.size = InvalidValue;

		CasKey actualKey = casKey;
		if (m_storeCompressed)
			actualKey = AsCompressed(casKey, true);

		SCOPED_WRITE_LOCK(m_waitEntriesLock, waitLock);
		WaitEntry& waitEntry = m_waitEntries[actualKey];
		++waitEntry.refCount;
		waitLock.Leave();

		auto g = MakeGuard([&]()
			{
				SCOPED_WRITE_LOCK(m_waitEntriesLock, waitLock2);
				if (!--waitEntry.refCount)
					m_waitEntries.erase(actualKey);
			});

		if (HasCasFile(actualKey))
			return true;

		u64 startTime = GetTime();
		u32 timeout = 0;
		while (!waitEntry.Done.IsSet(timeout)) // TODO WaitMultipleObjects (additional work and Done)
		{
			timeout = m_server.DoAdditionalWork() ? 0 : 50;

			u64 waited = GetTime() - startTime;
			if (TimeToMs(waited) > 4 * 60 * 1000) // 4 minutes timeout
			{
				m_logger.Info(TC("Timed out waiting %s for cas %s to be transferred from remote to storage (%s)"), TimeToText(waited).str, CasKeyString(casKey).str, hint);
				return false;
			}
		}
		return waitEntry.Success;
	}

	void StorageServer::ActiveFetch::Release(StorageServer& server, const tchar* reason)
	{
		if (mappedView.handle.IsValid())
		{
			if (ownsMapping)
			{
				UnmapViewOfFile(memoryBegin, mappedView.size, TC(""));
				CloseFileMapping(mappedView.handle);
				CloseFile(nullptr, readFileHandle);
			}
			else
				server.m_casDataBuffer.UnmapView(mappedView, TC("OnDisconnected"));
		}
		else
			server.PushBufferSlot(memoryBegin);
	}

	void StorageServer::OnDisconnected(u32 clientId)
	{
		{
			SCOPED_WRITE_LOCK(m_proxiesLock, lock);
			for (auto it=m_proxies.begin(); it!=m_proxies.end(); ++it)
			{
				ProxyEntry& e = it->second;
				if (e.clientId != clientId)
					continue;
				m_logger.Detail(TC("Proxy %s:%u for zone %s removed"), e.host.c_str(), e.port, e.zone.c_str());
				m_proxies.erase(it);
				break;
			}
		}
		{
			SCOPED_WRITE_LOCK(m_activeStoresLock, lock);
			for (auto it=m_activeStores.begin(); it!=m_activeStores.end();)
			{
				ActiveStore& store = it->second;
				if (store.clientId != clientId)
				{
					++it;
					continue;
				}

				{
					SCOPED_WRITE_LOCK(store.casEntry->lock, entryLock);
					store.casEntry->verified = false;
					store.casEntry->beingWritten = false;
					if (m_traceStore)
						m_trace->FileEndStore(clientId, store.casEntry->key);
				}

				m_casDataBuffer.UnmapView(store.mappedView, TC("OnDisconnected"));
				it = m_activeStores.erase(it);
			}
		}
		{
			SCOPED_WRITE_LOCK(m_activeFetchesLock, lock);
			for (auto it=m_activeFetches.begin(); it!=m_activeFetches.end();)
			{
				ActiveFetch& fetch = it->second;
				if (fetch.clientId != clientId)
				{
					++it;
					continue;
				}

				fetch.Release(*this, TC("OnDisconnected"));

				if (m_traceFetch)
					m_trace->FileEndFetch(clientId, AsCompressed(fetch.casKey, m_storeCompressed));

				it = m_activeFetches.erase(it);
			}
		}
	}

	bool StorageServer::StoreCasFile(CasKey& out, StringKey fileNameKey, const tchar* fileName, FileMappingHandle mappingHandle, u64 mappingOffset, u64 fileSize, const tchar* hint, bool deferCreation, bool keepMappingInMemory)
	{
		u8* fileMem = MapViewOfFile(mappingHandle, FILE_MAP_READ, mappingOffset, fileSize);
		UBA_ASSERT(fileMem);
		auto memClose = MakeGuard([&](){ UnmapViewOfFile(fileMem, fileSize, fileName); });

		bool storeCompressed = true;
		out = CalculateCasKey(fileMem, fileSize, storeCompressed);
		if (out == CasKeyZero)
			return false;

		SCOPED_WRITE_LOCK(m_fileTableLookupLock, lookupLock);
		auto insres = m_fileTableLookup.try_emplace(fileNameKey);
		FileEntry& fileEntry = insres.first->second;
		lookupLock.Leave();
		SCOPED_WRITE_LOCK(fileEntry.lock, entryLock);
		fileEntry.verified = true;
		fileEntry.casKey = out;
		fileEntry.size = fileSize;

		SCOPED_WRITE_LOCK(m_externalFileMappingsLock, externalFileLock);
		m_externalFileMappings.try_emplace(fileNameKey, ExternalFileMapping{mappingHandle, mappingOffset, fileSize});
		externalFileLock.Leave();

		if (!AddCasFile(fileName, fileEntry.casKey, deferCreation))
			return false;

		return true;
	}

	bool StorageServer::WriteCompressed(WriteResult& out, const tchar* from, const tchar* toFile)
	{
		StringBuffer<> fromForKey;
		fromForKey.Append(from);
		if (CaseInsensitiveFs)
			fromForKey.MakeLower();
		StringKey fileNameKey = ToStringKey(fromForKey);

		SCOPED_WRITE_LOCK(m_externalFileMappingsLock, lock);
		auto findIt = m_externalFileMappings.find(fileNameKey);
		if (findIt == m_externalFileMappings.end())
		{
			lock.Leave();
			return StorageImpl::WriteCompressed(out, from, toFile);
		}

		ExternalFileMapping& mapping = findIt->second;
		lock.Leave();
		u8* fileMem = MapViewOfFile(mapping.mappingHandle, FILE_MAP_READ, mapping.mappingOffset, mapping.fileSize);
		UBA_ASSERT(fileMem);
		auto memClose = MakeGuard([&](){ UnmapViewOfFile(fileMem, mapping.fileSize, from); });
		return StorageImpl::WriteCompressed(out, from, InvalidFileHandle, fileMem, mapping.fileSize, toFile);
	}

	bool StorageServer::IsDisallowedPath(const tchar* fileName)
	{
		for (auto& path : m_disallowedPaths)
			if (StartsWith(fileName, path.c_str()))
				return true;
		return false;
	}

	void StorageServer::SetTrace(Trace* trace, bool detailed)
	{
		m_trace = trace;
		m_traceFetch = detailed;
		m_traceStore = detailed;
	}

	bool StorageServer::HasProxy(u32 clientId)
	{
		SCOPED_READ_LOCK(m_proxiesLock, l);
		for (auto& kv : m_proxies)
			if (kv.second.clientId == clientId)
				return true;
		return false;
	}

	bool StorageServer::WaitForWritten(CasEntry& casEntry, ScopedWriteLock& entryLock, const tchar* hint)
	{
		int waitCount = 0;
		while (true)
		{
			if (!casEntry.beingWritten)
				return true;
			entryLock.Leave();
			Sleep(100);
			entryLock.Enter();

			if (++waitCount == 10000)
			{
				m_logger.Error(TC("Got store for file %s that is already being written. Waited 1000 seconds for it to finish without success."), hint);
				return false;
			}
		}
	}

	bool StorageServer::HandleMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer)
	{
		ActiveStore* firstStore = nullptr;
		ActiveStore tempStore;

		switch (messageType)
		{
			case StorageMessageType_Connect:
			{
				StringBuffer<> clientName;
				reader.ReadString(clientName);
				u32 clientVersion = reader.ReadU32();
				if (clientVersion != StorageNetworkVersion)
					return m_logger.Error(TC("Different network versions. Client: %u, Server: %u. Disconnecting"), clientVersion, StorageNetworkVersion);

				if (reader.ReadBool()) // is proxy
					return m_logger.Error(TC("Proxy is sending connect message. This path is not implemented"));
				u16 proxyPort = reader.ReadU16();
				SCOPED_WRITE_LOCK(m_connectionInfoLock, lock);
				Info& info = m_connectionInfo[connectionInfo.GetId()];
				info.zone = reader.ReadString();
				info.storageSize = reader.ReadU64();
				info.internalAddress = reader.ReadString();
				info.proxyPort = proxyPort;

				writer.WriteGuid(m_uid);
				return true;
			}

			case StorageMessageType_FetchBegin:
			{
				if (reader.ReadBool()) // Wants proxy
				{
					SCOPED_READ_LOCK(m_connectionInfoLock, lock);
					auto findIt = m_connectionInfo.find(connectionInfo.GetId());
					UBA_ASSERT(findIt != m_connectionInfo.end());
					Info& info = findIt->second;
					lock.Leave();

					if (!info.zone.empty())
					{
						// Zone logic is a bit special to be able to handle AWS setups.
						// AWS has zones named a,b,c etc in the end. us-east-1a, us-east-1b etc.
						// If host is also in AWS, then we want different proxies per zone but if host is not in AWS, we want one proxy for all zones in a region

						StringBuffer<256> proxyName;

						if (!m_zone.empty() && info.zone.size() == m_zone.size() && Equals(m_zone.c_str(), info.zone.c_str(), u64(m_zone.size() - 1)))
						{
							// Host is inside AWS and same region.... if aws availability zone is different than host, then we use proxy
							if (m_zone != info.zone)
								proxyName.Append(info.zone.c_str());
						}
						else if (m_zone != info.zone)
						{
							// We remove last character from zone to make sure all AWS availability zones in the same region use same proxy if host zone is outside AWS
							proxyName.Append(info.zone.c_str(), info.zone.size() - 1);
						}

						if (!proxyName.IsEmpty())
						{
							writer.WriteU16(u16(~0));
							writer.Write7BitEncoded(0);
							writer.WriteByte(1 << 2);

							auto proxyKey = ToStringKeyNoCheck(proxyName.data, proxyName.count);
							SCOPED_WRITE_LOCK(m_proxiesLock, proxiesLock);
							ProxyEntry& proxy = m_proxies[proxyKey];
							if (proxy.clientId == ~0u)
							{
								proxy.clientId = connectionInfo.GetId();
								proxy.host = info.internalAddress;
								proxy.port = info.proxyPort;
								proxy.zone = proxyName.data;

								m_logger.Detail(TC("%s:%u (%s) is assigned as proxy for zone %s"), proxy.host.c_str(), proxy.port, GuidToString(connectionInfo.GetUid()).str, proxy.zone.c_str());

								writer.WriteBool(true);
								writer.WriteU16(info.proxyPort);
								if (m_trace)
									m_trace->ProxyCreated(proxy.clientId, proxyName.data);
							}
							else
							{
								const tchar* proxyHost = proxy.host.c_str();
								if (connectionInfo.GetId() == proxy.clientId)
									proxyHost = TC("inprocess");

								writer.WriteBool(false);
								writer.WriteString(proxyHost);
								writer.WriteU16(proxy.port);
								if (m_trace)
									m_trace->ProxyUsed(connectionInfo.GetId(), proxyName.data);
							}
							return true;
						}
					}
				}

				u64 start = GetTime();
				CasKey casKey = reader.ReadCasKey();
				StringBuffer<> hint;
				reader.ReadString(hint);

				casKey = AsCompressed(casKey, m_storeCompressed);

				CasEntry* casEntry = nullptr;
				bool has = HasCasFile(casKey, &casEntry); // HasCasFile also writes deferred cas entries if in queue
				if (!has)
				{
					// Last resort.. use hint to load file into cas (hint should be renamed since it is now a critical parameter)
					// We better check the caskey first to make sure it is matching on the server

					CasKey checkedCasKey;
					{
						StringKey fileNameKey = CaseInsensitiveFs ? ToStringKeyLower(hint) : ToStringKey(hint);
						SCOPED_READ_LOCK(m_fileTableLookupLock, lookupLock);
						auto findIt = m_fileTableLookup.find(fileNameKey);
						if (findIt != m_fileTableLookup.end())
						{
							FileEntry& fileEntry = findIt->second;
							lookupLock.Leave();
							SCOPED_READ_LOCK(fileEntry.lock, entryLock);
							if (fileEntry.verified)
								checkedCasKey = fileEntry.casKey;
						}
					}
					if (checkedCasKey == CasKeyZero)
					{
						m_logger.Info(TC("Server did not find cas for %s in file table lookup. Recalculating cas key"), hint.data);
						if (!CalculateCasKey(checkedCasKey, hint.data))
						{
							m_logger.Error(TC("FetchBegin failed for cas file %s (%s) requested by %s. Can't calculate cas key for file"), CasKeyString(casKey).str, hint.data, GuidToString(connectionInfo.GetUid()).str);
							writer.WriteU16(0);
							return false;
						}
					}

					if (checkedCasKey != casKey)
					{
						m_logger.Error(TC("FetchBegin failed for cas file %s (%s). Server has a source file"), CasKeyString(casKey).str, hint.data);
						writer.WriteU16(0);
						return false;
					}

					if (!AddCasFile(hint.data, casKey, false))
					{
						m_logger.Error(TC("FetchBegin failed for cas file %s (%s). Can't add cas file to database"), CasKeyString(casKey).str, hint.data);
						writer.WriteU16(0);
						return true;
					}
					SCOPED_WRITE_LOCK(m_casLookupLock, lookupLock);
					auto findIt = m_casLookup.find(casKey);
					UBA_ASSERT(findIt != m_casLookup.end());
					casEntry = &findIt->second;
				}

				if (casEntry->disallowed)
				{
					writer.WriteU16(0);
					m_logger.Error(TC("Client is asking for cas content of file that is not allowed to be transferred. (%s)"), hint.data);
					return true;
				}
				
				StringBuffer<512> casFile;
				FileHandle readFileHandle = InvalidFileHandle;
				auto rfg = MakeGuard([&]() { CloseFile(nullptr, readFileHandle); });
				u64 fileSize;
				u8* memoryBegin = nullptr;
				u8* memoryPos = nullptr;
				bool ownsMapping = false;

				MappedView mappedView;
				auto mvg = MakeGuard([&](){ m_casDataBuffer.UnmapView(mappedView, TC("FetchBegin")); });

				bool useFileMapping = casEntry->mappingHandle.IsValid();
				if (useFileMapping)
				{
					mappedView = m_casDataBuffer.MapView(casEntry->mappingHandle, casEntry->mappingOffset, casEntry->mappingSize, CasKeyString(casKey).str);
					memoryBegin = mappedView.memory;
					fileSize = casEntry->mappingSize;
					if (!memoryBegin)
						return m_logger.Error(TC("Failed to map memory map for %s. Will use file handle instead (%s)"), CasKeyString(casKey).str, LastErrorToText().data);
					memoryPos = memoryBegin;
				}
				else
				{
	#if !UBA_USE_SPARSEFILE
					GetCasFileName(casFile, casKey);
					if (!OpenFileSequentialRead(m_logger, casFile.data, readFileHandle))
					{
						writer.WriteU16(0);
						return true;
					}

					if (!uba::GetFileSizeEx(fileSize, readFileHandle))
						return m_logger.Error(TC("GetFileSizeEx failed on file %s (%s)"), casFile.data, LastErrorToText().data);
	#else
					UBA_ASSERT(false);
	#endif
					if (fileSize > BufferSlotSize)
					{
						mappedView.handle = CreateFileMappingW(readFileHandle, PAGE_READONLY, fileSize, TC(""));
						if (!mappedView.handle.IsValid())
							return m_logger.Error(TC("Failed to create file mapping of %s (%s)"), casFile.data, LastErrorToText().data);
						u64 offset = memoryPos - memoryBegin;
						mappedView.memory = MapViewOfFile(mappedView.handle, FILE_MAP_READ, 0, fileSize);
						if (!mappedView.memory)
							return m_logger.Error(TC("Failed to map memory of %s (%s)"), casFile.data, LastErrorToText().data);
						memoryBegin = mappedView.memory;
						memoryPos = memoryBegin + offset;
						ownsMapping = true;
						useFileMapping = true;
					}
				}

				if (m_trace)
					m_trace->FileBeginFetch(connectionInfo.GetId(), casKey, fileSize, hint.data, m_traceFetch);

				auto cg = MakeGuard([&](){ if (m_traceFetch) m_trace->FileEndFetch(connectionInfo.GetId(), casKey); });

				u64 left = fileSize;

				u16* fetchId = (u16*)writer.AllocWrite(sizeof(u16));
				*fetchId = 0;
				writer.Write7BitEncoded(fileSize);
				u8 flags = 0;
				flags |= u8(m_storeCompressed) << 0;
				flags |= u8(m_traceFetch)    << 1;
				writer.WriteByte(flags);

				u64 capacityLeft = writer.GetCapacityLeft();
				u32 toWrite = u32(Min(left, capacityLeft));
				void* writeBuffer = writer.AllocWrite(toWrite);
				if (useFileMapping)
				{
					memcpy(writeBuffer, memoryPos, toWrite);
					memoryPos += toWrite;
				}
				else if (toWrite == left)
				{
					if (!ReadFile(m_logger, casFile.data, readFileHandle, writeBuffer, toWrite))
					{
						UBA_ASSERT(false); // Implement
						return false;
					}
				}
				else
				{
					memoryBegin = PopBufferSlot();
					memoryPos = memoryBegin;
					u32 toRead = u32(Min(left, BufferSlotSize));
 					if (!ReadFile(m_logger, casFile.data, readFileHandle, memoryBegin, toRead))
					{
						UBA_ASSERT(false); // Implement
						return false;
					}
					memcpy(writeBuffer, memoryPos, toWrite);
					memoryPos += toWrite;

					CloseFile(casFile.data, readFileHandle);
					readFileHandle = InvalidFileHandle;
				}

				u64 actualSize = fileSize;
				if (m_storeCompressed)
					actualSize = *(u64*)writeBuffer;

				StorageStats& stats = Stats();
				stats.sendCasBytesComp += fileSize;
				stats.sendCasBytesRaw += actualSize;

				left -= toWrite;

				if (!left)
				{
					*fetchId = u16(~0);
					u64 sendCasTime = GetTime() - start;
					stats.sendCas.Add(Timer{sendCasTime, 1});
					return true;
				}

				mvg.Cancel();
				cg.Cancel();
				rfg.Cancel();

				*fetchId = PopId();

				SCOPED_WRITE_LOCK(m_activeFetchesLock, lock);
				auto insres = m_activeFetches.try_emplace(*fetchId);
				UBA_ASSERT(insres.second);
				ActiveFetch& fetch = insres.first->second;
				fetch.clientId = connectionInfo.GetId();
				lock.Leave();

				mappedView.size = fileSize;

				fetch.readFileHandle = readFileHandle;
				fetch.mappedView = mappedView;
				fetch.ownsMapping = ownsMapping;
				fetch.memoryBegin = memoryBegin;
				fetch.memoryPos = memoryPos;
				fetch.left = left;
				fetch.casKey = casKey;
				fetch.sendCasTime = GetTime() - start;

				return true;
			}
			case StorageMessageType_FetchSegment:
			{
				u64 start = GetTime();
				u16 fetchId = reader.ReadU16();
				u32 fetchIndex = reader.ReadU32();

				SCOPED_READ_LOCK(m_activeFetchesLock, lock);
				auto findIt = m_activeFetches.find(fetchId);
				if (findIt == m_activeFetches.end())
					return m_logger.Error(TC("Can't find active fetch %u, disconnected client? (index %u)"), fetchId, fetchIndex);
				ActiveFetch& fetch = findIt->second;
				UBA_ASSERT(fetch.clientId == connectionInfo.GetId());
				lock.Leave();

				UBA_ASSERT(fetchIndex);
				const u8* pos = fetch.memoryPos + (fetchIndex-1) * writer.GetCapacityLeft();
				u64 toWrite = writer.GetCapacityLeft();
				if ((pos - fetch.memoryBegin) + toWrite > fetch.mappedView.size)
					toWrite = fetch.mappedView.size - (pos - fetch.memoryBegin);

				memcpy(writer.AllocWrite(toWrite), pos, toWrite);

				bool isDone = fetch.left.fetch_sub(toWrite) == toWrite;
				if (!isDone)
				{
					fetch.sendCasTime += GetTime() - start;
					return true;
				}

				fetch.Release(*this, TC("FetchDone"));

				u64 sendCasTime = fetch.sendCasTime;
				SCOPED_WRITE_LOCK(m_activeFetchesLock, activeLock);
				m_activeFetches.erase(fetchId);
				activeLock.Leave();
				PushId(fetchId);

				sendCasTime += GetTime() - start;
				Stats().sendCas.Add(Timer{sendCasTime, 1});
				return true;
			}

			case StorageMessageType_FetchEnd:
			{
				CasKey key = reader.ReadCasKey();
				if (m_traceFetch)
					m_trace->FileEndFetch(connectionInfo.GetId(), AsCompressed(key, m_storeCompressed));
				return true;
			}

			case StorageMessageType_ExistsOnServer:
			{
				CasKey casKey = reader.ReadCasKey();
				UBA_ASSERT(IsCompressed(casKey));
				SCOPED_WRITE_LOCK(m_casLookupLock, lookupLock);
				auto casInsres = m_casLookup.try_emplace(casKey);
				CasEntry& casEntry = casInsres.first->second;
				lookupLock.Leave();

				SCOPED_WRITE_LOCK(casEntry.lock, entryLock);
				
				if (!WaitForWritten(casEntry, entryLock, TC("UNKNOWN")))
					return false;

				bool exists = casEntry.verified && casEntry.exists;

				if (!exists && casEntry.exists)
				{
#if !UBA_USE_SPARSEFILE
					StringBuffer<> casFile;
					if (!GetCasFileName(casFile, casKey))
						return false;
					u64 outFileSize = 0;
					if (FileExists(m_logger, casFile.data, &outFileSize))
					{
						if (outFileSize == 0 && casKey != EmptyFileKey)
						{
							m_logger.Warning(TC("Found file %s with size 0 which did not have the zero-size-caskey. Deleting"), casFile.data);
							if (!uba::DeleteFileW(casFile.data))
								return m_logger.Error(TC("Failed to delete %s. Clean cas folder and restart"), casFile.data);
							casEntry.exists = false;
						}
						else
						{
							casEntry.verified = true;
							exists = true;
							entryLock.Leave();
							CasEntryWritten(casEntry, outFileSize);
						}
					}
					else
					{
						casEntry.exists = false;
					}
					casEntry.verified = true;
#endif
				}
				writer.WriteBool(exists);
				return true;
			}

			case StorageMessageType_StoreBegin:
			{
				u64 start = GetTime();
				CasKey casKey = reader.ReadCasKey();
				u64 fileSize = reader.ReadU64();
				u64 actualSize = reader.ReadU64();
				UBA_ASSERT(IsCompressed(casKey));
				StringBuffer<> hint;
				reader.ReadString(hint);

				SCOPED_WRITE_LOCK(m_casLookupLock, lookupLock);
				auto casInsres = m_casLookup.try_emplace(casKey);
				CasEntry& casEntry = casInsres.first->second;
				lookupLock.Leave();

				SCOPED_WRITE_LOCK(casEntry.lock, entryLock);
				if (!casEntry.verified)
				{
					casEntry.key = casKey;
					casEntry.verified = true;
				}
				else
				{
					if (!WaitForWritten(casEntry, entryLock, hint.data))
						return false;

					if (casEntry.exists)
					{
						entryLock.Leave();
						CasEntryAccessed(casEntry);
						writer.WriteU16(u16(~0));
						return true;
					}
				}
				if (!fileSize)
				{
					m_logger.Error(TC("Store from client is of 0 size (%s)"), hint.data);
					casEntry.verified = false;
					return false;
				}

				auto mappedView = m_casDataBuffer.AllocAndMapView(MappedView_Transient, fileSize, 1, CasKeyString(casKey).str);
				if (!mappedView.memory)
				{
					casEntry.verified = false;
					return false;
				}

				casEntry.beingWritten = true;

				firstStore = &tempStore;

				*(u64*)mappedView.memory = fileSize;

				firstStore->casEntry  = &casEntry;
				firstStore->fileSize = fileSize;
				firstStore->actualSize = actualSize;
				firstStore->mappedView = mappedView;
				firstStore->recvCasTime = GetTime() - start;

				if (m_trace)
					m_trace->FileBeginStore(connectionInfo.GetId(), casKey, fileSize, hint.data, m_traceStore);

				[[fallthrough]];
			}
			case StorageMessageType_StoreSegment:
			{
				u64 start = GetTime();

				u16 storeId = 0;
				u64 memOffset = 0;
				ActiveStore* activeStoreTemp = firstStore;
				if (!firstStore) // If this is set we are a continuation from StorageMessageType_Begin
				{
					storeId = reader.ReadU16();
					memOffset = reader.ReadU64();
					SCOPED_READ_LOCK(m_activeStoresLock, activeLock);
					auto storeIt = m_activeStores.find(storeId);
					if (storeIt == m_activeStores.end())
						return m_logger.Error(TC("Can't find active store %u, disconnected client?"), storeId);
					activeStoreTemp = &storeIt->second;
					UBA_ASSERT(activeStoreTemp->clientId == connectionInfo.GetId());
				}
				ActiveStore& activeStore = *activeStoreTemp;

				u64 toRead = reader.GetLeft();
				reader.ReadBytes(activeStore.mappedView.memory + memOffset, toRead);
				
				u64 time2 = GetTime();
				activeStore.recvCasTime += time2 - start;

				u64 fileSize = activeStore.fileSize;
				u64 totalWritten = activeStore.totalWritten.fetch_add(toRead) + toRead;
				if (totalWritten == fileSize)
				{
					m_casDataBuffer.UnmapView(activeStore.mappedView, TC("StoreDone"));

					CasEntry& casEntry = *activeStore.casEntry;
					{
						SCOPED_WRITE_LOCK(casEntry.lock, entryLock);
						casEntry.mappingHandle = activeStore.mappedView.handle;
						casEntry.mappingOffset = activeStore.mappedView.offset;
						casEntry.mappingSize = totalWritten;
						casEntry.exists = true;
						casEntry.beingWritten = false;
					}

					bool isPersistentStore = false;
					if (isPersistentStore)
						CasEntryWritten(*activeStore.casEntry, totalWritten);

					activeStore.recvCasTime += GetTime() - time2;

					StorageStats& stats = Stats();
					stats.recvCas.Add(Timer{activeStore.recvCasTime, 1});
					stats.recvCasBytesComp += activeStore.fileSize;
					stats.recvCasBytesRaw += activeStore.actualSize;

					SCOPED_WRITE_LOCK(m_waitEntriesLock, waitLock);
					auto waitFindIt = m_waitEntries.find(casEntry.key);
					if (waitFindIt != m_waitEntries.end())
					{
						WaitEntry& waitEntry = waitFindIt->second;
						waitEntry.Success = true;
						waitEntry.Done.Set();
					}
					waitLock.Leave();

					if (!firstStore)
					{
						SCOPED_WRITE_LOCK(m_activeStoresLock, activeLock);
						m_activeStores.erase(storeId);
						activeLock.Leave();
						PushId(storeId);
					}
					else
					{
						writer.WriteU16(0);
						writer.WriteBool(m_traceStore);
					}

					return true;
				}

				if (firstStore)
				{
					storeId = PopId();
					UBA_ASSERT(storeId != 0);
					writer.WriteU16(storeId);
					writer.WriteBool(m_traceStore);

					SCOPED_WRITE_LOCK(m_activeStoresLock, activeLock);
					auto insres = m_activeStores.try_emplace(storeId);
					UBA_ASSERT(insres.second);
					ActiveStore& s = insres.first->second;
					s.clientId = connectionInfo.GetId();
					activeLock.Leave();

					s.fileSize = firstStore->fileSize;
					s.mappedView = firstStore->mappedView;
					s.casEntry = firstStore->casEntry;
					s.totalWritten = firstStore->totalWritten.load();
					s.recvCasTime = firstStore->recvCasTime.load();
					s.error = firstStore->error;
				}
				return true;
			}
			case StorageMessageType_StoreEnd:
			{
				CasKey key = reader.ReadCasKey();
				if (m_traceStore)
					m_trace->FileEndStore(connectionInfo.GetId(), key);
				return true;
			}

		}
		UBA_ASSERT(false);
		return false;
	}

	u16 StorageServer::PopId()
	{
		SCOPED_WRITE_LOCK(m_availableIdsLock, lock);
		if (m_availableIds.empty())
		{
			UBA_ASSERT(m_availableIdsHigh < u16(~0) - 1);
			return m_availableIdsHigh++;
		}
		u16 storeId = m_availableIds.back();
		m_availableIds.pop_back();
		return storeId;
	}

	void StorageServer::PushId(u16 id)
	{
		SCOPED_WRITE_LOCK(m_availableIdsLock, lock);
		m_availableIds.push_back(id);
	}

}

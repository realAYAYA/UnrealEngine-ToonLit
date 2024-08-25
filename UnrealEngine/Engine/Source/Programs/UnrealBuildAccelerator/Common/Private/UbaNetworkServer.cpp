// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaNetworkServer.h"
#include "UbaCrypto.h"
#include "UbaBinaryReaderWriter.h"
#include "UbaPlatform.h"

namespace uba
{
	struct NetworkServer::WorkerContext
	{
		WorkerContext(NetworkServer& s) : server(s), workAvailable(false)
		{
			writeMemSize = server.m_sendSize;
			writeMem = new u8[writeMemSize];
		}

		~WorkerContext()
		{
			delete[] writeMem;
		}

		NetworkServer& server;
		Event workAvailable;
		u8* writeMem = nullptr;
		u32 writeMemSize = 0;

		Vector<u8> buffer;
		Connection* connection = nullptr;
		u32 dataSize = 0;
		u8 serviceId = 0;
		u8 messageType = 0;
		u16 id = 0;
	};

	class NetworkServer::Worker
	{
	public:
		Worker() {}
		~Worker()
		{
			UBA_ASSERT(!m_inUse);
			m_context->connection = nullptr;
			m_loop = false;
			m_context->workAvailable.Set();
			m_thread.Wait();
			delete m_context;
			m_context = nullptr;
		}

		void Start(NetworkServer& server)
		{
			m_context = new WorkerContext(server);
			m_loop = true;
			m_thread.Start([&]() { ThreadWorker(server); return 0; });
		}

		void Stop(NetworkServer& server)
		{
			m_loop = false;
			SCOPED_WRITE_LOCK(server.m_availableWorkersLock, lock);
			while (m_inUse)
			{
				m_context->workAvailable.Set();
				lock.Leave();
				if (m_thread.Wait(5))
					break;
				lock.Enter();
			}
		}

		void ThreadWorker(NetworkServer& server);
		void Update(WorkerContext& context, bool signalAvailable);
		void DoAdditionalWorkAndSignalAvailable(NetworkServer& server);

		Worker* m_nextWorker = nullptr;
		Worker* m_prevWorker = nullptr;

		WorkerContext* m_context = nullptr;

		Atomic<bool> m_loop;
		Atomic<bool> m_inUse;
		Thread m_thread;

		Worker(const Worker&) = delete;
	};
	thread_local NetworkServer::Worker* t_worker;

	class NetworkServer::Connection
	{
	public:
		Connection(NetworkServer& server, NetworkBackend& backend, void* backendConnection, const sockaddr& remoteSockAddr, CryptoKey cryptoKey, u32 id)
		:	m_server(server)
		,	m_backend(backend)
		,	m_remoteSockAddr(remoteSockAddr)
		,	m_cryptoKey(cryptoKey)
		,	m_disconnectCallbackCalled(true)
		,	m_id(id)
		,	m_backendConnection(backendConnection)
		{
			m_activeWorkerCount = 1;

			m_backend.SetDisconnectCallback(m_backendConnection, this, [](void* context, const Guid& connectionUid, void* connection)
				{
					auto& conn = *(Connection*)context;
					conn.Disconnect();
					conn.m_disconnectCallbackCalled.Set();
				});

			m_backend.SetDataSentCallback(m_backendConnection, this, [](void* context, u32 bytes)
				{
					auto& conn = *(Connection*)context;
					if (auto c = conn.m_client)
						c->recvBytes += bytes;
					conn.m_server.m_sendBytes += bytes;
				});

			m_backend.SetRecvTimeout(m_backendConnection, m_server.m_receiveTimeoutMs);

			if (m_cryptoKey)
				m_backend.SetRecvCallbacks(m_backendConnection, this, 0, ReceiveHandshakeHeader, ReceiveHandshakeBody, TC("ReceiveHandshake"));
			else
				m_backend.SetRecvCallbacks(m_backendConnection, this, 4, ReceiveVersion, nullptr, TC("ReceiveVersion"));
		}

		~Connection()
		{
			Stop();
			if (m_cryptoKey)
				Crypto::DestroyKey(m_cryptoKey);
		}

		void Disconnect()
		{
			if (m_disconnectCalled.fetch_add(1) != 0)
				return;
			SetShouldDisconnect();
			if (--m_activeWorkerCount == 0) // Will disconnect in send if there are active workers
				TestDisconnect();
		}

		bool Stop()
		{
			Disconnect();

			u64 startTimer = GetTime();
			while (m_activeWorkerCount)
			{
				if (TimeToMs(GetTime() - startTimer) > 3000)
				{
					m_server.m_logger.Error(TC("Connection has waited 3 seconds to stop... something is stuck (Active worker count: %u). Need someone to attach a debugger to process to get callstacks"), m_activeWorkerCount.load());
					return false;
				}
				Sleep(1);
			}

			if (!m_disconnectCallbackCalled.IsSet(60000)) // This should never time out!
				m_server.m_logger.Warning(TC("This should never happen!! Unknown consequences"));
			return true;
		}

		bool SendInitialResponse(u8 value)
		{
			u8 data[32];
			*data = value;
			*(Guid*)(data+1) = m_server.m_uid;
			NetworkBackend::SendContext context(NetworkBackend::SendFlags_Async);
			return m_backend.Send(m_server.m_logger, m_backendConnection, data, 1 + sizeof(Guid), context);
		}

		static bool ReceiveHandshakeHeader(void* context, const Guid& connectionUid, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize)
		{
			u8* handshakeData = new u8[sizeof(EncryptionHandshakeString)];
			outBodyData = handshakeData;
			outBodySize = sizeof(EncryptionHandshakeString);
			return true;
		}

		static bool ReceiveHandshakeBody(void* context, bool recvError, u8* headerData, void* bodyContext, u8* bodyData, u32 bodySize)
		{
			auto& conn = *(Connection*)context;
			u8* handshakeData = bodyData;
			auto g = MakeGuard([handshakeData]() { delete[] handshakeData; });

			if (!Crypto::Decrypt(conn.m_server.m_logger, conn.m_cryptoKey, handshakeData, sizeof(EncryptionHandshakeString)))
				return false;

			if (memcmp(handshakeData, EncryptionHandshakeString, sizeof(EncryptionHandshakeString)) != 0)
				return conn.m_server.m_logger.Error(TC("Crypto mismatch..."));

			conn.m_backend.SetRecvCallbacks(conn.m_backendConnection, &conn, 4, ReceiveVersion, nullptr, TC("ReceiveVersion"));

			return true;
		}

		static bool ReceiveVersion(void* context, const Guid& connectionUid, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize)
		{
			auto& conn = *(Connection*)context;
			u32 clientVersion = *(u32*)headerData;
			if (clientVersion != SystemNetworkVersion)
			{
				conn.SendInitialResponse(1);
				return false;
			}

			conn.m_backend.SetRecvCallbacks(conn.m_backendConnection, &conn, sizeof(Guid), ReceiveClientUid, nullptr, TC("ReceiveClientUid"));

			return true;
		}

		static bool ReceiveClientUid(void* context, const Guid& connectionUid, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize)
		{
			auto& conn = *(Connection*)context;
			auto& server = conn.m_server;

			Guid clientUid = *(Guid*)headerData;

			if (!server.m_allowNewClients)
			{
				SCOPED_READ_LOCK(server.m_clientsLock, clientsLock);
				bool found = false;
				for (auto& kv : server.m_clients)
					found |= kv.second.uid == clientUid;
				if (!found)
				{
					conn.SendInitialResponse(3);
					return false;
				}
			}

			constexpr u32 HeaderSize = 6;
			conn.m_backend.SetRecvCallbacks(conn.m_backendConnection, &conn, HeaderSize, ReceiveMessageHeader, ReceiveMessageBody, TC("ReceiveMessage"));

			if (!conn.SendInitialResponse(0))
				return false;

			SCOPED_WRITE_LOCK(conn.m_shutdownLock, shutdownLock);

			SCOPED_WRITE_LOCK(server.m_clientsLock, clientsLock);
			u32 clientId = u32(server.m_clients.size() + 1);
			for (auto& kv : server.m_clients)
				if (kv.second.uid == clientUid)
					clientId = kv.second.id;
			Client& client = server.m_clients.try_emplace(clientId, clientUid, clientId).first->second;
			clientsLock.Leave();

			conn.m_client = &client;

			if (client.connectionCount.fetch_add(1) == 0)
			{
				if (server.m_onConnectionFunction)
					server.m_onConnectionFunction(clientUid, clientId);
				server.m_logger.Detail(TC("Client %s connected on connection %s"), GuidToString(clientUid).str, GuidToString(connectionUid).str);
			}
			else
				server.m_logger.Detail(TC("Client %s additional connection %s connected"), GuidToString(clientUid).str, GuidToString(connectionUid).str);


			return true;
		}

		static bool ReceiveMessageHeader(void* context, const Guid& connectionUid, u8* headerData, void*& outBodyContext, u8*& outBodyData, u32& outBodySize)
		{
			auto& conn = *(Connection*)context;

			u8 serviceIdAndMessageType = headerData[0];
			u8 serviceId = serviceIdAndMessageType >> 6;
			u8 messageType = serviceIdAndMessageType & 0b111111;
			u16 messageId = u16(headerData[1] << 8) | u16((*(u32*)(headerData + 2) & 0xff000000) >> 24);
			u32 messageSize = *(u32*)(headerData + 2) & 0x00ffffff;
			UBA_ASSERTF(messageSize <= SendMaxSize, TC("Got message size %u which is larger than max %u. Protocol error?"), messageSize, SendMaxSize);
			UBA_ASSERTF(serviceId < sizeof(NetworkServer::m_workerFunctions), TC("Got message with service id %u which is out of range. Protocol error?"), serviceId);

			//m_logger.Debug(TC("Recv: %u, %u, %u, %u"), serviceId, messageType, id, size);
			Worker* worker = conn.m_server.PopWorker();
			if (!worker)
				return false;
			auto& wc = *worker->m_context;
			wc.id = messageId;
			wc.serviceId = serviceId;
			wc.messageType = messageType;
			wc.dataSize = messageSize;
			wc.connection = &conn;// this;
			if (wc.buffer.size() < messageSize)
				wc.buffer.resize(size_t(Min(messageSize + 1024u, SendMaxSize)));
			outBodyContext = worker;
			outBodyData = wc.buffer.data();
			outBodySize = messageSize;
			return true;
		}

		static bool ReceiveMessageBody(void* context, bool recvError, u8* headerData, void* bodyContext, u8* bodyData, u32 bodySize)
		{
			auto& conn = *(Connection*)context;
			auto worker = (Worker*)bodyContext;

			if (recvError)
			{
				conn.m_server.PushWorker(worker);
				return false;
			}
			auto& wc = *worker->m_context;

			conn.m_client->sendBytes += wc.dataSize;
			conn.m_server.m_recvBytes += wc.dataSize;
			++conn.m_server.m_recvCount;

			++conn.m_activeWorkerCount;
			wc.workAvailable.Set();
			return true;
		}

		void Send(const void* data, u32 bytes)
		{
			TimerScope ts(m_sendTimer);
			NetworkBackend::SendContext context;
			if (!m_backend.Send(m_server.m_logger, m_backendConnection, data, bytes, context))
				SetShouldDisconnect();
		}

		bool SetShouldDisconnect()
		{
			SCOPED_WRITE_LOCK(m_shutdownLock, lock);
			bool isConnected = !m_shouldDisconnect;
			m_shouldDisconnect = true;
			return isConnected;
		}

		void Release()
		{
			if (--m_activeWorkerCount == 0)
				TestDisconnect();
		}

		void TestDisconnect()
		{
			SCOPED_WRITE_LOCK(m_shutdownLock, lock);
			if (!m_shouldDisconnect)
				return;
			if (m_disconnected)
				return;
			lock.Leave();
			m_backend.Shutdown(m_backendConnection);
			if (m_client && m_client->connectionCount.fetch_sub(1) == 1)
			{
				SCOPED_READ_LOCK(m_server.m_onDisconnectFunctionsLock, l);
				for (auto& entry : m_server.m_onDisconnectFunctions)
					entry.function(m_client->uid, m_client->id);
				m_server.m_logger.Detail(TC("Client %s disconnected"), GuidToString(m_client->uid).str);
			}
			m_disconnected = true;
		}

		NetworkServer& m_server;
		NetworkBackend& m_backend;
		ReaderWriterLock m_shutdownLock;
		Client* m_client = nullptr;
		sockaddr m_remoteSockAddr;
		CryptoKey m_cryptoKey;
		Event m_disconnectCallbackCalled;
		Atomic<int> m_activeWorkerCount;
		Atomic<int> m_disconnectCalled;
		Atomic<bool> m_disconnected;
		u32 m_id = 0;
		bool m_shouldDisconnect = false;
		void* m_backendConnection = nullptr;

		Timer m_sendTimer;
		Timer m_encryptTimer;
		Timer m_decryptTimer;

		Connection(const Connection& o) = delete;
		void operator=(const Connection& o) = delete;
	};

	const Guid& ConnectionInfo::GetUid() const
	{
		return ((NetworkServer::Connection*)internalData)->m_client->uid;
	}

	u32 ConnectionInfo::GetId() const
	{
		return ((NetworkServer::Connection*)internalData)->m_client->id;
	}

	bool ConnectionInfo::GetName(StringBufferBase& out) const
	{
		#if PLATFORM_WINDOWS
		auto& remoteSockAddr = ((NetworkServer::Connection*)internalData)->m_remoteSockAddr;
		if (!InetNtopW(AF_INET, &remoteSockAddr, out.data, out.capacity))
			return false;
		out.count = u32(wcslen(out.data));
		return true;
		#else
		UBA_ASSERT(false);
		return false;
		#endif
	}

	bool ConnectionInfo::ShouldDisconnect() const
	{
		auto& conn = *(NetworkServer::Connection*)internalData;
		SCOPED_WRITE_LOCK(conn.m_shutdownLock, lock);
		return conn.m_shouldDisconnect;
	}

	void NetworkServer::Worker::Update(WorkerContext& context, bool signalAvailable)
	{
		auto& server = context.server;

		auto sg = MakeGuard([&]()
			{
				if (signalAvailable)
					DoAdditionalWorkAndSignalAvailable(server);
			});

		// This is only additional work
		if (!context.connection)
			return;
		auto& connection = *context.connection;
		context.connection = nullptr;

		CryptoKey cryptoKey = connection.m_cryptoKey;
		if (cryptoKey)
		{
			TimerScope ts(connection.m_decryptTimer);
			if (!Crypto::Decrypt(server.m_logger, cryptoKey, context.buffer.data(), context.dataSize))
			{
				connection.SetShouldDisconnect();
				connection.Release();
				return;
			}
		}

		BinaryReader reader(context.buffer.data(), 0, context.dataSize);


		constexpr u32 HeaderSize = 5; // 2 byte id, 3 bytes size
		constexpr u32 ErrorSize = 0xffffff;

		BinaryWriter writer(context.writeMem, 0, context.writeMemSize);
		u8* idAndSizePtr = writer.AllocWrite(HeaderSize);
			
		u32 size;
		WorkerRec& rec = server.m_workerFunctions[context.serviceId];

		u32 workId = server.TrackWorkStart(rec.toString(context.messageType));

		MessageInfo mi;
		mi.type = context.messageType;
		mi.connectionId = connection.m_id;
		mi.messageId = context.id;

		if (!rec.func)
		{
			server.m_logger.Error(TC("WORKER FUNCTION NOT FOUND. id: %u, serviceid: %u type: %s, client: %s"), context.id, context.serviceId, rec.toString(context.messageType), GuidToString(connection.m_client->uid).str);
			connection.SetShouldDisconnect();
			size = ErrorSize;
		}
		else if (!rec.func({&connection}, mi, reader, writer))
		{
			if (connection.SetShouldDisconnect())
				server.m_logger.Error(TC("WORKER FUNCTION FAILED. id: %u, serviceid: %u type: %s, client: %s"), context.id, context.serviceId, rec.toString(context.messageType), GuidToString(connection.m_client->uid).str);
			size = ErrorSize;
		}
		else
		{
			size = u32(writer.GetPosition());
		}

		server.TrackWorkEnd(workId);

		if (mi.messageId)
		{
			UBA_ASSERT(size < (1 << 24));
				
			u32 bodySize = u32(size - HeaderSize);
			if (cryptoKey && size != ErrorSize && bodySize)
			{
				TimerScope ts(connection.m_encryptTimer);
				u8* bodyData = writer.GetData() + HeaderSize;
				if (!Crypto::Encrypt(server.m_logger, cryptoKey, bodyData, bodySize))
				{
					connection.SetShouldDisconnect();
					size = ErrorSize;
					bodySize = u32(size - HeaderSize);
				}
			}

			idAndSizePtr[0] = context.id >> 8;
			*(u32*)(idAndSizePtr + 1) = bodySize | u32(context.id << 24);

			// This can happen for proxy servers in a valid situation
			//if (size == ErrorSize)
			//	UBA_ASSERT(false);

			connection.Send(writer.GetData(), size == ErrorSize ? HeaderSize : size);
		}
			
		connection.Release();
	}

	void NetworkServer::Worker::ThreadWorker(NetworkServer& server)
	{
		ElevateCurrentThreadPriority();

		t_worker = this;
		while (m_context->workAvailable.IsSet(~0u) && m_loop)
			Update(*m_context, true);
		t_worker = nullptr;

		if (m_inUse) // I have no idea how this can happen.. should not be possible. There is a path somewhere where it can leave while still being in use
			server.PushWorker(this);
	}

	void NetworkServer::Worker::DoAdditionalWorkAndSignalAvailable(NetworkServer& server)
	{
		while (true)
		{
			while (true)
			{
				AdditionalWork work;
				SCOPED_WRITE_LOCK(server.m_additionalWorkLock, lock);
				if (server.m_additionalWork.empty())
					break;
				work = std::move(server.m_additionalWork.front());
				server.m_additionalWork.pop_front();
				lock.Leave();

				u32 workId = server.TrackWorkStart(work.desc.c_str());

				work.func();

				server.TrackWorkEnd(workId);
			}

			// Both locks needs to be taken to verify if additional work
			// is present before making ourself available to avoid
			// a race where AddWork would not see this thread in the
			// available list after adding some work.
			SCOPED_WRITE_LOCK(server.m_availableWorkersLock, lock1);
			SCOPED_READ_LOCK(server.m_additionalWorkLock, lock2);
			// Verify there is not additional work while we hold both lock
			// and only add ourself as available if no additional work is present.
			if (!server.m_additionalWork.empty())
				continue;
			server.PushWorkerNoLock(this);
			break;
		}
	}

	const tchar* g_typeStr[] = { TC("0"), TC("1"), TC("2"), TC("3"), TC("4"), TC("5"), TC("6"), TC("7"), TC("8"), TC("9"), TC("10"), TC("11"), TC("12") };

	static const tchar* GetMessageTypeToName(u8 type)
	{
		if (type <= 12)
			return g_typeStr[type];
		return TC("NUMBER HIGHER THAN 12");
	}

	NetworkServer::NetworkServer(bool& outCtorSuccess, const NetworkServerCreateInfo& info, const tchar* name)
	:	m_logger(info.logWriter, name)
	,	m_workerAvailable(true)
	{
		outCtorSuccess = true;

		u32 workerCount;
		if (info.workerCount == 0)
			workerCount = GetLogicalProcessorCount();
		else
			workerCount = Min(Max(info.workerCount, (u32)(1u)), (u32)(1024u));
		m_maxWorkerCount = workerCount;

		#if UBA_DEBUG
		m_logger.Info(TC("Created in DEBUG"));
		#endif

		u32 fixedSendSize = Max(info.sendSize, (u32)(4*1024));
		fixedSendSize = Min(fixedSendSize, (u32)(SendMaxSize));
		if (info.sendSize != fixedSendSize)
			m_logger.Detail(TC("Adjusted msg size to %u to stay inside limits"), fixedSendSize);
		m_sendSize = fixedSendSize;
		m_receiveTimeoutMs = info.receiveTimeoutSeconds * 1000;

		m_workerFunctions[SystemServiceId].toString = GetMessageTypeToName;
		m_workerFunctions[SystemServiceId].func = [this](const ConnectionInfo& connectionInfo, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
			{
				return HandleSystemMessage(connectionInfo, messageInfo.type, reader, writer);
			};

		if (!CreateGuid(m_uid))
			outCtorSuccess = false;
	}

	NetworkServer::~NetworkServer()
	{
		UBA_ASSERT(m_connections.empty());
		FlushWorkers();
	}

	bool NetworkServer::StartListen(NetworkBackend& backend, u16 port, const tchar* ip, const u8* cryptoKey128)
	{
		if (cryptoKey128)
		{
			m_listenCrypto = Crypto::CreateKey(m_logger, cryptoKey128);
			if (!m_listenCrypto)
				return false;
		}

		return backend.StartListen(m_logger, port, ip, [&](void* connection, const sockaddr& remoteSockAddr)
			{
				CryptoKey cryptoKey = InvalidCryptoKey;
				if (m_listenCrypto)
				{
					cryptoKey = Crypto::DuplicateKey(m_logger, m_listenCrypto);
					if (!cryptoKey)
						return false;
				}
				return AddConnection(backend, connection, remoteSockAddr, cryptoKey);
			});
	}
	
	void NetworkServer::DisallowNewClients()
	{
		m_allowNewClients = false;
	}

	void NetworkServer::DisconnectClients()
	{
		{
			SCOPED_WRITE_LOCK(m_availableWorkersLock, lock);
			m_workersEnabled = false;
			m_workerAvailable.Set();
		}
		{
			SCOPED_WRITE_LOCK(m_addConnectionsLock, lock);
			m_addConnections.clear();
		}

		{
			SCOPED_WRITE_LOCK(m_connectionsLock, lock);
			bool success = true;
			for (auto& c : m_connections)
			{
				success = c.Stop() && success;
				m_sendTimer.Add(c.m_sendTimer);
				m_encryptTimer.Add(c.m_encryptTimer);
				m_decryptTimer.Add(c.m_decryptTimer);
			}
			lock.Leave();

			// If stopping connections fail we need to abort because we will most likely run into a deadlock when deleting the workers.
			if (!success)
			{
				m_logger.Info(TC("Failed to stop connection(s) in a graceful way. Will abort process"));
				abort(); // TODO: Does this produce core dump on windows?
			}
		}

		FlushWorkers();

		SCOPED_WRITE_LOCK(m_connectionsLock, lock);
		m_connections.clear();
	}

	bool NetworkServer::AddClient(NetworkBackend& backend, const tchar* ip, u16 port, const u8* cryptoKey128)
	{
		SCOPED_WRITE_LOCK(m_addConnectionsLock, lock);
		if (!m_workersEnabled)
			return false;

		for (auto it = m_addConnections.begin(); it != m_addConnections.end();)
		{
			if (it->Wait(0))
				it = m_addConnections.erase(it);
			else
				++it;
		}

		CryptoKey cryptoKey = InvalidCryptoKey;
		if (cryptoKey128)
		{
			cryptoKey = Crypto::CreateKey(m_logger, cryptoKey128);
			if (cryptoKey == InvalidCryptoKey)
				return false;
		}

		Event done(true);
		bool success = false;

		m_addConnections.emplace_back([this, &success , &done, &backend, ip2 = TString(ip), port, cryptoKey]()
			{
				// TODO: Should this retry?
				success = backend.Connect(m_logger, ip2.c_str(), [this, &backend, cryptoKey](void* connection, const sockaddr& remoteSocketAddr, bool* timedOut)
					{
						return AddConnection(backend, connection, remoteSocketAddr, cryptoKey);
					}, port, nullptr);
				if (!success)
					Crypto::DestroyKey(cryptoKey);
				done.Set();
				return 0;
			});

		done.IsSet();
		return success;
	}

	void NetworkServer::PrintSummary(Logger& logger)
	{
		if (!m_maxActiveConnections)
			return;

		StringBuffer<> workers;
		workers.Appendf(TC("%u/%u"), m_createdWorkerCount, m_maxWorkerCount);

		logger.Info(TC("  ----- Uba server stats summary ------"));
		logger.Info(TC("  MaxActiveConnections           %6u"), m_maxActiveConnections);
		logger.Info(TC("  SendTotal          %8u %9s"), m_sendTimer.count.load(), TimeToText(m_sendTimer.time).str);
		logger.Info(TC("     Bytes                    %9s"), BytesToText(m_sendBytes).str);
		logger.Info(TC("  RecvTotal          %8u %9s"), m_recvCount.load(), BytesToText(m_recvBytes.load()).str);
		if (m_encryptTimer.count || m_decryptTimer.count)
		{
			logger.Info(TC("  EncryptTotal       %8u %9s"), m_encryptTimer.count.load(), TimeToText(m_encryptTimer.time).str);
			logger.Info(TC("  DecryptTotal       %8u %9s"), m_decryptTimer.count.load(), TimeToText(m_decryptTimer.time).str);
		}
		logger.Info(TC("  WorkerCount                 %9s"), workers.data);
		logger.Info(TC("  SendSize Set/Max  %9s %9s"), BytesToText(m_sendSize).str, BytesToText(SendMaxSize).str);
		logger.Info(TC(""));
	}

	void NetworkServer::RegisterService(u8 serviceId, const WorkerFunction& function, TypeToNameFunction* typeToNameFunc)
	{
		UBA_ASSERTF(serviceId != 0, TC("ServiceId 0 is reserved by system"));
		WorkerRec& rec = m_workerFunctions[serviceId];
		UBA_ASSERT(!rec.func);
		rec.func = function;
		rec.toString = typeToNameFunc;
		if (!typeToNameFunc)
			rec.toString = GetMessageTypeToName;
	}

	void NetworkServer::UnregisterService(u8 serviceId)
	{
		SCOPED_WRITE_LOCK(m_connectionsLock, lock);
		UBA_ASSERTF(m_connections.empty(), TC("Unregistering service while still having live connections"));
		WorkerRec& rec = m_workerFunctions[serviceId];
		rec.func = {};
		//rec.toString = nullptr; // Keep this for now, we want to be able to output stats
	}

	void NetworkServer::RegisterOnClientConnected(u8 id, const OnConnectionFunction& func)
	{
		UBA_ASSERT(!m_onConnectionFunction);
		m_onConnectionFunction = func;
	}

	void NetworkServer::UnregisterOnClientConnected(u8 id)
	{
		SCOPED_WRITE_LOCK(m_connectionsLock, lock);
		UBA_ASSERT(m_connections.empty());
		m_onConnectionFunction = {};
	}

	void NetworkServer::RegisterOnClientDisconnected(u8 id, const OnDisconnectFunction& func)
	{
		SCOPED_WRITE_LOCK(m_onDisconnectFunctionsLock, l);
		m_onDisconnectFunctions.emplace_back(OnDisconnectEntry{id, func});
	}

	void NetworkServer::UnregisterOnClientDisconnected(u8 id)
	{
		SCOPED_WRITE_LOCK(m_onDisconnectFunctionsLock, l);
		for (auto it = m_onDisconnectFunctions.begin(); it != m_onDisconnectFunctions.end(); ++it)
		{
			if (it->id != id)
				continue;
			m_onDisconnectFunctions.erase(it);
			return;
		}
	}

	void NetworkServer::AddWork(const Function<void()>& work, u32 count, const tchar* desc)
	{
		SCOPED_WRITE_LOCK(m_additionalWorkLock, lock);
		for (u32 i = 0; i != count; ++i)
		{
			m_additionalWork.push_back({ work });
			if (m_workTracker)
				m_additionalWork.back().desc = desc;
		}
		lock.Leave();

		SCOPED_WRITE_LOCK(m_availableWorkersLock, lock2);
		if (!m_workersEnabled)
			return;
		while (count--)
		{
			Worker* worker = PopWorkerNoLock();
			if (!worker)
				break;
			UBA_ASSERT(worker->m_inUse);
			worker->m_context->connection = nullptr;
			worker->m_context->workAvailable.Set();
		}
	}

	u32 NetworkServer::GetWorkerCount()
	{
		return m_maxWorkerCount;
	}

	u64 NetworkServer::GetTotalSentBytes()
	{
		return m_sendBytes;
	}

	u64 NetworkServer::GetTotalRecvBytes()
	{
		return m_recvBytes;
	}

	u32 NetworkServer::GetConnectionCount()
	{
		SCOPED_READ_LOCK(m_connectionsLock, lock);
		u32 count = 0;
		for (auto& con : m_connections)
			if (!con.m_disconnected)
				++count;
		return count;
	}

	void NetworkServer::GetClientStats(ClientStats& out, u32 clientId)
	{
		SCOPED_READ_LOCK(m_clientsLock, lock);
		auto findIt = m_clients.find(clientId);
		if (findIt == m_clients.end())
			return;
		Client& c = findIt->second;
		out.send += c.sendBytes;
		out.recv += c.recvBytes;
		out.connectionCount += c.connectionCount;
	}

	bool NetworkServer::DoAdditionalWork()
	{
		AdditionalWork work;
		SCOPED_WRITE_LOCK(m_additionalWorkLock, lock);
		if (m_additionalWork.empty())
		{
			lock.Leave();

			SCOPED_WRITE_LOCK(m_availableWorkersLock, lock2);
			if (m_createdWorkerCount != m_maxWorkerCount)
				return false;
			lock2.Leave();
			auto worker = t_worker;
			UBA_ASSERT(worker);
			auto oldContext = worker->m_context;
			WorkerContext context(*this);
			worker->m_context = &context;

			PushWorker(worker);
			bool workAvail = context.workAvailable.IsSet(10);
			lock2.Enter();
			if (worker->m_inUse)
			{
				lock2.Leave();
				if (!workAvail)
					context.workAvailable.IsSet(~0u);
				worker->Update(context, false);
				UBA_ASSERT(worker->m_inUse);
			}
			else
			{
				// Take worker back from free list
				if (m_firstAvailableWorker == worker)
					m_firstAvailableWorker = worker->m_nextWorker;
				else
					worker->m_prevWorker->m_nextWorker = worker->m_nextWorker;
				if (worker->m_nextWorker)
					worker->m_nextWorker->m_prevWorker = worker->m_prevWorker;
				worker->m_prevWorker = nullptr;
				worker->m_nextWorker = m_firstActiveWorker;
				if (m_firstActiveWorker)
					m_firstActiveWorker->m_prevWorker = worker;
				m_firstActiveWorker = worker;
				worker->m_inUse = true;
			}
			
			worker->m_context = oldContext;
			return true;
		}
		work = std::move(m_additionalWork.front());
		m_additionalWork.pop_front();
		lock.Leave();

		TrackWorkScope tws(*this, work.desc.c_str());

		work.func();

		return true;
	}

	bool NetworkServer::SendResponse(const MessageInfo& info, const u8* body, u32 bodySize)
	{
		UBA_ASSERT(info.connectionId);
		UBA_ASSERT(info.messageId);

		SCOPED_READ_LOCK(m_connectionsLock, lock);
		Connection* found = nullptr;
		for (auto& it : m_connections)
		{
			if (it.m_id != info.connectionId)
				continue;
			found = &it;
			break;
		}
		if (!found)
			return false;
		Connection& connection = *found;

		u8 buffer[SendMaxSize];

		constexpr u32 HeaderSize = 5; // 2 byte id, 3 bytes size

		BinaryWriter writer(buffer, 0, sizeof_array(buffer));
		u8* idAndSizePtr = writer.AllocWrite(HeaderSize);

		if (body)
		{
			writer.WriteBytes(body, bodySize);

			if (connection.m_cryptoKey && bodySize)
			{
				TimerScope ts(connection.m_encryptTimer);
				u8* bodyData = writer.GetData() + HeaderSize;
				if (!Crypto::Encrypt(m_logger, connection.m_cryptoKey, bodyData, bodySize))
				{
					connection.SetShouldDisconnect();
					return false;
				}
			}
		}
		else
		{
			constexpr u32 ErrorSize = 0xffffff;
			bodySize = ErrorSize;
			connection.SetShouldDisconnect();
		}

		idAndSizePtr[0] = info.messageId >> 8;
		*(u32*)(idAndSizePtr + 1) = bodySize | u32(info.messageId << 24);

		connection.Send(writer.GetData(), u32(writer.GetPosition()));
		return true;
	}


	NetworkServer::Worker* NetworkServer::PopWorker()
	{
		while (true)
		{
			SCOPED_WRITE_LOCK(m_availableWorkersLock, lock);
			if (!m_workersEnabled)
				return nullptr;
			if (auto worker = PopWorkerNoLock())
				return worker;
			m_workerAvailable.Reset();
			lock.Leave();
			m_workerAvailable.IsSet();
		}
	}

	NetworkServer::Worker* NetworkServer::PopWorkerNoLock()
	{
		Worker* worker = m_firstAvailableWorker;
		if (worker)
		{
			m_firstAvailableWorker = worker->m_nextWorker;
			if (m_firstAvailableWorker)
				m_firstAvailableWorker->m_prevWorker = nullptr;
		}
		else
		{
			if (m_createdWorkerCount == m_maxWorkerCount)
				return nullptr;

			worker = new Worker();
			worker->Start(*this);
			++m_createdWorkerCount;
		}

		if (m_firstActiveWorker)
			m_firstActiveWorker->m_prevWorker = worker;
		worker->m_nextWorker = m_firstActiveWorker;
		m_firstActiveWorker = worker;
		worker->m_inUse = true;

		return worker;
	}

	void NetworkServer::PushWorker(Worker* worker)
	{
		SCOPED_WRITE_LOCK(m_availableWorkersLock, lock);
		PushWorkerNoLock(worker);
	}

	void NetworkServer::PushWorkerNoLock(Worker* worker)
	{
		UBA_ASSERT(worker->m_inUse);

		if (worker->m_prevWorker)
			worker->m_prevWorker->m_nextWorker = worker->m_nextWorker;
		else
			m_firstActiveWorker = worker->m_nextWorker;
		if (worker->m_nextWorker)
			worker->m_nextWorker->m_prevWorker = worker->m_prevWorker;

		if (m_firstAvailableWorker)
			m_firstAvailableWorker->m_prevWorker = worker;
		worker->m_prevWorker = nullptr;
		worker->m_nextWorker = m_firstAvailableWorker;
		worker->m_inUse = false;
		m_firstAvailableWorker = worker;
		m_workerAvailable.Set();
	}

	void NetworkServer::FlushWorkers()
	{
		SCOPED_WRITE_LOCK(m_availableWorkersLock, lock);
		while (auto worker = m_firstActiveWorker)
		{
			lock.Leave();
			worker->Stop(*this);
			lock.Enter();
		}

		auto worker = m_firstAvailableWorker;
		while (worker)
		{
			auto temp = worker;
			worker = worker->m_nextWorker;
			delete temp;
		}
		m_firstAvailableWorker = nullptr;
	}

	void NetworkServer::RemoveDisconnectedConnections()
	{
		for (auto it=m_connections.begin(); it!=m_connections.end();)
		{
			Connection& con = *it;
			if (!con.m_disconnected)
			{
				++it;
				continue;
			}
			m_sendTimer.Add(con.m_sendTimer);
			it = m_connections.erase(it);
		}
	}

	bool NetworkServer::HandleSystemMessage(const ConnectionInfo& connectionInfo, u8 messageType, BinaryReader& reader, BinaryWriter& writer)
	{
		switch (messageType)
		{
			case SystemMessageType_SetConnectionCount:
			{
				u32 connectionCount = reader.ReadU32();

				SCOPED_READ_LOCK(m_clientsLock, lock);
				auto findIt = m_clients.find(connectionInfo.GetId());
				if (findIt == m_clients.end())
					return true;
				Client& c = findIt->second;
				lock.Leave();

				if (c.connectionCount >= connectionCount)
					return true;
				u32 toAdd = connectionCount - c.connectionCount;

				auto connPtr = (NetworkServer::Connection*)connectionInfo.internalData;
				SCOPED_WRITE_LOCK(m_addConnectionsLock, lock2);
				for (u32 i = 0; i != toAdd; ++i)
				{
					m_addConnections.emplace_back([this, connPtr]()
						{
							auto& conn = *connPtr;
							conn.m_backend.Connect(m_logger, conn.m_remoteSockAddr, [this, &conn](void* connection, const sockaddr& remoteSocketAddr, bool* timedOut)
								{
									CryptoKey cryptoKey = InvalidCryptoKey;
									if (conn.m_cryptoKey)
									{
										cryptoKey = Crypto::DuplicateKey(m_logger, conn.m_cryptoKey);
										if (cryptoKey == InvalidCryptoKey)
											return false;
									}
									return AddConnection(conn.m_backend, connection, remoteSocketAddr, cryptoKey);
								}, nullptr);
							return 0;
						});
				}
				return true;
			}
			case SystemMessageType_KeepAlive:
			{
				// No-op
				return true;
			}
		}
		return false;
	}

	bool NetworkServer::AddConnection(NetworkBackend& backend, void* backendConnection, const sockaddr& remoteSocketAddr, CryptoKey cryptoKey)
	{
		SCOPED_WRITE_LOCK(m_connectionsLock, lock);

		RemoveDisconnectedConnections();

		if (!m_workersEnabled)
		{
			// Just to prevent errors in log
			backend.SetDisconnectCallback(backendConnection, nullptr, [](void*, const Guid&, void*) {});
			backend.SetRecvCallbacks(backendConnection, nullptr, 0, [](void*, const Guid&, u8*, void*&, u8*&, u32&) { return false; }, nullptr, TC("Disconnecting"));
			return false;
		}

		m_connections.emplace_back(*this, backend, backendConnection, remoteSocketAddr, cryptoKey, m_connectionIdCounter++);
		m_maxActiveConnections = Max(m_maxActiveConnections, u32(m_connections.size()));
		return true;
	}
}

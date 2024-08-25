// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaNetworkBackendMemory.h"
#include "UbaEvent.h"
#include "UbaHash.h"
#include "UbaPlatform.h"
#include "UbaStringBuffer.h"
#include "UbaTimer.h"

namespace uba
{
	struct NetworkBackendMemory::Connection
	{
		struct Peer
		{
			RecvHeaderCallback* headerCallback = nullptr;
			RecvBodyCallback* bodyCallback = nullptr;
			void* context = nullptr;
			u32 headerSize = 0;
			const tchar* recvHint = nullptr;
			
			DisconnectCallback* disconnectCallback = nullptr;
			void* disconnectContext = nullptr;
		};

		Peer peer[2];
		Guid uid;
		Atomic<bool> connected;
	};

	NetworkBackendMemory::NetworkBackendMemory(LogWriter& writer, const tchar* prefix)
	{
	}

	NetworkBackendMemory::~NetworkBackendMemory()
	{
		delete m_connection;
	}

	void NetworkBackendMemory::Shutdown(void* connection)
	{
		u64 from = ((uintptr_t)connection)-1;
		u64 to = from == 0 ? 1 : 0;
		auto& rc = m_connection->peer[to];

		m_connection->connected = false;

		if (auto cb = rc.disconnectCallback)
			cb(rc.disconnectContext, m_connection->uid, connection);
	}

	bool NetworkBackendMemory::Send(Logger& logger, void* connection, const void* data, u32 dataSize, SendContext& sendContext)
	{
		UBA_ASSERT(m_connection);
		sendContext.isUsed = true;
		sendContext.isFinished = true;

		u64 from = ((uintptr_t)connection)-1;
		u64 to = from == 0 ? 1 : 0;

		auto& peer = m_connection->peer[to];

		if (!m_connection->connected)
			return false;

		void* bodyContext = nullptr;
		u8* bodyData = nullptr;
		u32 bodySize = 0;

		if (!peer.headerCallback(peer.context, m_connection->uid, (u8*)data, bodyContext, bodyData, bodySize))
			return false;

		if (!bodySize)
			return true;

		memcpy(bodyData, ((const u8*)data) + peer.headerSize, bodySize);
		auto bc = peer.bodyCallback;
		if (!bc)
			return logger.Error(TC("Connection body callback not set"));

		if (!bc(peer.context, false, (u8*)data, bodyContext, bodyData, bodySize))
			return false;
		return true;
	}

	void NetworkBackendMemory::SetDataSentCallback(void* connection, void* context, DataSentCallback* callback)
	{
	}

	void NetworkBackendMemory::SetRecvCallbacks(void* connection, void* context, u32 headerSize, RecvHeaderCallback* h, RecvBodyCallback* b, const tchar* recvHint)
	{
		UBA_ASSERT(m_connection);
		auto& peer = m_connection->peer[((uintptr_t)connection)-1];
		peer.headerCallback = h;
		peer.bodyCallback = b;
		peer.context = context;
		peer.headerSize = headerSize;
		peer.recvHint = recvHint;
	}

	void NetworkBackendMemory::SetRecvTimeout(void* connection, u32 timeoutMs)
	{
	}

	void NetworkBackendMemory::SetDisconnectCallback(void* connection, void* context, DisconnectCallback* callback)
	{
		UBA_ASSERT(m_connection);
		auto& peer = m_connection->peer[((uintptr_t)connection)-1];
		peer.disconnectCallback = callback;
		peer.disconnectContext = context;
	}

	bool NetworkBackendMemory::StartListen(Logger& logger, u16 port, const tchar* ip, const ListenConnectedFunc& connectedFunc)
	{
		SCOPED_WRITE_LOCK(m_connectedFuncLock, l);
		m_connectedFunc = connectedFunc;
		return true;
	}

	void NetworkBackendMemory::StopListen()
	{
	}

	bool NetworkBackendMemory::Connect(Logger& logger, const tchar* ip, const ConnectedFunc& connectedFunc, u16 port, bool* timedOut)
	{
		u64 start = GetTime();
		SCOPED_READ_LOCK(m_connectedFuncLock, l);
		while (!m_connectedFunc)
		{
			if (TimeToMs(GetTime() - start) > 1000*2)
				return false;
			l.Leave();
			Sleep(10);
			l.Enter();
		}
		UBA_ASSERT(!m_connection);
		m_connection = new Connection();
		m_connection->connected = true;
		if (!m_connectedFunc((void*)1, {}))
			return false;
		l.Leave();
		return connectedFunc((void*)2, {}, timedOut);
	}

	bool NetworkBackendMemory::Connect(Logger& logger, const sockaddr& remoteSocketAddr, const ConnectedFunc& connectedFunc, bool* timedOut, const tchar* nameHint)
	{
		return false;
	}

	void NetworkBackendMemory::GetTotalSendAndRecv(u64& outSend, u64& outRecv)
	{
		outSend = 0;
		outRecv = 0;
	}
}
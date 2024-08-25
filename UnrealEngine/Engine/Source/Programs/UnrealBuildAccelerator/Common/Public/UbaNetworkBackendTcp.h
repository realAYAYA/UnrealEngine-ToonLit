// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaLogger.h"
#include "UbaNetworkBackend.h"
#include "UbaThread.h"

namespace uba
{
	class NetworkBackendTcp : public NetworkBackend
	{
	public:
		NetworkBackendTcp(LogWriter& writer, const tchar* prefix = TC("NetworkBackendTcp"));
		virtual ~NetworkBackendTcp();
		virtual void Shutdown(void* connection) override;
		virtual bool Send(Logger& logger, void* connection, const void* data, u32 dataSize, SendContext& sendContext) override;
		virtual void SetDataSentCallback(void* connection, void* context, DataSentCallback* callback) override;
		virtual void SetRecvCallbacks(void* connection, void* context, u32 headerSize, RecvHeaderCallback* h, RecvBodyCallback* b, const tchar* recvHint) override;
		virtual void SetRecvTimeout(void* connection, u32 timeoutMs) override;
		virtual void SetDisconnectCallback(void* connection, void* context, DisconnectCallback* callback) override;

		virtual bool StartListen(Logger& logger, u16 port, const tchar* ip, const ListenConnectedFunc& connectedFunc) override;
		virtual void StopListen() override;
		virtual bool Connect(Logger& logger, const tchar* ip, const ConnectedFunc& connectedFunc, u16 port = DefaultPort, bool* timedOut = nullptr) override;
		virtual bool Connect(Logger& logger, const sockaddr& remoteSocketAddr, const ConnectedFunc& connectedFunc, bool* timedOut = nullptr, const tchar* nameHint = nullptr) override;

		virtual void GetTotalSendAndRecv(u64& outSend, u64& outRecv) override;

	private:
		bool EnsureInitialized(Logger& logger);

		struct Connection;
		struct ListenEntry;
		bool ThreadListen(Logger& logger, ListenEntry& entry);
		void ThreadRecv(Connection& connection);

		LoggerWithWriter m_logger;
		ReaderWriterLock m_listenEntriesLock;
		List<ListenEntry> m_listenEntries;

		ReaderWriterLock m_connectionsLock;
		List<Connection> m_connections;

		Atomic<u64> m_totalSend;
		Atomic<u64> m_totalRecv;

		#if PLATFORM_WINDOWS
		bool m_wsaInitDone = false;
		#endif
	};
};

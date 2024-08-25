// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaNetworkBackendTcp.h"
#include "UbaEvent.h"
#include "UbaHash.h"
#include "UbaPlatform.h"
#include "UbaStringBuffer.h"
#include "UbaTimer.h"

#if PLATFORM_LINUX
#include <netinet/tcp.h>
#endif

#if PLATFORM_WINDOWS
#include <iphlpapi.h>
#include <ipifcons.h>
#pragma comment (lib, "Netapi32.lib")
#pragma comment (lib, "Ws2_32.lib")
#pragma comment(lib, "IPHLPAPI.lib") // For GetAdaptersInfo
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <poll.h>
#define TIMEVAL timeval
#define SOCKET_ERROR -1
#define SOCKET int
#define INVALID_SOCKET -1
#define SD_BOTH SHUT_RDWR
#define WSAHOST_NOT_FOUND 0
#define WSAEADDRINUSE EADDRINUSE
#define closesocket(a) close(a)
#define addrinfoW addrinfo
#define GetAddrInfoW getaddrinfo
#define FreeAddrInfoW freeaddrinfo
#define WSAGetLastError() errno
#define strcpy_s(a, b, c) strcpy(a, c)
#define WSAPOLLFD pollfd
#define WSAPoll poll
#endif

#define UBA_LOCK_AROUND_SEND 1  // !PLATFORM_WINDOWS

namespace uba
{
	constexpr u32 MaxHeaderSize = 24;

	struct NetworkBackendTcp::ListenEntry
	{
		StringBuffer<128> ip;
		u16 port;
		ListenConnectedFunc connectedFunc;
		Event listening;
		Atomic<SOCKET> socket = INVALID_SOCKET;
		Thread thread;
	};

	struct NetworkBackendTcp::Connection
	{
		Connection(Logger& l, SOCKET s) : logger(l), socket(s), ready(true) { CreateGuid(uid); }

		Logger& logger;
		Atomic<SOCKET> socket;

		Event ready;
		Guid uid;
		u32 headerSize = 0;
		u32 recvTimeoutMs = 0;

		void* recvContext = nullptr;
		RecvHeaderCallback* headerCallback = nullptr;
		RecvBodyCallback* bodyCallback = nullptr;
		const tchar* recvHint = TC("");

		void* dataSentContext = nullptr;
		DataSentCallback* dataSentCallback = nullptr;

		void* disconnectContext = nullptr;
		DisconnectCallback* disconnectCallback = nullptr;

		ReaderWriterLock sendLock;
		CriticalSection shutdownLock;

		Thread recvThread;

		Connection(const Connection&) = delete;
		void operator=(const Connection&) = delete;
	};

	bool SetKeepAlive(Logger& logger, SOCKET socket);
	bool SetBlocking(Logger& logger, SOCKET socket, bool blocking);
	bool DisableNagle(Logger& logger, SOCKET socket);
	bool SendSocket(Logger& logger, SOCKET socket, const void* b, u64 bufferLen);
	bool RecvSocket(Logger& logger, SOCKET socket, void* b, u32 bufferLen, u32 timeoutMs, const Guid& connection, const tchar* hint1, const tchar* hint2, bool isFirstCall);

	bool NetworkBackendTcp::EnsureInitialized(Logger& logger)
	{
		#if PLATFORM_WINDOWS
		WSADATA wsaData;
		if (!m_wsaInitDone)
			if (int res = WSAStartup(MAKEWORD(2, 2), &wsaData))
				return logger.Error(TC("WSAStartup failed (%d)"), res);
		m_wsaInitDone = true;
		#else
		static bool initOnce = []()
			{
				struct sigaction sa = { { SIG_IGN } };
				sigaction(SIGPIPE, &sa, NULL); // Needed for broken pipe that can happen if helpers crash
				return true;
			}();

		#endif
		return true;
	}

	bool CloseSocket(Logger& logger, SOCKET s)
	{
		if (s != INVALID_SOCKET)
			if (closesocket(s) == SOCKET_ERROR)
				return logger.Error(TC("failed to close socket (%s)"), LastErrorToText(WSAGetLastError()).data);
		return true;
	}



	NetworkBackendTcp::NetworkBackendTcp(LogWriter& writer, const tchar* prefix)
		: m_logger(writer, prefix)
	{
	}

	NetworkBackendTcp::~NetworkBackendTcp()
	{
		StopListen();

		SCOPED_WRITE_LOCK(m_connectionsLock, lock);
		for (auto& conn : m_connections)
		{
			ScopedCriticalSection lock2(conn.shutdownLock);
			if (conn.socket == INVALID_SOCKET)
				continue;
			SOCKET s = conn.socket;
			conn.socket = INVALID_SOCKET;
			shutdown(s, SD_BOTH);
			lock2.Leave();
			conn.recvThread.Wait();
			CloseSocket(conn.logger, s);
		}
		m_connections.clear();

		#if PLATFORM_WINDOWS
		if (m_wsaInitDone)
			WSACleanup();
		#endif
	}

	void NetworkBackendTcp::Shutdown(void* connection)
	{
		auto& conn = *(Connection*)connection;
		ScopedCriticalSection lock(conn.shutdownLock);
		if (conn.socket == INVALID_SOCKET)
			return;
		shutdown(conn.socket, SD_BOTH);
	}

	bool NetworkBackendTcp::Send(Logger& logger, void* connection, const void* data, u32 dataSize, SendContext& sendContext)
	{
		auto& conn = *(Connection*)connection;
		sendContext.isUsed = true;

		#if UBA_LOCK_AROUND_SEND
		SCOPED_WRITE_LOCK(conn.sendLock, lock);
		#else
		SCOPED_READ_LOCK(conn.sendLock, lock);
		#endif
		if (conn.socket == INVALID_SOCKET)
			return false;
		bool res = SendSocket(logger, conn.socket, data, dataSize);

		#if UBA_LOCK_AROUND_SEND
		lock.Leave();
		#endif

		sendContext.isFinished = true;

		m_totalSend += dataSize;

		if (auto c = conn.dataSentCallback)
			c(conn.dataSentContext, dataSize);
		return res;
	}

	void NetworkBackendTcp::SetDataSentCallback(void* connection, void* context, DataSentCallback* callback)
	{
		auto& conn = *(Connection*)connection;
		conn.dataSentCallback = callback;
		conn.dataSentContext = context;
	}

	void NetworkBackendTcp::SetRecvCallbacks(void* connection, void* context, u32 headerSize, RecvHeaderCallback* h, RecvBodyCallback* b, const tchar* recvHint)
	{
		UBA_ASSERT(h);
		UBA_ASSERT(headerSize <= MaxHeaderSize);
		auto& conn = *(Connection*)connection;

		ScopedCriticalSection lock(conn.shutdownLock);
		UBA_ASSERTF(conn.disconnectCallback, TC("SetDisconnectCallback must be called before SetRecvCallbacks"));
		conn.recvContext = context;
		conn.headerSize = headerSize;
		conn.headerCallback = h;
		conn.bodyCallback = b;
		conn.recvHint = recvHint;
		conn.ready.Set();
	}

	void NetworkBackendTcp::SetRecvTimeout(void* connection, u32 timeoutMs)
	{
		auto& conn = *(Connection*)connection;
		conn.recvTimeoutMs = timeoutMs;
	}

	void NetworkBackendTcp::SetDisconnectCallback(void* connection, void* context, DisconnectCallback* callback)
	{
		auto& conn = *(Connection*)connection;
		ScopedCriticalSection lock(conn.shutdownLock);
		conn.disconnectCallback = callback;
		conn.disconnectContext = context;
	}

	bool NetworkBackendTcp::StartListen(Logger& logger, u16 port, const tchar* ip, const ListenConnectedFunc& connectedFunc)
	{
		if (!EnsureInitialized(logger))
			return false;

		SCOPED_WRITE_LOCK(m_listenEntriesLock, lock);

		auto prevListenEntryCount = int(m_listenEntries.size());

		auto AddAddr = [&](const tchar* addr)
			{
				m_listenEntries.emplace_back();
				auto& entry = m_listenEntries.back();
				entry.ip.Append(addr);
				entry.port = port;
				entry.connectedFunc = connectedFunc;
			};

		if (ip && *ip)
		{
			AddAddr(ip);
		}
		else
		{
			TraverseNetworkAddresses(logger, [&](const StringBufferBase& addr)
				{
					AddAddr(addr.data);
					return true;
				});
			AddAddr(TC("127.0.0.1"));
		}

		if (m_listenEntries.empty())
		{
			logger.Warning(TC("No host addresses found for UbaServer. Will not be able to use remote workers"));
			return false;
		}

		auto skipCount = prevListenEntryCount;
		for (auto& e : m_listenEntries)
		{
			if (skipCount-- > 0)
				continue;
			e.listening.Create(true);
			e.thread.Start([this, &logger, &e]
				{
					ThreadListen(logger, e);
					return 0;
				});
		}

		bool success = true;
		skipCount = prevListenEntryCount;
		for (auto& e : m_listenEntries)
		{
			if (skipCount-- > 0)
				continue;
			if (!e.listening.IsSet(4000))
				success = false;
			if (e.socket == INVALID_SOCKET)
				success = false;
			e.listening.Destroy();
		}
		return success;
	}

	void NetworkBackendTcp::StopListen()
	{
		SCOPED_WRITE_LOCK(m_listenEntriesLock, lock);
		for (auto& e : m_listenEntries)
			e.socket = INVALID_SOCKET;
		for (auto& e : m_listenEntries)
			e.thread.Wait();
		m_listenEntries.clear();
	}

	bool NetworkBackendTcp::ThreadListen(Logger& logger, ListenEntry& entry)
	{
		addrinfoW hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET; //AF_UNSPEC; (Skip AF_INET6)
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		// Resolve the server address and port
		struct addrinfoW* result = NULL;
		StringBuffer<32> portStr;
		portStr.AppendValue(entry.port);
		int res = GetAddrInfoW(entry.ip.data, portStr.data, &hints, &result);

		auto listenEv = MakeGuard([&]() { entry.listening.Set(); });

		if (res != 0)
			return logger.Error(TC("getaddrinfo failed (%d)"), res);

		UBA_ASSERT(result);
		auto addrGuard = MakeGuard([result]() { FreeAddrInfoW(result); });

		// Create a socket for listening to connections
		SOCKET listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (listenSocket == INVALID_SOCKET)
			return logger.Error(TC("socket failed (%s)"), LastErrorToText(WSAGetLastError()).data);

		auto listenSocketCleanup = MakeGuard([&]() { CloseSocket(logger, listenSocket); });

		u32 reuseAddr = 1;
		if (::setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddr, sizeof reuseAddr) == SOCKET_ERROR)
			return logger.Error(TC("setsockopt SO_REUSEADDR failed (error: %s)"), LastErrorToText(WSAGetLastError()).data);

		// Setup the TCP listening socket
		res = bind(listenSocket, result->ai_addr, (socklen_t)result->ai_addrlen);

		if (res == SOCKET_ERROR)
		{
			int lastError = WSAGetLastError();
			if (lastError != WSAEADDRINUSE)
				return logger.Error(TC("bind %s:%hu failed (%s)"), entry.ip.data, entry.port, LastErrorToText(lastError).data);
			logger.Info(TC("bind %s:%hu failed because address/port is in use. Some other process is already using this address/port"), entry.ip.data, entry.port);
			return false;
		}

		addrGuard.Execute();

		res = listen(listenSocket, SOMAXCONN);
		if (res == SOCKET_ERROR)
			return logger.Error(TC("Listen failed (%s)"), LastErrorToText(WSAGetLastError()).data);

		if (!SetKeepAlive(logger, listenSocket))
			return false;

		logger.Info(TC("Listening on %s:%hu"), entry.ip.data, entry.port);
		entry.socket = listenSocket;

		listenEv.Execute();

		while (entry.socket != INVALID_SOCKET)
		{
			WSAPOLLFD p;
			p.fd = listenSocket;
			p.revents = 0;
			p.events = POLLIN;
			int timeoutMs = 2000;
			int pollRes = WSAPoll(&p, 1, timeoutMs);

			if (pollRes == SOCKET_ERROR)
			{
				int lastError = WSAGetLastError();
				logger.Warning(TC("WSAPoll returned error %s"), LastErrorToText(lastError).data);
				break;
			}

			if (!pollRes)
				continue;

			if (p.revents & POLLNVAL)
			{
				logger.Warning(TC("WSAPoll returned successful but with unexpected flags: %u"), p.revents);
				continue;
			}

			sockaddr remoteSockAddr = { 0 }; // for TCP/IP
			socklen_t remoteSockAddrLen = sizeof(remoteSockAddr);
			SOCKET clientSocket = accept(listenSocket, (sockaddr*)&remoteSockAddr, &remoteSockAddrLen);

			if (clientSocket == INVALID_SOCKET)
			{
				if (entry.socket != INVALID_SOCKET)
					logger.Info(TC("Accept failed with WSA error: %s"), LastErrorToText(WSAGetLastError()).data);
				break;
			}

			if (!DisableNagle(logger, clientSocket) || !SetKeepAlive(logger, clientSocket))
			{
				CloseSocket(logger, clientSocket);
				continue;
			}

			SCOPED_WRITE_LOCK(m_connectionsLock, lock);
			auto it = m_connections.emplace(m_connections.end(), logger, clientSocket);
			auto& conn = *it;
			conn.recvThread.Start([this, connPtr = &conn] { ThreadRecv(*connPtr); return 0; });
			lock.Leave();

			if (!entry.connectedFunc(&conn, remoteSockAddr))
			{
				shutdown(clientSocket, SD_BOTH);
				conn.ready.Set();
				conn.recvThread.Wait();
				SCOPED_WRITE_LOCK(m_connectionsLock, lock2);
				m_connections.erase(it);
				continue;
			}
		}

		return true;
	}

	void NetworkBackendTcp::ThreadRecv(Connection& connection)
	{
		ElevateCurrentThreadPriority();
		
		auto& logger = connection.logger;

		if (connection.ready.IsSet(60000)) // This should never time out!
		{
			bool isFirst = true;
			while (connection.socket != INVALID_SOCKET)
			{
				void* bodyContext = nullptr;
				u8* bodyData = nullptr;
				u32 bodySize = 0;

				u8 headerData[MaxHeaderSize];
				if (!RecvSocket(logger, connection.socket, headerData, connection.headerSize, connection.recvTimeoutMs, connection.uid, connection.recvHint, TC(""), isFirst))
					break;
				isFirst = false;

				m_totalRecv += connection.headerSize;

				auto hc = connection.headerCallback;
				if (!hc)
				{
					logger.Error(TC("Tcp connection header callback not set"));
					break;
				}

				if (!hc(connection.recvContext, connection.uid, headerData, bodyContext, bodyData, bodySize))
					break;
				if (!bodySize)
					continue;

				bool success = RecvSocket(logger, connection.socket, bodyData, bodySize, connection.recvTimeoutMs, connection.uid, connection.recvHint, TC("Body"), false);

				m_totalRecv += bodySize;

				auto bc = connection.bodyCallback;
				if (!bc)
				{
					logger.Error(TC("Tcp connection body callback not set"));
					break;
				}

				if (!bc(connection.recvContext, !success, headerData, bodyContext, bodyData, bodySize))
					break;
				if (!success)
					break;
			}
		}
		else
		{
			logger.Warning(TC("Tcp connection timed out waiting for recv thread to be ready"));
		}

		ScopedCriticalSection lock2(connection.shutdownLock);
		SOCKET s = connection.socket;

		{
			SCOPED_WRITE_LOCK(connection.sendLock, lock);
			connection.socket = INVALID_SOCKET;
		}
		if (auto cb = connection.disconnectCallback)
		{
			auto context = connection.disconnectContext;
			connection.disconnectCallback = nullptr;
			connection.disconnectContext = nullptr;
			cb(context, connection.uid, &connection);
		}

		if (s == INVALID_SOCKET)
			return;
		shutdown(s, SD_BOTH);
		CloseSocket(logger, s);
	}

	bool NetworkBackendTcp::Connect(Logger& logger, const tchar* ip, const ConnectedFunc& connectedFunc, u16 port, bool* timedOut)
	{
		if (!EnsureInitialized(logger))
			return false;

		u64 startTime = GetTime();

		if (timedOut)
			*timedOut = false;

		bool connected = false;
		bool success = true;
		TraverseRemoteAddresses(logger, ip, port, [&](const sockaddr& remoteSockaddr)
			{
				bool timedOut2 = false;
				connected = Connect(logger, remoteSockaddr, connectedFunc, &timedOut2, ip);
				if (connected)
					return false;
				if (timedOut2)
					return true;
				success = false;
				return false;
			});

		if (connected)
			return true;

		if (!success)
			return false;

		if (!timedOut)
			return false;

		*timedOut = true;
		int connectTimeMs = int(TimeToMs(GetTime() - startTime));
		int timeoutMs = 2000;
		if (connectTimeMs < timeoutMs)
			Sleep(u32(timeoutMs - connectTimeMs));
		return false;
	}

	bool NetworkBackendTcp::Connect(Logger& logger, const sockaddr& remoteSocketAddr, const ConnectedFunc& connectedFunc, bool* timedOut, const tchar* nameHint)
	{
		// Create a socket for connecting to server

		//TODO: Wrap this up in a better function
#if PLATFORM_WINDOWS
		SOCKET socketFd = WSASocketW(remoteSocketAddr.sa_family, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
		SOCKET socketFd = socket(remoteSocketAddr.sa_family, SOCK_STREAM, IPPROTO_TCP);
#endif
		if (socketFd == INVALID_SOCKET)
			return logger.Error(TC("socket failed (%s)"), LastErrorToText(WSAGetLastError()).data);

		// Create guard in case we fail to connect (will be cancelled further down if we succeed)
		auto socketClose = MakeGuard([&]() { CloseSocket(logger, socketFd); });

		// Set to non-blocking just for the connect call (we want to control the connect timeout after connect using select instead)
		if (!SetBlocking(logger, socketFd, false))
			return false;

		// Connect to server.
		int res = ::connect(socketFd, &remoteSocketAddr, sizeof(remoteSocketAddr));

#if PLATFORM_WINDOWS
		if (res == SOCKET_ERROR)
			if (WSAGetLastError() != WSAEWOULDBLOCK)
				return false;
#else
		if (res != 0)
		{
			if (errno != EINPROGRESS)
			{
				logger.Error(TC("Connect failed (%d: %s)"), WSAGetLastError(), LastErrorToText(WSAGetLastError()).data);
				return false;
			}
		}
#endif

		// Return to blocking since we want select to block
		if (!SetBlocking(logger, socketFd, true))
			return false;

		int timeoutMs = 2000;

		WSAPOLLFD p;
		p.fd = socketFd;
		p.revents = 0;
		p.events = POLLOUT;
		int pollRes = WSAPoll(&p, 1, timeoutMs);

		if (pollRes == SOCKET_ERROR)
		{
			int lastError = WSAGetLastError();
			logger.Warning(TC("WSAPoll returned error %s (%s)"), LastErrorToText(lastError).data, nameHint);
			return false;
		}

		u16 validFlags = POLLERR | POLLHUP; // Treat hangup as timeout (since we want retry if that happens). Also treat error as timeout. This is needed for Wine agent to be able to retry
		if (!pollRes || p.revents & validFlags)
		{
			if (timedOut)
				*timedOut = true;
			return false;
		}

		if (p.revents & POLLNVAL)
		{
			logger.Warning(TC("WSAPoll returned successful but with unexpected flags: %u"), p.revents);
			return false;
		}


#if !PLATFORM_WINDOWS
		// Before we send anything even though the
		// the socket is writable, but let's make sure
		// the connection is actually valid by getting
		// information about what we've connected to
		struct sockaddr_in junk;
		socklen_t length = sizeof(junk);
		memset(&junk, 0, sizeof(junk));
		if (getpeername(socketFd, (struct sockaddr *)&junk, &length) != 0)
		{
			if (timedOut)
				*timedOut = true;
			return false;
		}

		int sent = (int)send(socketFd, nullptr, 0, 0);
		if (sent == SOCKET_ERROR)
		{
			if (errno == ECONNREFUSED || errno == EPIPE)
			{
				if (timedOut)
					*timedOut = true;
				return false;
			}
			return false;
		}
#endif

		// Socket is good, cancel the socket close scope and break out of the loop.
		if (!DisableNagle(logger, socketFd))
			return false;

		if (!SetKeepAlive(logger, socketFd))
			return false;

		socketClose.Cancel();

		SCOPED_WRITE_LOCK(m_connectionsLock, lock);
		auto it = m_connections.emplace(m_connections.end(), logger, socketFd);
		auto& conn = *it;
		conn.recvThread.Start([this, connPtr = &conn] { ThreadRecv(*connPtr); return 0; });
		lock.Leave();

		if (!connectedFunc(&conn, remoteSocketAddr, timedOut))
		{
			shutdown(socketFd, SD_BOTH);
			conn.ready.Set();
			conn.recvThread.Wait();
			SCOPED_WRITE_LOCK(m_connectionsLock, lock2);
			m_connections.erase(it);
			return false;
		}

		//char* ip = inet_ntoa(((sockaddr_in*)const_cast<sockaddr*>(&remoteSocketAddr))->sin_addr);
		if (nameHint)
			logger.Detail(TC("Connected to %s:%u (%s)"), nameHint, ((sockaddr_in&)remoteSocketAddr).sin_port, GuidToString(conn.uid).str);
		else
			logger.Detail(TC("Connected using sockaddr (%s)"), GuidToString(conn.uid).str);

		return true;
	}

	void NetworkBackendTcp::GetTotalSendAndRecv(u64& outSend, u64& outRecv)
	{
		outSend = m_totalSend;
		outRecv = m_totalRecv;
	}

	bool SetBlocking(Logger& logger, SOCKET socket, bool blocking)
	{
#if PLATFORM_WINDOWS
		u_long value = blocking ? 0 : 1;
		if (ioctlsocket(socket, FIONBIO, &value) == SOCKET_ERROR)
			return logger.Error(TC("Setting non blocking socket failed (error: %s)"), LastErrorToText(WSAGetLastError()).data);
#else
		int flags = fcntl(socket, F_GETFL, 0);
		if (flags == -1) return false;
		flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
		if (fcntl(socket, F_SETFL, flags) != 0)
			return logger.Error(TC("Setting non blocking socket failed (error: %s)"), LastErrorToText(WSAGetLastError()).data);
#endif
		return true;
	}

	bool DisableNagle(Logger& logger, SOCKET socket)
	{
#if !PLATFORM_MAC
		u32 value = 1;
		if (setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&value, sizeof(value)) == SOCKET_ERROR)
			return logger.Error(TC("setsockopt TCP_NODELAY error: (error: %s)"), LastErrorToText(WSAGetLastError()).data);
#endif
		return true;
	}

	bool SetKeepAlive(Logger& logger, SOCKET socket)
	{
		u32 value = 1;
		if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&value, sizeof(value)) == SOCKET_ERROR)
			return logger.Error(TC("setsockopt SO_KEEPALIVE (error: %s)"), LastErrorToText(WSAGetLastError()).data);
		return true;
	}

	bool SendSocket(Logger& logger, SOCKET socket, const void* b, u64 bufferLen)
	{
		u64 left = bufferLen;
		while (left)
		{
			int sent = (int)send(socket, (char*)b, u32(bufferLen), 0);
			if (sent == SOCKET_ERROR)
			{
				//#if UBA_DEBUG
				//logger.Warning(TC("ERROR sending socket (error: %s)"), LastErrorToText(WSAGetLastError()).data);
				//#endif
				return false;
			}

			left -= sent;

			#if PLATFORM_WINDOWS
			UBA_ASSERTF(left == 0, L"Failed to send all data in one call. Wanted to send %llu, sent %i", bufferLen, sent);
			#endif
		}
		return true;
	}

	bool RecvSocket(Logger& logger, SOCKET socket, void* b, u32 bufferLen, u32 timeoutMs, const Guid& connection, const tchar* hint1, const tchar* hint2, bool isFirstCall)
	{
		u8* buffer = (u8*)b;
		u32 recvLeft = bufferLen;
		while (recvLeft)
		{
			if (timeoutMs)
			{
				WSAPOLLFD p;
				p.fd = socket;
				p.revents = 0;
				p.events = POLLRDNORM;
				int res = WSAPoll(&p, 1, int(timeoutMs));
				if (!res)
				{
					logger.Info(TC("WSAPoll returned timeout for connection %s after %s (%s%s)"), GuidToString(connection).str, TimeToText(MsToTime(timeoutMs)).str, hint1, hint2);
					return false;
				}
				if (res == SOCKET_ERROR)
				{
#if UBA_DEBUG && PLATFORM_WINDOWS
					// When cancelling all kinds of errors can happen..
					int lastError = WSAGetLastError();
					if (lastError != WSAEINTR && lastError != WSAESHUTDOWN && lastError != WSAECONNABORTED && lastError != WSAECONNRESET) // Interrupted by cancel
						logger.Warning(TC("WSAPoll returned an error for connection %s: %s (%s%s)"), GuidToString(connection).str, LastErrorToText(lastError).data, hint1, hint2);
#endif
				}
			}

			int read = (int)recv(socket, (char*)buffer, recvLeft, 0);
			if (read == 0)
			{
				//#if UBA_DEBUG
				//logger.Warning(TC("Socket closed while in recv"));
				//#endif
				return false;
			}

			if (read == SOCKET_ERROR)
			{
#if PLATFORM_WINDOWS
#if UBA_DEBUG
				// When cancelling all kinds of errors can happen..
				int lastError = WSAGetLastError();
				if (lastError != WSAEINTR && lastError != WSAESHUTDOWN && lastError != WSAECONNABORTED && lastError != WSAECONNRESET) // Interrupted by cancel
					logger.Warning(TC("ERROR receiving socket: %s (%s%s)"), LastErrorToText(lastError).data, hint1, hint2);
#endif
#else
				if (!isFirstCall && errno != ECONNRESET)
					logger.Error(TC("ERROR receiving socket %i after %u bytes (%s%s) (%s)"), socket, bufferLen, hint1, hint2, strerror(errno));
#endif
				return false;
			}
			recvLeft -= (u32)read;
			buffer += read;
		}
		return true;
	}

	void TraverseNetworkAddresses(Logger& logger, const Function<bool(const StringBufferBase& addr)>& func)
	{
#if PLATFORM_WINDOWS
		// Fallback code for some cloud setups where we can't use the dns to find out ip addresses. (note it always work by providing the adapter we want to listen on)
		IP_ADAPTER_INFO info[16];
		ULONG bufLen = sizeof(info);
		if (GetAdaptersInfo(info, &bufLen) != ERROR_SUCCESS)
		{
			logger.Info(TC("GetAdaptersInfo failed (%s)"), LastErrorToText(WSAGetLastError()).data);
			return;
		}
		for (IP_ADAPTER_INFO* it = info; it; it = it->Next)
		{
			if (it->Type != MIB_IF_TYPE_ETHERNET && it->Type != IF_TYPE_IEEE80211)
				continue;
			for (IP_ADDR_STRING* s = &it->IpAddressList; s; s = s->Next)
			{
				StringBuffer<128> ip;
				ip.Appendf(TC("%hs"), s->IpAddress.String);
				if (ip.Equals(L"0.0.0.0"))
					continue;
				if (!func(ip))
					return;
			}
		}
#else
		struct ifaddrs* ifaddr;
		if (getifaddrs(&ifaddr) == -1)
		{
			logger.Info("getifaddrs failed");
			return;
		}
		auto g = MakeGuard([ifaddr]() { freeifaddrs(ifaddr); });

		for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next)
		{
			if (ifa->ifa_addr == nullptr)
				continue;

			int family = ifa->ifa_addr->sa_family;
			if (family != AF_INET)
				continue;

			StringBuffer<NI_MAXHOST> ip;
			int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), ip.data, ip.capacity, NULL, 0, NI_NUMERICHOST);
			if (s != 0)
				continue;
			ip.count = strlen(ip.data);
			if (ip.StartsWith("169.254") || ip.Equals("127.0.0.1"))
				continue;
			if (!func(ip))
				return;
		}
#endif
	}

	bool TraverseRemoteAddresses(Logger& logger, const tchar* addr, u16 port, const Function<bool(const sockaddr& remoteSockaddr)>& func)
	{
		addrinfoW  hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET; //AF_UNSPEC; (Skip AF_INET6)
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		StringBuffer<32> portStr;
		portStr.AppendValue(port);

		// Resolve the server address and port
		addrinfoW* remoteAddrInfo = nullptr;
		int res = GetAddrInfoW(addr, portStr.data, &hints, &remoteAddrInfo);
		if (res != 0)
		{
			if (res == WSAHOST_NOT_FOUND)
				return logger.Error(TC("Invalid server address '%s'"), addr);
			//logger.Error(TC("GetAddrInfoW failed with error: %s"), getErrorText(res).c_str());
			return false;
		}

		auto addrCleanup = MakeGuard([&]() { if (remoteAddrInfo) FreeAddrInfoW(remoteAddrInfo); });

		auto addrInfoIt = remoteAddrInfo;
		// Loop through and attempt to connect to an address until one succeeds
		for (; addrInfoIt != NULL; addrInfoIt = addrInfoIt->ai_next)
			if (!func(*addrInfoIt->ai_addr))
				return true;
		return true;
	}

	HttpConnection::HttpConnection()
	{
		m_socket = INVALID_SOCKET;
		*m_host = 0;
	}

	HttpConnection::~HttpConnection()
	{
		if (m_socket != INVALID_SOCKET)
		{
			LoggerWithWriter logger(g_nullLogWriter);
			CloseSocket(logger, m_socket);
		}

		#if PLATFORM_WINDOWS
		if (m_wsaInitDone)
			WSACleanup();
		#endif
	}

	bool HttpConnection::Connect(Logger& logger, const char* host)
	{
		#if PLATFORM_WINDOWS
		WSADATA wsaData;
		if (!m_wsaInitDone)
			if (int res = WSAStartup(MAKEWORD(2, 2), &wsaData))
				return logger.Error(TC("WSAStartup failed (%d)"), res);
		m_wsaInitDone = true;
		#endif

		protoent* protoent = getprotobyname("tcp");
		if (protoent == NULL)
			return logger.Error(TC("HttpRequest: socket error"));

		SOCKET sock = socket(AF_INET, SOCK_STREAM, protoent->p_proto);
		if (sock == -1)
			return logger.Error(TC("HttpRequest: socket error"));
		auto socketClose = MakeGuard([sock]() { closesocket(sock); });

		hostent* hostent = gethostbyname(host);
		if (hostent == NULL)
			return logger.Error(TC("HttpRequest: gethostbyname error"));

		unsigned long in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
		if (in_addr == INADDR_NONE)
			return logger.Error(TC("HttpRequest: inet_addr error"));

		sockaddr_in sockaddr_in;
		sockaddr_in.sin_addr.s_addr = in_addr;
		sockaddr_in.sin_family = AF_INET;
		sockaddr_in.sin_port = htons(80);

		if (connect(sock, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in)) == -1)
			return false;// logger.Error(TC("HttpRequest: connect error"));

		socketClose.Cancel();

		strcpy_s(m_host, sizeof_array(m_host), host);
		m_socket = sock;
		return true;
	}

	bool HttpConnection::Query(Logger& logger, const char* type, StringBufferBase& outResponse, u32& outStatusCode, const char* host, const char* path, const char* header)
	{
		// TODO: Fix so we reuse socket connection for multiple queries
		if (*m_host)// && _stricmp(m_host, host) != 0)
		{
			CloseSocket(logger, m_socket);
			m_socket = INVALID_SOCKET;
			*m_host = 0;
		}

		if (m_socket == INVALID_SOCKET)
			if (!Connect(logger, host))
				return false;

		char request[512];
		int requestLen = snprintf(request, 512, "%s /%s HTTP/1.1\r\nHost: %s\r\n%s\r\n", type, path, m_host, header);

		int totalBytesSent = 0;
		while (totalBytesSent < requestLen) {
			int bytesSent = send(m_socket, request + totalBytesSent, requestLen - totalBytesSent, 0);
			if (bytesSent == -1)
				return logger.Error(TC("HttpRequest: send error"));
			totalBytesSent += bytesSent;
		}

#if PLATFORM_WINDOWS
#pragma warning(push)
#pragma warning(disable:6386) // analyzer claims that buf can have buffer overrun.. but can't see how that can happen
#endif

		u32 readPos = 0;
		char buf[4*1024];
		int bytesRead = 0;
		while ((bytesRead = recv(m_socket, buf + readPos, sizeof(buf) - readPos, 0)) > 0)
			readPos += bytesRead;

		if (bytesRead == -1)
			return logger.Error(TC("HttpRequest: recv error"));

		if (readPos == sizeof(buf))
			return logger.Error(TC("HttpRequest: buffer overflow"));

		buf[readPos] = 0;

#if PLATFORM_WINDOWS
#pragma warning(pop)
#endif

		char* firstSpace = strchr(buf, ' '); // After version (where status code starts)
		if (!firstSpace)
			return logger.Error(TC("HttpRequest: first space not found (read %u)"), readPos);
		char* secondSpace = strchr(firstSpace + 1, ' '); // after status code
		if (!secondSpace)
			return logger.Error(TC("HttpRequest: second space not found"));
		*secondSpace = 0;
		outStatusCode = strtoul(firstSpace + 1, nullptr, 10);

		char* bodyStart = strstr(secondSpace + 1, "\r\n\r\n");
		if (!bodyStart)
			return logger.Error(TC("HttpRequest: no body found"));
		outResponse.Append(bodyStart + 4);
		return true;
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCommon.h"

UNSYNC_THIRD_PARTY_INCLUDES_START

#if UNSYNC_PLATFORM_WINDOWS
#	include <winsock2.h>  // must be first
#	include <ws2tcpip.h>
#	pragma comment(lib, "Ws2_32.lib")
#endif	// UNSYNC_PLATFORM_WINDOWS

#if UNSYNC_PLATFORM_UNIX
#	include <sys/socket.h>
#	include <sys/types.h>
#	include <netinet/in.h>
#	include <netdb.h>
#	include <arpa/inet.h>
#	include <unistd.h>
#	define SOCKET		   int
#	define INVALID_SOCKET (SOCKET)(~0)
#	define SOCKET_ERROR   (-1)
#	define closesocket(x) close(x)
#endif	// UNSYNC_PLATFORM_UNIX

#include <unordered_set>

#if UNSYNC_USE_TLS
#	include <tls.h>
#endif	// UNSYNC_USE_TLS

UNSYNC_THIRD_PARTY_INCLUDES_END

#include "UnsyncBuffer.h"
#include "UnsyncHash.h"
#include "UnsyncLog.h"
#include "UnsyncSocket.h"
#include "UnsyncUtil.h"

namespace unsync {

static_assert(sizeof(FSocketAddress) >= sizeof(sockaddr), "SocketAddress is too small");
static_assert(sizeof(FSocketAddress) >= sizeof(sockaddr_in), "SocketAddress is too small");

struct FSocketInitHelper
{
	FSocketInitHelper()
	{
#if UNSYNC_PLATFORM_WINDOWS
		WSADATA Wsa;
		if (WSAStartup(MAKEWORD(2, 2), &Wsa) != 0)
		{
			UNSYNC_ERROR(L"WSAStartup failed: %d", WSAGetLastError());
		}
#endif	// UNSYNC_PLATFORM_WINDOWS

#if UNSYNC_USE_TLS
		tls_init();
#endif	// UNSYNC_USE_TLS
	}
	~FSocketInitHelper()
	{
#if UNSYNC_PLATFORM_WINDOWS
		WSACleanup();
#endif	// UNSYNC_PLATFORM_WINDOWS
	}
};

static void
LazyInitSockets()
{
	static FSocketInitHelper InitHelper;
}

FSocketHandle
SocketConnectTcp(const char* DestAddress, uint16 Port)
{
	LazyInitSockets();

	SOCKET Sock = socket(AF_INET, SOCK_STREAM, 0);

	if (Sock == INVALID_SOCKET)
	{
#if UNSYNC_PLATFORM_WINDOWS
		UNSYNC_ERROR(L"Failed to create TCP socket (error code %d)", WSAGetLastError());
#else
		UNSYNC_ERROR(L"Failed to create TCP socket");  // TODO: report unix socket error
#endif	// UNSYNC_PLATFORM_WINDOWS
		return 0;
	}

	sockaddr_in SockAddr = {};
	SockAddr.sin_family	 = AF_INET;
	SockAddr.sin_port	 = htons(Port);

	struct addrinfo AddrHints = {};
	AddrHints.ai_family		  = PF_UNSPEC;
	AddrHints.ai_socktype	  = SOCK_STREAM;
	AddrHints.ai_flags		  = AI_CANONNAME;

	struct addrinfo* Addr = {};

	char ResolvedAddress[100] = {};

	int Err = getaddrinfo(DestAddress, nullptr, &AddrHints, &Addr);
	if (Err == 0)
	{
		struct addrinfo* Curr = Addr;
		while (Curr)
		{
			if (Curr->ai_family == SockAddr.sin_family)
			{
				void* AddrData = nullptr;
				AddrData	   = &((struct sockaddr_in*)Curr->ai_addr)->sin_addr;
				if (inet_ntop(Curr->ai_family, AddrData, ResolvedAddress, sizeof(ResolvedAddress)))
				{
					DestAddress = ResolvedAddress;
					break;
				}
			}
			Curr = Curr->ai_next;
		}
	}
	else
	{
		UNSYNC_ERROR(L"Invalid address '%hs'", DestAddress);
		return 0;
	}

	if (Addr)
	{
		freeaddrinfo(Addr);
	}

	if (inet_pton(SockAddr.sin_family, DestAddress, &SockAddr.sin_addr) <= 0)
	{
		UNSYNC_ERROR(L"Invalid address '%hs'", DestAddress);
		return 0;
	}

	if (connect(Sock, (struct sockaddr*)&SockAddr, sizeof(SockAddr)) < 0)
	{
		UNSYNC_LOG(L"Warning: Failed to connect to '%hs' on port %d", DestAddress, Port);
		return 0;
	}

	return Sock;
}

void
SocketClose(FSocketHandle Socket)
{
	if (Socket)
	{
		closesocket(Socket);
	}
}

int
SocketSend(FSocketHandle Socket, const void* Data, size_t DataSize)
{
	if (DataSize == 0)
		return 0;

	int Result = send(Socket, reinterpret_cast<const char*>(Data), (int)DataSize, 0);
	return Result;
}

int
SocketRecvAll(FSocketHandle Socket, void* Data, size_t DataSize)
{
	if (DataSize == 0)
		return 0;

	return recv(Socket, reinterpret_cast<char*>(Data), (int)DataSize, MSG_WAITALL);
}

int
SocketRecvAny(FSocketHandle Socket, void* Data, size_t DataSize)
{
	if (DataSize == 0)
		return 0;

	return recv(Socket, reinterpret_cast<char*>(Data), (int)DataSize, 0);
}

bool
SocketValid(FSocketHandle Socket)
{
	int RecvResult = send(Socket, nullptr, 0, 0);
	return RecvResult != SOCKET_ERROR;
}

FSocketAddress
SocketMakeAddress(const char* Address, uint16 Port)
{
	sockaddr_in Addr	 = {};
	Addr.sin_family		 = AF_INET;
	Addr.sin_addr.s_addr = inet_addr(Address);
	Addr.sin_port		 = htons(Port);
	FSocketAddress Result;
	memcpy(&Result, &Addr, sizeof(Addr));
	return Result;
}

int
FSocketRaw::Send(const void* Data, size_t DataSize)
{
	return SocketSend(Handle, Data, DataSize);
}

int
FSocketRaw::RecvAll(void* Data, size_t DataSize)
{
	return SocketRecvAll(Handle, Data, DataSize);
}

int
FSocketRaw::RecvAny(void* Data, size_t DataSize)
{
	return SocketRecvAny(Handle, Data, DataSize);
}

#if UNSYNC_USE_TLS
FSocketTls::FSocketTls(FSocketHandle InHandle, FTlsClientSettings ClientSettings) : FSocketBase(InHandle)
{
	Security = ESocketSecurity::Unknown;

	tls_config* TlsCfg = tls_config_new();

	if (ClientSettings.CacertData)
	{
		tls_config_set_ca_mem(TlsCfg, ClientSettings.CacertData, (size_t)ClientSettings.CacertSize);
	}
	else
	{
		const FBuffer& SystemRootCerts = GetSystemRootCerts();
		tls_config_set_ca_mem(TlsCfg, SystemRootCerts.Data(), SystemRootCerts.Size());
	}

	if (!ClientSettings.bVerifyCertificate)
	{
		tls_config_insecure_noverifycert(TlsCfg);
	}

	if (!ClientSettings.Subject)
	{
		tls_config_insecure_noverifyname(TlsCfg);
	}

	TlsCtx = tls_client();

	int Err = 0;

	if (Err == 0)
	{
		tls_configure(TlsCtx, TlsCfg);
	}

	tls_config_free(TlsCfg);

	if (Err == 0)
	{
		Err = tls_connect_socket(TlsCtx, (int)Handle, ClientSettings.Subject);
	}

	if (Err == 0)
	{
		Err = tls_handshake(TlsCtx);
	}

	if (Err)
	{
		const char* TlsErrorMsg = tls_error(TlsCtx);
		if (TlsErrorMsg)
		{
			UNSYNC_LOG(L"Warning: Failed to establish TLS connection: %hs", TlsErrorMsg);
		}
		else
		{
			UNSYNC_LOG(L"Warning: Failed to establish TLS connection");
		}
		tls_free(TlsCtx);
		TlsCtx = nullptr;
	}
	else
	{
		const char* ConnVersion = tls_conn_version(TlsCtx);
		if (ConnVersion)
		{
			if (!strcmp(ConnVersion, "TLSv1.3"))
			{
				Security = ESocketSecurity::TLSv1_3;
			}
			else if (!strcmp(ConnVersion, "TLSv1.2"))
			{
				Security = ESocketSecurity::TLSv1_2;
			}
		}
	}
}

FSocketTls::~FSocketTls()
{
	if (TlsCtx)
	{
		tls_close(TlsCtx);
		tls_free(TlsCtx);
	}
}

int
FSocketTls::Send(const void* Data, size_t DataSize)
{
	if (DataSize == 0)
		return 0;

	int ProcessedBytes = 0;
	while (ProcessedBytes < DataSize)
	{
		int Res = CheckedNarrow(tls_write(TlsCtx, (const char*)Data + ProcessedBytes, DataSize - ProcessedBytes));
		if (Res <= 0)
		{
			break;
		}
		ProcessedBytes += Res;
	}

	return ProcessedBytes;
}

int
FSocketTls::RecvAll(void* Data, size_t DataSize)
{
	if (DataSize == 0)
		return 0;

	int ProcessedBytes = 0;
	while (ProcessedBytes < DataSize)
	{
		int Res = CheckedNarrow(tls_read(TlsCtx, (char*)Data + ProcessedBytes, DataSize - ProcessedBytes));
		if (Res <= 0)
		{
			break;
		}
		ProcessedBytes += Res;
	}

	return ProcessedBytes;
}

int
FSocketTls::RecvAny(void* Data, size_t DataSize)
{
	if (DataSize == 0)
		return 0;

	int ProcessedBytes = 0;
	do
	{
		int Res = CheckedNarrow(tls_read(TlsCtx, (char*)Data + ProcessedBytes, DataSize - ProcessedBytes));
		if (Res <= 0)
		{
			break;
		}
		ProcessedBytes += Res;
	} while (false);

	return ProcessedBytes;
}
#endif	// UNSYNC_USE_TLS

FSocketBase::~FSocketBase()
{
	SocketClose(Handle);
}

const char*
ToString(ESocketSecurity Security)
{
	switch (Security)
	{
		case ESocketSecurity::None:
			return "None";
#if UNSYNC_USE_TLS
		case ESocketSecurity::TLSv1_2:
			return "TLS 1.2";
		case ESocketSecurity::TLSv1_3:
			return "TLS 1.3";
#endif	// UNSYNC_USE_TLS
		default:
			return "Unknown";
	}
}

bool
SendBuffer(FSocketBase& Socket, const FBuffer& Data)
{
	int32 Size = int32(Data.Size());
	bool  bOk  = true;
	bOk &= SocketSendT(Socket, Size);
	bOk &= (SocketSend(Socket, Data.Data(), Size) == Size);
	return bOk;
}

}  // namespace unsync

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/Config.h"

#if UE_TRACE_ENABLED && PLATFORM_ANDROID

#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace UE {
namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
UPTRINT ThreadCreate(const ANSICHAR* Name, void (*Entry)())
{
	void* (*PthreadThunk)(void*) = [] (void* Param) -> void * {
		typedef void (*EntryType)(void);
		pthread_setname_np(pthread_self(), "Trace");
		(EntryType(Param))();
		return nullptr;
	};

	pthread_t ThreadHandle;
	if (pthread_create(&ThreadHandle, nullptr, PthreadThunk, reinterpret_cast<void *>(Entry)) != 0)
	{
		return 0;
	}
	return static_cast<UPTRINT>(ThreadHandle);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadSleep(uint32 Milliseconds)
{
	usleep(Milliseconds * 1000U);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadJoin(UPTRINT Handle)
{
	pthread_join(static_cast<pthread_t>(Handle), nullptr);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadDestroy(UPTRINT Handle)
{
	// no-op
}



////////////////////////////////////////////////////////////////////////////////
uint64 TimeGetFrequency()
{
	return 1000000ull;
}

////////////////////////////////////////////////////////////////////////////////
uint64 TimeGetTimestamp()
{
	// should stay in sync with FPlatformTime::Cycles64() or the timeline will be broken!
	struct timespec TimeSpec;
	clock_gettime(CLOCK_MONOTONIC, &TimeSpec);
	return static_cast<uint64>(static_cast<uint64>(TimeSpec.tv_sec) * 1000000ULL + static_cast<uint64>(TimeSpec.tv_nsec) / 1000ULL);
}



////////////////////////////////////////////////////////////////////////////////
static bool TcpSocketSetNonBlocking(int Socket, bool bNonBlocking)
{
	int Flags = fcntl(Socket, F_GETFL, 0);
	if (Flags == -1)
	{
		return false;
	}

	Flags = bNonBlocking ? (Flags|O_NONBLOCK) : (Flags & ~O_NONBLOCK);
	return fcntl(Socket, F_SETFL, Flags) >= 0;
}

////////////////////////////////////////////////////////////////////////////////
UPTRINT TcpSocketConnect(const ANSICHAR* Host, uint16 Port)
{
	int Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (Socket < 0)
	{
		return 0;
	}

	sockaddr_in SockAddr;
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_addr.s_addr = inet_addr(Host);
	SockAddr.sin_port = htons(Port);

	int Result = connect(Socket, (sockaddr*)&SockAddr, sizeof(SockAddr));
	if (Result < 0)
	{
		close(Socket);
		return 0;
	}

	if (!TcpSocketSetNonBlocking(Socket, false))
	{
		close(Socket);
		return 0;
	}

	return UPTRINT(Socket + 1);
}

////////////////////////////////////////////////////////////////////////////////
UPTRINT TcpSocketListen(uint16 Port)
{
	int Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (Socket < 0)
	{
		return 0;
	}

	sockaddr_in SockAddr;
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_addr.s_addr = 0;
	SockAddr.sin_port = htons(Port);
	int Result = bind(Socket, reinterpret_cast<sockaddr*>(&SockAddr), sizeof(SockAddr));
	if (Result < 0)
	{
		close(Socket);
		return 0;
	}

	Result = listen(Socket, 1);
	if (Result < 0)
	{
		close(Socket);
		return 0;
	}

	if (!TcpSocketSetNonBlocking(Socket, true))
	{
		close(Socket);
		return 0;
	}

	return UPTRINT(Socket + 1);
}

////////////////////////////////////////////////////////////////////////////////
int32 TcpSocketAccept(UPTRINT Socket, UPTRINT& Out)
{
	int Inner = Socket - 1;

	Inner = accept(Inner, nullptr, nullptr);
	if (Inner < 0)
	{
		return (errno == EAGAIN || errno == EWOULDBLOCK) - 1; // 0 if would block else -1
	}

	if (!TcpSocketSetNonBlocking(Inner, false))
	{
		close(Inner);
		return 0;
	}

	Out = UPTRINT(Inner + 1);
	return 1;
}

////////////////////////////////////////////////////////////////////////////////
bool TcpSocketHasData(UPTRINT Socket)
{
	int Inner = Socket - 1;
	fd_set FdSet;
	FD_ZERO(&FdSet);
	FD_SET(Inner, &FdSet);
	timeval TimeVal = {};
	return (select(Inner + 1, &FdSet, nullptr, nullptr, &TimeVal) != 0);
}



////////////////////////////////////////////////////////////////////////////////
bool IoWrite(UPTRINT Handle, const void* Data, uint32 Size)
{
	int Inner = int(Handle) - 1;
	return write(Inner, Data, Size) == Size;
}

////////////////////////////////////////////////////////////////////////////////
int32 IoRead(UPTRINT Handle, void* Data, uint32 Size)
{
	int Inner = int(Handle) - 1;
	return read(Inner, Data, Size);
}

////////////////////////////////////////////////////////////////////////////////
void IoClose(UPTRINT Handle)
{
	int Inner = int(Handle) - 1;
	close(Inner);
}



////////////////////////////////////////////////////////////////////////////////
UPTRINT FileOpen(const ANSICHAR* Path)
{
	int Flags = O_CREAT|O_WRONLY|O_TRUNC;
	int Mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;

	int Out = open(Path, Flags, Mode);
	if (Out < 0)
	{
		return 0;
	}

	return UPTRINT(Out + 1);
}
	
////////////////////////////////////////////////////////////////////////////////
int32 GetLastErrorCode()
{
	return errno;
}

////////////////////////////////////////////////////////////////////////////////
bool GetErrorMessage(char* OutBuffer, uint32 BufferSize, int32 ErrorCode)
{
	const char* ErrorMessage = strerror(ErrorCode);
	const bool bResult = strncpy(OutBuffer, ErrorMessage, BufferSize) != 0;
	OutBuffer[BufferSize-1] = 0;
	return bResult;
}

} // namespace Private
} // namespace Trace
} // namespace UE

#endif // UE_TRACE_ENABLED

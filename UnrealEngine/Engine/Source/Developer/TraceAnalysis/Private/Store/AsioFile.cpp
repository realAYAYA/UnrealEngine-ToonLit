// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsioFile.h"
#include "Containers/StringConv.h"

#if !PLATFORM_WINDOWS
#include <sys/file.h>
#include <errno.h>
#endif

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
FAsioFile::FAsioFile(asio::io_context& IoContext, UPTRINT OsHandle)
#if PLATFORM_WINDOWS
: Handle(IoContext, HANDLE(OsHandle))
#else
: StreamDescriptor(IoContext, OsHandle)
#endif
{
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioFile::IsOpen() const
{
#if PLATFORM_WINDOWS
	return Handle.is_open();
#else
	return StreamDescriptor.is_open();
#endif
}

////////////////////////////////////////////////////////////////////////////////
void FAsioFile::Close()
{
#if PLATFORM_WINDOWS
	Handle.close();
#else
	StreamDescriptor.close();
#endif
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioFile::Write(const void* Src, uint32 Size, FAsioIoSink* Sink, uint32 Id)
{
	if (!SetSink(Sink, Id))
	{
		return false;
	}

#if PLATFORM_WINDOWS
	asio::async_write_at(
		Handle,
		Offset,
#else
	asio::async_write(
		StreamDescriptor,
#endif
		asio::buffer(Src, Size),
		[this] (const asio::error_code& ErrorCode, size_t BytesWritten)
		{
			Offset += BytesWritten;
			return OnIoComplete(ErrorCode, BytesWritten);
		}
	);

	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioFile::Read(void* Dest, uint32 Size, FAsioIoSink* Sink, uint32 Id)
{
	return ReadSome(Dest, Size, Sink, Id);
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioFile::ReadSome(void* Dest, uint32 DestSize, FAsioIoSink* Sink, uint32 Id)
{
	if (!SetSink(Sink, Id))
	{
		return false;
	}

#if PLATFORM_WINDOWS
	Handle.async_read_some_at(
		Offset,
#else
	StreamDescriptor.async_read_some(
#endif
		asio::buffer(Dest, DestSize),
		[this] (const asio::error_code& ErrorCode, size_t BytesRead)
		{
			Offset += BytesRead;
			return OnIoComplete(ErrorCode, BytesRead);
		}
	);

	return true;
}

////////////////////////////////////////////////////////////////////////////////
FAsioWriteable* FAsioFile::WriteFile(asio::io_context& IoContext, const TCHAR* Path)
{
#if PLATFORM_WINDOWS
	HANDLE Handle = CreateFileW(Path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
		CREATE_ALWAYS, FILE_FLAG_OVERLAPPED|FILE_ATTRIBUTE_NORMAL, nullptr);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		return nullptr;
	}
	return new FAsioFile(IoContext, UPTRINT(Handle));
#else
	int File = open(TCHAR_TO_ANSI(Path), O_WRONLY|O_CREAT, 0666);
	if (!File)
	{
		return nullptr;
	}
	return new FAsioFile(IoContext, UPTRINT(File));
#endif
}

////////////////////////////////////////////////////////////////////////////////
FAsioReadable* FAsioFile::ReadFile(asio::io_context& IoContext, const TCHAR* Path)
{
#if PLATFORM_WINDOWS
	HANDLE Handle = CreateFileW(Path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
		nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		return nullptr;
	}
	return new FAsioFile(IoContext, UPTRINT(Handle));
#else
	int File = open(TCHAR_TO_ANSI(Path), O_RDONLY, 0444);
	if (!File)
	{
		return nullptr;
	}
	return new FAsioFile(IoContext, UPTRINT(File));
#endif
}

} // namespace Trace
} // namespace UE

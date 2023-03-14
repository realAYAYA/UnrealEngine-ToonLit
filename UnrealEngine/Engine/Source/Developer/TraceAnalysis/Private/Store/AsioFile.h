// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Asio/Asio.h"

#include "AsioIoable.h"
#if !PLATFORM_WINDOWS
#include "asio/posix/stream_descriptor.hpp"
#endif

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
class FAsioFile
	: public FAsioReadable
	, public FAsioWriteable
{
public:
										FAsioFile(asio::io_context& IoContext, UPTRINT OsHandle);
	static FAsioWriteable*				WriteFile(asio::io_context& IoContext, const TCHAR* Path);
	static FAsioReadable*				ReadFile(asio::io_context& IoContext, const TCHAR* Path);
	virtual bool						IsOpen() const override;
	virtual void						Close() override;
	virtual bool						Write(const void* Src, uint32 Size, FAsioIoSink* Sink, uint32 Id) override;
	virtual bool						Read(void* Dest, uint32 Size, FAsioIoSink* Sink, uint32 Id) override;
	virtual bool						ReadSome(void* Dest, uint32 BufferSize, FAsioIoSink* Sink, uint32 Id) override;

private:
#if PLATFORM_WINDOWS
	asio::windows::random_access_handle	Handle;
#else
	asio::posix::stream_descriptor		StreamDescriptor;
#endif
	uint64								Offset = 0;
};

} // namespace Trace
} // namespace UE

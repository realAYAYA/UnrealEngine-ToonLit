// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Asio/Asio.h"
#include "AsioIoable.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
class FAsioSocket
	: public FAsioReadable
	, public FAsioWriteable
{
public:
							FAsioSocket(asio::ip::tcp::socket& InSocket);
	virtual					~FAsioSocket();
	asio::io_context&		GetIoContext();
	virtual bool			IsOpen() const override;
	virtual void			Close() override;
	uint32					GetRemoteAddress() const;
	uint32					GetRemotePort() const;
	uint32					GetLocalPort() const;
	virtual bool			Read(void* Dest, uint32 Size, FAsioIoSink* Sink, uint32 Id) override;
	virtual bool			ReadSome(void* Dest, uint32 DestSize, FAsioIoSink* Sink, uint32 Id) override;
	virtual bool			Write(const void* Src, uint32 Size, FAsioIoSink* Sink, uint32 Id) override;

private:
	asio::ip::tcp::socket	Socket;
};

} // namespace Trace
} // namespace UE

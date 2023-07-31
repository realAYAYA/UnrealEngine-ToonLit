// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Asio/Asio.h"
#include "AsioIoable.h"
#include "AsioTcpServer.h"
#include "AsioTickable.h"

namespace UE {
namespace Trace {

class FAsioRecorder;
class FAsioSocket;

////////////////////////////////////////////////////////////////////////////////
class FAsioTraceRelay
	: public FAsioTcpServer
	, public FAsioIoSink
	, public FAsioTickable
{
public:
						FAsioTraceRelay(asio::io_context& IoContext, FAsioReadable* InInput, uint32 InTraceIf, FAsioRecorder& InRecorder);
						~FAsioTraceRelay();
	bool				IsOpen() const;
	void				Close();

private:
	virtual bool		OnAccept(asio::ip::tcp::socket& Socket) override;
	virtual void		OnTick() override;
	virtual void		OnIoComplete(uint32 Id, int32 Size) override;
	enum				{ OpStart, OpRead, OpSend };
	static const uint32	BufferSize = 64 << 10;
	FAsioReadable*		Input;
	FAsioSocket*		Output = nullptr;
	FAsioRecorder&		Recorder;
	uint32				SessionId;
	uint8				Buffer[BufferSize];
};

} // namespace Trace
} // namespace UE

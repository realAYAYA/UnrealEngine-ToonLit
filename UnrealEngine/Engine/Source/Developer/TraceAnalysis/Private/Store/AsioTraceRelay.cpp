// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsioTraceRelay.h"
#include "AsioRecorder.h"
#include "AsioSocket.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
FAsioTraceRelay::FAsioTraceRelay(
	asio::io_context& IoContext,
	FAsioReadable* InInput,
	uint32 InSessionId,
	FAsioRecorder& InRecorder)
: FAsioTcpServer(IoContext)
, FAsioTickable(IoContext)
, Input(InInput)
, Recorder(InRecorder)
, SessionId(InSessionId)
{
	StartServer();
	TickOnce(3000);
}

////////////////////////////////////////////////////////////////////////////////
FAsioTraceRelay::~FAsioTraceRelay()
{
	check(!Input->IsOpen());
	delete Input;

	if (Output != nullptr)
	{
		check(!Output->IsOpen());
		delete Output;
	}
}

////////////////////////////////////////////////////////////////////////////////
void FAsioTraceRelay::Close()
{
	if (Output != nullptr)
	{
		Output->Close();
	}

	Input->Close();

	FAsioTcpServer::Close();
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioTraceRelay::IsOpen() const
{
	if (FAsioTcpServer::IsOpen())
	{
		return true;
	}

	bool bOpen = Input->IsOpen();
	if (Output != nullptr)
	{
		bOpen &= Output->IsOpen();
	}

	return bOpen;
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioTraceRelay::OnAccept(asio::ip::tcp::socket& Socket)
{
	Output = new FAsioSocket(Socket);
	StopTick();
	OnIoComplete(OpStart, 0);
	return false;
}

////////////////////////////////////////////////////////////////////////////////
void FAsioTraceRelay::OnTick()
{
	if (FAsioTcpServer::IsOpen())
	{
		Close();
		return;
	}

	return OnIoComplete(OpStart, 0);
}

////////////////////////////////////////////////////////////////////////////////
void FAsioTraceRelay::OnIoComplete(uint32 Id, int32 Size)
{
	if (Size < 0)
	{
		if (Id == OpRead && SessionId && -Size == asio::error::eof)
		{
			for (int i = 0, n = Recorder.GetSessionCount(); i < n; ++i)
			{
				const FAsioRecorder::FSession* Session = Recorder.GetSessionInfo(i);
				if (Session->GetId() == SessionId)
				{
					TickOnce(200);
					return;
				}
			}
		}

		Output->Close();
		Input->Close();
		return;
	}

	switch (Id)
	{
	case OpStart:
	case OpSend:
		Input->Read(Buffer, BufferSize, this, OpRead);
		break;

	case OpRead:
		Output->Write(Buffer, Size, this, OpSend);
		break;
	}
}

} // namespace Trace
} // namespace UE

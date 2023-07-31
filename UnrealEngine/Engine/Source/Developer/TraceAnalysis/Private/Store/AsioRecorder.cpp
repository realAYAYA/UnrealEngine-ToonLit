// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsioRecorder.h"
#include "AsioIoable.h"
#include "AsioSocket.h"
#include "AsioStore.h"

#if PLATFORM_WINDOWS
	THIRD_PARTY_INCLUDES_START
#	include "Windows/AllowWindowsPlatformTypes.h"
#		include <Mstcpip.h>
#	include "Windows/HideWindowsPlatformTypes.h"
	THIRD_PARTY_INCLUDES_END
#endif

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
class FAsioRecorderRelay
	: public FAsioIoSink
{
public:
						FAsioRecorderRelay(asio::ip::tcp::socket& Socket, FAsioWriteable* InOutput);
	virtual				~FAsioRecorderRelay();
	bool				IsOpen();
	void				Close();
	uint32				GetIpAddress() const;
	uint32				GetControlPort() const;

private:
	virtual void		OnIoComplete(uint32 Id, int32 Size) override;
	bool				ReadMetadata(int32 Size);
	static const uint32	BufferSize = 64 * 1024;
	enum				{ OpStart, OpSocketReadMetadata, OpSocketRead, OpFileWrite };
	FAsioSocket			Input;
	FAsioWriteable*		Output;
	uint32				ActiveReadOp = OpSocketReadMetadata;
	uint16				ControlPort = 0;
	uint8				Buffer[BufferSize];
};

////////////////////////////////////////////////////////////////////////////////
FAsioRecorderRelay::FAsioRecorderRelay(asio::ip::tcp::socket& Socket, FAsioWriteable* InOutput)
: Input(Socket)
, Output(InOutput)
{
	OnIoComplete(OpStart, 0);
}

////////////////////////////////////////////////////////////////////////////////
FAsioRecorderRelay::~FAsioRecorderRelay()
{
	check(!Input.IsOpen());
	check(!Output->IsOpen());
	delete Output;
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioRecorderRelay::IsOpen()
{
	return Input.IsOpen();
}

////////////////////////////////////////////////////////////////////////////////
void FAsioRecorderRelay::Close()
{
	Input.Close();
	Output->Close();
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioRecorderRelay::GetIpAddress() const
{
	return Input.GetRemoteAddress();
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioRecorderRelay::GetControlPort() const
{
	return ControlPort;
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioRecorderRelay::ReadMetadata(int32 Size)
{
	const uint8* Cursor = Buffer;
	auto Read = [&] (int32 SizeToRead)
	{
		const uint8* Ptr = Cursor;
		Cursor += SizeToRead;
		Size -= SizeToRead;
		return Ptr;
	};

	// Stream header
	uint32 Magic;
	if (Size < sizeof(Magic))
	{
		return true;
	}

	Magic = *(const uint32*)(Read(sizeof(Magic)));
	switch (Magic)
	{
	case 'TRC2':		/* trace with metadata data */
		break;

	case 'TRCE':		/* valid, but to old or wrong endian for us */
	case 'ECRT':
	case '2CRT':
		return true;

	default:			/* unexpected magic */
		return false;
	}

	// MetadataSize field
	if (Size < 2)
	{
		return true;
	}
	Size = *(const uint16*)(Read(2));

	// MetadataFields
	while (Size >= 2)
	{
		struct {
			uint8	Size;
			uint8	Id;
		} MetadataField;
		MetadataField = *(const decltype(MetadataField)*)(Read(sizeof(MetadataField)));

		if (Size < MetadataField.Size)
		{
			break;
		}

		if (MetadataField.Id == 0) /* ControlPortFieldId */
		{
			ControlPort = *(const uint16*)Cursor;
		}

		Size -= MetadataField.Size;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
void FAsioRecorderRelay::OnIoComplete(uint32 Id, int32 Size)
{
	if (Size < 0)
	{
		Close();
		return;
	}

	switch (Id)
	{
	case OpSocketReadMetadata:
		ActiveReadOp = OpSocketRead;
		if (!ReadMetadata(Size))
		{
			Close();
			return;
		}
		/* fallthrough */

	case OpSocketRead:
		Output->Write(Buffer, Size, this, OpFileWrite);
		break;

	case OpStart:
	case OpFileWrite:
		Input.ReadSome(Buffer, BufferSize, this, ActiveReadOp);
		break;
	}
}



////////////////////////////////////////////////////////////////////////////////
uint32 FAsioRecorder::FSession::GetId() const
{
	return Id;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioRecorder::FSession::GetTraceId() const
{
	return TraceId;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioRecorder::FSession::GetIpAddress() const
{
	return Relay->GetIpAddress();
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioRecorder::FSession::GetControlPort() const
{
	return Relay->GetControlPort();
}



////////////////////////////////////////////////////////////////////////////////
FAsioRecorder::FAsioRecorder(asio::io_context& IoContext, FAsioStore& InStore)
: FAsioTcpServer(IoContext)
, FAsioTickable(IoContext)
, Store(InStore)
{
	StartTick(500);
}

////////////////////////////////////////////////////////////////////////////////
FAsioRecorder::~FAsioRecorder()
{
	check(!FAsioTickable::IsActive());
	check(!FAsioTcpServer::IsOpen());

	for (FSession& Session : Sessions)
	{
		delete Session.Relay;
	}
}

////////////////////////////////////////////////////////////////////////////////
void FAsioRecorder::Close()
{
	FAsioTickable::StopTick();
	FAsioTcpServer::Close();

	for (FSession& Session : Sessions)
	{
		Session.Relay->Close();
	}
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioRecorder::GetSessionCount() const
{
	return uint32(Sessions.Num());
}

////////////////////////////////////////////////////////////////////////////////
const FAsioRecorder::FSession* FAsioRecorder::GetSessionInfo(uint32 Index) const
{
	if (Index >= uint32(Sessions.Num()))
	{
		return nullptr;
	}

	return Sessions.GetData() + Index;
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioRecorder::OnAccept(asio::ip::tcp::socket& Socket)
{
	FAsioStore::FNewTrace Trace = Store.CreateTrace();
	if (Trace.Writeable == nullptr)
	{
		return true;
	}

#if PLATFORM_WINDOWS
	// Trace data is a stream and communication is one way. It is implemented
	// this way to share code between sending trace data over the wire and writing
	// it to a file. Because there's no ping/pong we can end up with a half-open
	// TCP connection if the other end doesn't close its socket. So we'll enable
	// keep-alive on the socket and set a short timeout (default is 2hrs).
	tcp_keepalive KeepAlive =
	{
		1,		// on
		15000,	// timeout_ms
		2000,	// interval_ms
	};

	DWORD BytesReturned;
	WSAIoctl(
		Socket.native_handle(),
		SIO_KEEPALIVE_VALS,
		&KeepAlive, sizeof(KeepAlive),
		nullptr, 0,
		&BytesReturned,
		nullptr,
		nullptr
	);
#endif

	auto* Relay = new FAsioRecorderRelay(Socket, Trace.Writeable);

	uint32 IdPieces[] = {
		Relay->GetIpAddress(),
		Socket.remote_endpoint().port(),
		Socket.local_endpoint().port(),
		0,
	};

	FSession Session;
	Session.Relay = Relay;
	Session.Id = QuickStoreHash(IdPieces);
	Session.TraceId = Trace.Id;
	Sessions.Add(Session);

	return true;
}

////////////////////////////////////////////////////////////////////////////////
void FAsioRecorder::OnTick()
{
	uint32 FinalNum = 0;
	for (int i = 0, n = Sessions.Num(); i < n; ++i)
	{
		FSession& Session = Sessions[i];
		if (Session.Relay->IsOpen())
		{
			Sessions[FinalNum] = Session;
			++FinalNum;
			continue;
		}

		delete Session.Relay;
	}

	Sessions.SetNum(FinalNum);
}

} // namespace Trace
} // namespace UE

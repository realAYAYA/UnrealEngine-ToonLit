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
						FAsioRecorderRelay(asio::ip::tcp::socket& Socket, FAsioStore& InStore);
	virtual				~FAsioRecorderRelay();
	bool				IsOpen();
	void				Close();
	uint32				GetTraceId() const;
	uint32				GetIpAddress() const;
	uint32				GetControlPort() const;

private:
	virtual void		OnIoComplete(uint32 Id, int32 Size) override;
	bool				CreateTrace();
	bool				ReadMagic();
	bool				ReadMetadata(int32 Size);
	static const uint32	BufferSize = 64 * 1024;
	FAsioSocket			Input;
	FAsioWriteable*		Output = nullptr;
	FAsioStore&			Store;
	uint8*				PreambleCursor;
	uint32				TraceId = 0;
	uint16				ControlPort = 0;
	uint8				Buffer[BufferSize];

	enum
	{
		OpMagicRead,
		OpMetadataRead,
		OpSocketRead,
		OpFileWrite,
	};

	using MagicType				= uint32;
	using MetadataSizeType		= uint16;
	using VersionType			= struct { uint8 Transport; uint8 Protocol; };

	static_assert(sizeof(VersionType) == 2, "Unexpected struct size");
};

////////////////////////////////////////////////////////////////////////////////
FAsioRecorderRelay::FAsioRecorderRelay(asio::ip::tcp::socket& Socket, FAsioStore& InStore)
: Input(Socket)
, Store(InStore)
{
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

	// Kick things off by reading the magic four bytes at the start of the stream
	// along with an additional two bytes that are likely the metadata size.
	uint32 PreambleReadSize = sizeof(MagicType) + sizeof(MetadataSizeType);
	PreambleCursor = Buffer + PreambleReadSize;
	Input.Read(Buffer, PreambleReadSize, this, OpMagicRead);
}

////////////////////////////////////////////////////////////////////////////////
FAsioRecorderRelay::~FAsioRecorderRelay()
{
	check(!Input.IsOpen());
	if (Output != nullptr)
	{
		check(!Output->IsOpen());
		delete Output;
	}
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
	if (Output != nullptr)
	{
		Output->Close();
	}
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioRecorderRelay::GetTraceId() const
{
	return TraceId;
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
bool FAsioRecorderRelay::CreateTrace()
{
	FAsioStore::FNewTrace Trace = Store.CreateTrace();
	TraceId = Trace.Id;
	Output = Trace.Writeable;
	return (Output != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioRecorderRelay::ReadMagic()
{
	// Here we'll check the magic four bytes at the start of the stream and create
	// a trace to write into if they are bytes we're expecting.

	// We will only support clients that send the magic. Very early clients did
	// not do this but they were unreleased and should no longer be in use.
	if (Buffer[3] != 'T' || Buffer[2] != 'R' || Buffer[1] != 'C')
	{
		return false;
	}

	// We can continue to support very old clients
	if (Buffer[0] == 'E')
	{
		if (CreateTrace())
		{
			// Old clients have no metadata so we can go straight into the
			// read-write loop. We've already got read data in Buffer.
			Output->Write(Buffer, sizeof(MagicType), this, OpFileWrite);
			return true;
		}
		return false;
	}

	// Later clients have a metadata block (TRC2). There's loose support for the
	// future too if need be (TRC[3-9]).
	if (Buffer[0] < '2' || Buffer[0] > '9')
	{
		return false;
	}

	// Concatenate metadata into the buffer, first validating the given size is
	// one that we can handle in a single read.
	uint32 MetadataSize = *(MetadataSizeType*)(Buffer + sizeof(MagicType));
	MetadataSize += sizeof(VersionType);
	if (MetadataSize > BufferSize - uint32(ptrdiff_t(PreambleCursor - Buffer)))
	{
		return false;
	}

	Input.Read(PreambleCursor, MetadataSize, this, OpMetadataRead);

	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioRecorderRelay::ReadMetadata(int32 Size)
{
	// At this point Buffer          [magic][md_size][metadata][t_ver][p_ver]
	// looks like this;              Buffer--------->PreambleCursor--------->
	//                                               |---------Size---------|

	// We want to consume [metadata] so some adjustment is required.
	int32 ReadSize = Size - sizeof(VersionType);
	const uint8* Cursor = PreambleCursor;
	auto Read = [&] (int32 SizeToRead)
	{
		const uint8* Ptr = Cursor;
		Cursor += SizeToRead;
		ReadSize -= SizeToRead;
		return Ptr;
	};

	// MetadataFields
	while (ReadSize > 0)
	{
		struct {
			uint8	Size;
			uint8	Id;
		} MetadataField;
		MetadataField = *(const decltype(MetadataField)*)(Read(sizeof(MetadataField)));

		if (ReadSize < MetadataField.Size)
		{
			return false;
		}

		if (MetadataField.Id == 0) /* ControlPortFieldId */
		{
			ControlPort = *(const uint16*)Cursor;
		}

		ReadSize -= MetadataField.Size;
	}

	// There should be no data left to consume if the metadata was well-formed
	if (ReadSize != 0)
	{
		return false;
	}

	// Now we've a full preamble we are ready to write the trace.
	if (!CreateTrace())
	{
		return false;
	}

	// Analysis needs the preamble too.
	uint32 PreambleSize = uint32(ptrdiff_t(PreambleCursor - Buffer)) + Size;
	Output->Write(Buffer, PreambleSize, this, OpFileWrite);

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
	case OpMagicRead:
		if (!ReadMagic())
		{
			Close();
		}
		break;

	case OpMetadataRead:
		if (!ReadMetadata(Size))
		{
			Close();
		}
		break;

	case OpSocketRead:
		Output->Write(Buffer, Size, this, OpFileWrite);
		break;

	case OpFileWrite:
		Input.ReadSome(Buffer, BufferSize, this, OpSocketRead);
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
	return Relay->GetTraceId();
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
	auto* Relay = new FAsioRecorderRelay(Socket, Store);

	uint32 IdPieces[] = {
		Relay->GetIpAddress(),
		Socket.remote_endpoint().port(),
		Socket.local_endpoint().port(),
		0,
	};

	FSession Session;
	Session.Relay = Relay;
	Session.Id = QuickStoreHash(IdPieces);
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

// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsioStoreCborServer.h"
#include "AsioIoable.h"
#include "AsioRecorder.h"
#include "AsioSocket.h"
#include "AsioStore.h"
#include "AsioTraceRelay.h"
#include "CborPayload.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
class FAsioStoreCborPeer
	: public FAsioIoSink
{
public:
							FAsioStoreCborPeer(asio::ip::tcp::socket& InSocket, FAsioStore& InStore, FAsioRecorder& InRecorder, FAsioStoreCborServer& InParent);
	virtual					~FAsioStoreCborPeer();
	bool					IsOpen() const;
	void					Close();

protected:
	void					OnPayload();
	void					OnSessionCount();
	void					OnSessionInfo();
	void					OnStatus();
	void					OnTraceCount();
	void					OnTraceInfo();
	void					OnTraceRead();
	void					SendError(EStatusCode StatusCode);
	void					SendResponse(const FPayload& Payload);
	virtual void			OnIoComplete(uint32 Id, int32 Size) override;
	enum					{ OpStart, OpReadPayloadSize, OpReadPayload, OpSendResponse };
	FAsioStore&				Store;
	FAsioRecorder&			Recorder;
	FAsioStoreCborServer&	Parent;
	FAsioSocket				Socket;
	uint32					PayloadSize;
	FResponse				Response;
};

////////////////////////////////////////////////////////////////////////////////
FAsioStoreCborPeer::FAsioStoreCborPeer(
	asio::ip::tcp::socket& InSocket,
	FAsioStore& InStore,
	FAsioRecorder& InRecorder,
	FAsioStoreCborServer& InParent)
: Store(InStore)
, Recorder(InRecorder)
, Parent(InParent)
, Socket(InSocket)
{
	OnIoComplete(OpStart, 0);
}

////////////////////////////////////////////////////////////////////////////////
FAsioStoreCborPeer::~FAsioStoreCborPeer()
{
	check(!IsOpen());
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioStoreCborPeer::IsOpen() const
{
	return Socket.IsOpen();
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::Close()
{
	Socket.Close();
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::OnSessionCount()
{
	TPayloadBuilder<> Builder(EStatusCode::Success);
	Builder.AddInteger("count", Recorder.GetSessionCount());
	SendResponse(Builder.Done());
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::OnSessionInfo()
{
	const FAsioRecorder::FSession* Session = nullptr;

	int32 Index = int32(Response.GetInteger("index", -1));
	if (Index >= 0)
	{
		Session = Recorder.GetSessionInfo(Index);
	}
	else if (uint32 Id = uint32(Response.GetInteger("id", 0)))
	{
		for (int i = 0, n = Recorder.GetSessionCount(); i < n; ++i)
		{
			const FAsioRecorder::FSession* Candidate = Recorder.GetSessionInfo(i);
			if (Candidate->GetId() == Id)
			{
				Session = Candidate;
				break;
			}
		}
	}
	else if (uint32 TraceId = uint32(Response.GetInteger("trace_id", 0)))
	{
		for (int i = 0, n = Recorder.GetSessionCount(); i < n; ++i)
		{
			const FAsioRecorder::FSession* Candidate = Recorder.GetSessionInfo(i);
			if (Candidate->GetTraceId() == TraceId)
			{
				Session = Candidate;
				break;
			}
		}
	}

	if (Session == nullptr)
	{
		return SendError(EStatusCode::BadRequest);
	}

	TPayloadBuilder<> Builder(EStatusCode::Success);
	Builder.AddInteger("id", Session->GetId());
	Builder.AddInteger("trace_id", Session->GetTraceId());
	Builder.AddInteger("ip_address", Session->GetIpAddress());
	Builder.AddInteger("control_port", Session->GetControlPort());
	SendResponse(Builder.Done());
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::OnStatus()
{
	TPayloadBuilder<> Builder(EStatusCode::Success);
	Builder.AddString("store_dir", TCHAR_TO_UTF8(Store.GetStoreDir()));
	Builder.AddInteger("recorder_port", Recorder.GetPort());
	Builder.AddInteger("change_serial", Store.GetChangeSerial());
	SendResponse(Builder.Done());
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::OnTraceCount()
{
	TPayloadBuilder<> Builder(EStatusCode::Success);
	Builder.AddInteger("count", Store.GetTraceCount());
	SendResponse(Builder.Done());
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::OnTraceInfo()
{
	const FAsioStore::FTrace* Trace = nullptr;

	int32 Index = int32(Response.GetInteger("index", -1));
	if (Index >= 0)
	{
		Trace = Store.GetTraceInfo(Index);
	}
	else
	{
		uint32 Id = uint32(Response.GetInteger("id", 0));
		if (Id != 0)
		{
			for (int i = 0, n = Store.GetTraceCount(); i < n; ++i)
			{
				const FAsioStore::FTrace* Candidate = Store.GetTraceInfo(i);
				if (Candidate->GetId() == Id)
				{
					Trace = Candidate;
					break;
				}
			}
		}
	}

	if (Trace == nullptr)
	{
		return SendError(EStatusCode::BadRequest);
	}

	char OutName[256];
	const FStringView& Name = Trace->GetName();
	uint32 NameLength = FMath::Min(uint32(sizeof(OutName)), uint32(Name.Len()));
	for (uint32 i = 0; i < NameLength; ++i)
	{
		OutName[i] = char(Name[i]);
	}

	TPayloadBuilder<> Builder(EStatusCode::Success);
	Builder.AddInteger("id", Trace->GetId());
	Builder.AddInteger("size", Trace->GetSize());
	Builder.AddInteger("timestamp", Trace->GetTimestamp());
	Builder.AddString("name", OutName, NameLength);
	SendResponse(Builder.Done());
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::OnTraceRead()
{
	uint32 Id = uint32(Response.GetInteger("id", 0));
	if (!Id || !Store.HasTrace(Id))
	{
		return SendError(EStatusCode::BadRequest);
	}

	FAsioTraceRelay* Relay = Parent.RelayTrace(Id);
	if (Relay == nullptr)
	{
		return SendError(EStatusCode::InternalError);
	}

	TPayloadBuilder<> Builder(EStatusCode::Success);
	Builder.AddInteger("port", Relay->GetPort());
	SendResponse(Builder.Done());
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::OnPayload()
{
	FUtf8StringView Request = Response.GetString("$request", "");
	FUtf8StringView Path = Response.GetString("$path", "");
	if (!Request.Len() || !Path.Len())
	{
		SendError(EStatusCode::BadRequest);
		return;
	}

	static struct {
		uint32	Hash;
		void	(FAsioStoreCborPeer::*Func)();
	} const DispatchTable[] = {
		{ QuickStoreHash("v1/session/count"),	&FAsioStoreCborPeer::OnSessionCount },
		{ QuickStoreHash("v1/session/info"),	&FAsioStoreCborPeer::OnSessionInfo },
		{ QuickStoreHash("v1/status"),			&FAsioStoreCborPeer::OnStatus },
		{ QuickStoreHash("v1/trace/count"),		&FAsioStoreCborPeer::OnTraceCount },
		{ QuickStoreHash("v1/trace/info"),		&FAsioStoreCborPeer::OnTraceInfo },
		{ QuickStoreHash("v1/trace/read"),		&FAsioStoreCborPeer::OnTraceRead },
	};

	uint32 PathHash = QuickStoreHash(Path);
	for (const auto& Row : DispatchTable)
	{
		if (Row.Hash == PathHash)
		{
			return (this->*(Row.Func))();
		}
	}

	SendError(EStatusCode::NotFound);
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::SendError(EStatusCode StatusCode)
{
	TPayloadBuilder<> Builder(StatusCode);
	FPayload Payload = Builder.Done();
	SendResponse(Payload);
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::SendResponse(const FPayload& Payload)
{
	struct {
		uint32	Size;
		uint8	Data[];
	}* Dest;

	Dest = decltype(Dest)(Response.Reserve(sizeof(*Dest) + Payload.Size));
	Dest->Size = Payload.Size;
	memcpy(Dest->Data, Payload.Data, Payload.Size);
	Socket.Write(Dest, sizeof(*Dest) + Payload.Size, this, OpSendResponse);
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborPeer::OnIoComplete(uint32 Id, int32 Size)
{
	if (Size < 0)
	{
		Close();
		return;
	}

	switch (Id)
	{
	case OpStart:
	case OpSendResponse:
		Socket.Read(&PayloadSize, sizeof(uint32), this, OpReadPayloadSize);
		break;

	case OpReadPayloadSize:
		if (PayloadSize == 0 || PayloadSize > 1024)
		{
			SendError(EStatusCode::BadRequest);
			break;
		}
		Socket.Read(Response.Reserve(PayloadSize), PayloadSize, this, OpReadPayload);
		break;

	case OpReadPayload:
		OnPayload();
		break;
	}
}



////////////////////////////////////////////////////////////////////////////////
FAsioStoreCborServer::FAsioStoreCborServer(
	asio::io_context& IoContext,
	FAsioStore& InStore,
	FAsioRecorder& InRecorder)
: FAsioTcpServer(IoContext)
, FAsioTickable(IoContext)
, Store(InStore)
, Recorder(InRecorder)
{
	if (!StartServer(1988))
	{
		StartServer();
	}
	StartTick(500);
}

////////////////////////////////////////////////////////////////////////////////
FAsioStoreCborServer::~FAsioStoreCborServer()
{
	check(!FAsioTcpServer::IsOpen());
	check(!FAsioTickable::IsActive());

	for (FAsioStoreCborPeer* Peer : Peers)
	{
		delete Peer;
	}

	for (FAsioTraceRelay* Relay : Relays)
	{
		delete Relay;
	}
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborServer::Close()
{
	FAsioTcpServer::Close();
	FAsioTickable::StopTick();

	for (FAsioStoreCborPeer* Peer : Peers)
	{
		Peer->Close();
	}

	for (FAsioTraceRelay* Relay : Relays)
	{
		Relay->Close();
	}
}

////////////////////////////////////////////////////////////////////////////////
FAsioStore& FAsioStoreCborServer::GetStore() const
{
	return Store;
}

////////////////////////////////////////////////////////////////////////////////
FAsioRecorder& FAsioStoreCborServer::GetRecorder() const
{
	return Recorder;
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioStoreCborServer::OnAccept(asio::ip::tcp::socket& Socket)
{
	FAsioStoreCborPeer* Peer = new FAsioStoreCborPeer(Socket, Store, Recorder, *this);
	Peers.Add(Peer);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
void FAsioStoreCborServer::OnTick()
{
	// Clean up dead peers
	uint32 FinalNum = 0;
	for (int i = 0, n = Peers.Num(); i < n; ++i)
	{
		FAsioStoreCborPeer* Peer = Peers[i];
		if (Peer->IsOpen())
		{
			Peers[FinalNum] = Peer;
			++FinalNum;
			continue;
		}

		delete Peer;
	}
	Peers.SetNum(FinalNum);

	// Clean up dead relays
	FinalNum = 0;
	for (int i = 0, n = Relays.Num(); i < n; ++i)
	{
		FAsioTraceRelay* Relay = Relays[i];
		if (Relay->IsOpen())
		{
			Relays[FinalNum] = Relay;
			++FinalNum;
			continue;
		}

		delete Relay;
	}
	Relays.SetNum(FinalNum);
}

////////////////////////////////////////////////////////////////////////////////
FAsioTraceRelay* FAsioStoreCborServer::RelayTrace(uint32 Id)
{
	FAsioReadable* Input = Store.OpenTrace(Id);
	if (Input == nullptr)
	{
		return nullptr;
	}

	uint32 SessionId = 0;
	for (int i = 0, n = Recorder.GetSessionCount(); i < n; ++i)
	{
		const FAsioRecorder::FSession* Session = Recorder.GetSessionInfo(i);
		if (Session->GetTraceId() == Id)
		{
			SessionId = Session->GetId();
			break;
		}
	}

	asio::io_context& IoContext = FAsioTcpServer::GetIoContext();
	FAsioTraceRelay* Relay = new FAsioTraceRelay(IoContext, Input, SessionId, Recorder);
	if (Relay->GetPort() == 0)
	{
		delete Relay;
		return nullptr;
	}

	Relays.Add(Relay);
	return Relay;
}

} // namespace Trace
} // namespace UE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/StoreClient.h"
#include "Asio/Asio.h"
#include "AsioStore.h"
#include "CborPayload.h"
#include "Templates/UnrealTemplate.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
class FTraceDataStream
	: public IInDataStream
{
public:
							FTraceDataStream(asio::ip::tcp::socket& InSocket);
	virtual					~FTraceDataStream();
	bool					IsOpen() const;
	virtual void			Close() override;
	virtual int32			Read(void* Dest, uint32 DestSize) override;

private:
	asio::ip::tcp::socket	Socket;
};

////////////////////////////////////////////////////////////////////////////////
FTraceDataStream::FTraceDataStream(asio::ip::tcp::socket& InSocket)
: Socket(MoveTemp(InSocket))
{
	asio::socket_base::receive_buffer_size RecvBufferSize(4 << 20);
	Socket.set_option(RecvBufferSize);
}

////////////////////////////////////////////////////////////////////////////////
FTraceDataStream::~FTraceDataStream()
{
	Close();
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceDataStream::IsOpen() const
{
	return Socket.is_open();
}

////////////////////////////////////////////////////////////////////////////////
void FTraceDataStream::Close()
{
	Socket.shutdown(asio::ip::tcp::socket::shutdown_receive);
	Socket.close();
}

////////////////////////////////////////////////////////////////////////////////
int32 FTraceDataStream::Read(void* Dest, uint32 DestSize)
{
	auto Handle = Socket.native_handle();

	fd_set Fds;
	FD_ZERO(&Fds);
	FD_SET(Handle, &Fds);

	timeval Timeout;
	Timeout.tv_sec = 1;
	Timeout.tv_usec = 0;

	while (true)
	{
		fd_set ReadFds = Fds;
		int Ret = select((int)Handle + 1, &ReadFds, 0, 0, &Timeout);

		if (Ret < 0)
		{
			Close();
			return -1;
		}

		if (Ret == 0)
		{
			continue;
		}

		asio::error_code ErrorCode;
		size_t BytesRead = Socket.read_some(asio::buffer(Dest, DestSize), ErrorCode);
		if (ErrorCode)
		{
			Close();
			return -1;
		}

		return int32(BytesRead);
	}
}



////////////////////////////////////////////////////////////////////////////////
class FStoreCborClient
{
public:
							FStoreCborClient(asio::io_context& InIoContext);
							~FStoreCborClient();
	bool					IsOpen() const;
	void					Close();
	uint32					GetStoreAddress() const;
	uint32					GetStorePort() const;
	const FResponse&		GetResponse() const;
	bool					Connect(const TCHAR* Host, uint16 Port);
	bool					GetStatus();
	bool					GetTraceCount();
	bool					GetTraceInfo(uint32 Index);
	bool					GetTraceInfoById(uint32 Id);
	FTraceDataStream*		ReadTrace(uint32 Id);
	bool					GetSessionCount();
	bool					GetSessionInfo(uint32 Index);
	bool					GetSessionInfoById(uint32 Id);
	bool					GetSessionInfoByTraceId(uint32 TraceId);

private:
	bool					Communicate(const FPayload& Payload);
	asio::io_context&		IoContext;
	asio::ip::tcp::socket	Socket;
	FResponse				Response;
};

////////////////////////////////////////////////////////////////////////////////
FStoreCborClient::FStoreCborClient(asio::io_context& InIoContext)
: IoContext(InIoContext)
, Socket(InIoContext)
{
}

////////////////////////////////////////////////////////////////////////////////
FStoreCborClient::~FStoreCborClient()
{
	Close();
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::IsOpen() const
{
	return Socket.is_open();
}

////////////////////////////////////////////////////////////////////////////////
void FStoreCborClient::Close()
{
	Socket.close();
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreCborClient::GetStoreAddress() const
{
	if (!IsOpen())
	{
		return 0;
	}

	const asio::ip::tcp::endpoint Endpoint = Socket.remote_endpoint();
	asio::ip::address Address = Endpoint.address();
	return Address.is_v4() ? Address.to_v4().to_uint() : 0;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreCborClient::GetStorePort() const
{
	if (!IsOpen())
	{
		return 0;
	}

	const asio::ip::tcp::endpoint Endpoint = Socket.remote_endpoint();
	return Endpoint.port();
}

////////////////////////////////////////////////////////////////////////////////
const FResponse& FStoreCborClient::GetResponse() const
{
	return Response;
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::Connect(const TCHAR* Host, uint16 Port)
{
	Port = (Port == 0) ? 1989 : Port;

	FTCHARToUTF8 HostUtf8(Host);
	char PortString[8];
	FCStringAnsi::Sprintf(PortString, "%d", Port);

	asio::ip::tcp::resolver Resolver(IoContext);
	asio::ip::tcp::resolver::results_type Endpoints = Resolver.resolve(
		asio::ip::tcp::resolver::protocol_type::v4(),
		(const char*)HostUtf8.Get(),
		PortString);

	asio::error_code ErrorCode;
#if PLATFORM_WINDOWS
	DWORD Flags = WSA_FLAG_NO_HANDLE_INHERIT|WSA_FLAG_OVERLAPPED;
	SOCKET WinSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, Flags);
	Socket.assign(asio::ip::tcp::v4(), WinSocket);
#else
	Socket.open(asio::ip::tcp::v4());
#endif

	asio::connect(Socket, Endpoints, ErrorCode);
	if (ErrorCode)
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::Communicate(const FPayload& Payload)
{
	if (!Socket.is_open())
	{
		return false;
	}

	asio::error_code ErrorCode;

	// Send the payload
	uint32 PayloadSize = Payload.Size;
	asio::write(Socket, asio::buffer(&PayloadSize, sizeof(PayloadSize)), ErrorCode);
	asio::write(Socket, asio::buffer(Payload.Data, Payload.Size), ErrorCode);
	if (ErrorCode)
	{
		Socket.close();
		return false;
	}

	// Wait for a response
	uint32 ResponseSize = 0;
	asio::read(Socket, asio::buffer(&ResponseSize, sizeof(ResponseSize)), ErrorCode);
	if (ErrorCode)
	{
		Socket.close();
		return false;
	}

	if (ResponseSize == 0)
	{
		Socket.close();
		return false;
	}

	uint8* Dest = Response.Reserve(ResponseSize);
	asio::read(Socket, asio::buffer(Dest, ResponseSize), ErrorCode);
	if (ErrorCode)
	{
		Socket.close();
		return false;
	}

	if (Response.GetStatusCode() != EStatusCode::Success)
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::GetStatus()
{
	TPayloadBuilder<32> Builder("v1/status");
	FPayload Payload = Builder.Done();
	return Communicate(Payload);
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::GetTraceCount()
{
	TPayloadBuilder<32> Builder("v1/trace/count");
	FPayload Payload = Builder.Done();
	return Communicate(Payload);
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::GetTraceInfo(uint32 Index)
{
	TPayloadBuilder<> Builder("v1/trace/info");
	Builder.AddInteger("index", Index);
	FPayload Payload = Builder.Done();
	return Communicate(Payload);
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::GetTraceInfoById(uint32 Id)
{
	TPayloadBuilder<> Builder("v1/trace/info");
	Builder.AddInteger("id", int32(Id));
	FPayload Payload = Builder.Done();
	return Communicate(Payload);
}

////////////////////////////////////////////////////////////////////////////////
FTraceDataStream* FStoreCborClient::ReadTrace(uint32 Id)
{
	TPayloadBuilder<> Builder("v1/trace/read");
	Builder.AddInteger("id", Id);
	FPayload Payload = Builder.Done();
	if (!Communicate(Payload))
	{
		return nullptr;
	}

	uint32 SenderPort = Response.GetUint32Checked("port", 0);
	if (!SenderPort)
	{
		return nullptr;
	}

	asio::ip::address ServerAddr = Socket.local_endpoint().address();
	asio::ip::tcp::endpoint Endpoint(ServerAddr, uint16(SenderPort));

	asio::error_code ErrorCode;
	asio::ip::tcp::socket SenderSocket(IoContext);
#if PLATFORM_WINDOWS
	DWORD Flags = WSA_FLAG_NO_HANDLE_INHERIT|WSA_FLAG_OVERLAPPED;
	SOCKET WinSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, Flags);
	SenderSocket.assign(asio::ip::tcp::v4(), WinSocket);
#else
	SenderSocket.open(asio::ip::tcp::v4());
#endif

	SenderSocket.connect(Endpoint, ErrorCode);
	if (ErrorCode)
	{
		return nullptr;
	}

	return new FTraceDataStream(SenderSocket);
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::GetSessionCount()
{
	TPayloadBuilder<> Builder("v1/session/count");
	FPayload Payload = Builder.Done();
	return Communicate(Payload);
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::GetSessionInfo(uint32 Index)
{
	TPayloadBuilder<> Builder("v1/session/info");
	Builder.AddInteger("index", int32(Index));
	FPayload Payload = Builder.Done();
	return Communicate(Payload);
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::GetSessionInfoById(uint32 Id)
{
	TPayloadBuilder<> Builder("v1/session/info");
	Builder.AddInteger("id", int32(Id));
	FPayload Payload = Builder.Done();
	return Communicate(Payload);
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreCborClient::GetSessionInfoByTraceId(uint32 TraceId)
{
	TPayloadBuilder<> Builder("v1/session/info");
	Builder.AddInteger("trace_id", int32(TraceId));
	FPayload Payload = Builder.Done();
	return Communicate(Payload);
}

} // namespace Trace
} // namespace UE


namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
FUtf8StringView FStoreClient::FStatus::GetStoreDir() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetString("store_dir", "");
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::FStatus::GetRecorderPort() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetUint32Checked("recorder_port", 0);
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::FStatus::GetChangeSerial() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetUint32Checked("change_serial", 0);
}





////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::FTraceInfo::GetId() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetUint32Checked("id", 0);
}

////////////////////////////////////////////////////////////////////////////////
uint64 FStoreClient::FTraceInfo::GetSize() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetUint64Checked("size", 0);
}

////////////////////////////////////////////////////////////////////////////////
uint64 FStoreClient::FTraceInfo::GetTimestamp() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetUint64Checked("timestamp", 0);
}

////////////////////////////////////////////////////////////////////////////////
FUtf8StringView FStoreClient::FTraceInfo::GetName() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetString("name", "nameless");
}



////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::FSessionInfo::GetId() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetUint32Checked("id", 0);
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::FSessionInfo::GetTraceId() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetUint32Checked("trace_id", 0);
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::FSessionInfo::GetIpAddress() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetUint32Checked("ip_address", 0);
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::FSessionInfo::GetControlPort() const
{
	const auto* Response = (const FResponse*)this;
	return Response->GetUint32Checked("control_port", 0);
}



////////////////////////////////////////////////////////////////////////////////
FStoreClient* FStoreClient::Connect(const TCHAR* Host, uint32 Port)
{
	static asio::io_context IoContext;

	FStoreCborClient* Impl = new FStoreCborClient(IoContext);
	if (!Impl->Connect(Host, static_cast<uint16>(Port)))
	{
		delete Impl;
		return nullptr;
	}

	return (FStoreClient*)Impl;
}

////////////////////////////////////////////////////////////////////////////////
void FStoreClient::operator delete (void* Addr)
{
	auto* Self = (FStoreCborClient*)Addr;
	delete Self;
}

////////////////////////////////////////////////////////////////////////////////
bool FStoreClient::IsValid() const
{
	auto* Self = (FStoreCborClient*)this;
	return Self->IsOpen();
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::GetStoreAddress() const
{
	auto* Self = (FStoreCborClient*)this;
	return Self->GetStoreAddress();
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::GetStorePort() const
{
	auto* Self = (FStoreCborClient*)this;
	return Self->GetStorePort();
}

////////////////////////////////////////////////////////////////////////////////
const FStoreClient::FStatus* FStoreClient::GetStatus()
{
	auto* Self = (FStoreCborClient*)this;
	if (!Self->GetStatus())
	{
		return nullptr;
	}

	const FResponse& Response = Self->GetResponse();
	return (FStatus*)(&Response);
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::GetTraceCount()
{
	auto* Self = (FStoreCborClient*)this;
	if (!Self->GetTraceCount())
	{
		return 0;
	}

	return Self->GetResponse().GetUint32Checked("count", 0);
}

////////////////////////////////////////////////////////////////////////////////
const FStoreClient::FTraceInfo* FStoreClient::GetTraceInfo(uint32 Index)
{
	auto* Self = (FStoreCborClient*)this;
	if (!Self->GetTraceInfo(Index))
	{
		return nullptr;
	}

	const FResponse& Response = Self->GetResponse();
	return (FTraceInfo*)(&Response);
}

////////////////////////////////////////////////////////////////////////////////
const FStoreClient::FTraceInfo* FStoreClient::GetTraceInfoById(uint32 Id)
{
	auto* Self = (FStoreCborClient*)this;
	if (!Self->GetTraceInfoById(Id))
	{
		return nullptr;
	}

	const FResponse& Response = Self->GetResponse();
	return (FTraceInfo*)(&Response);
}

////////////////////////////////////////////////////////////////////////////////
FStoreClient::FTraceData FStoreClient::ReadTrace(uint32 Id)
{
	auto* Self = (FStoreCborClient*)this;
	return FTraceData(Self->ReadTrace(Id));
}

////////////////////////////////////////////////////////////////////////////////
uint32 FStoreClient::GetSessionCount() const
{
	auto* Self = (FStoreCborClient*)this;
	if (!Self->GetSessionCount())
	{
		return 0;
	}

	return Self->GetResponse().GetUint32Checked("count", 0);
}

////////////////////////////////////////////////////////////////////////////////
const FStoreClient::FSessionInfo* FStoreClient::GetSessionInfo(uint32 Index) const
{
	auto* Self = (FStoreCborClient*)this;
	if (!Self->GetSessionInfo(Index))
	{
		return nullptr;
	}

	const FResponse& Response = Self->GetResponse();
	return (FSessionInfo*)(&Response);
}

////////////////////////////////////////////////////////////////////////////////
const FStoreClient::FSessionInfo* FStoreClient::GetSessionInfoById(uint32 Id) const
{
	auto* Self = (FStoreCborClient*)this;
	if (!Self->GetSessionInfoById(Id))
	{
		return nullptr;
	}

	const FResponse& Response = Self->GetResponse();
	return (FSessionInfo*)(&Response);
}

////////////////////////////////////////////////////////////////////////////////
const FStoreClient::FSessionInfo* FStoreClient::GetSessionInfoByTraceId(uint32 TraceId) const
{
	auto* Self = (FStoreCborClient*)this;
	if (!Self->GetSessionInfoByTraceId(TraceId))
	{
		return nullptr;
	}

	const FResponse& Response = Self->GetResponse();
	return (FSessionInfo*)(&Response);
}

} // namespace Trace
} // namespace UE

#include "MyTcpClient.h"

#include "MyTcpConnection.h"


bool FMyTcpClient::Connect(const FString& ServerURL, const FString& ServerProtocol)
{
	if (IsConnected())
	{
		UE_LOG(LogNetLib, Error, TEXT("%s: [TcpClient] connect failed, 已经连接."), *FString(__FUNCTION__));
		return false;
	}

	auto Ptr = MakeShared<FPbConnection>(0);
	const auto SocketPtr = MakeShared<FMySocketClientSide>();
	SocketPtr->Url = ServerURL;
	SocketPtr->Protocol = ServerProtocol;
	Ptr->Init(SocketPtr);

	Ptr->SetPackageCallback([this](const FPbConnectionPtr& Conn, uint64 Code, const FMyDataBufferPtr& Message)
	{
		OnMessage(Code, Message);
	});

	Ptr->SetConnectedCallback([this](const FPbConnectionPtr& Conn)
	{
		OnConnected();
	});

	Ptr->SetDisconnectedCallback([this](const FPbConnectionPtr& Conn)
	{
		OnDisconnected();
	});

	Ptr->SetErrorCallback([this](const FPbConnectionPtr& Conn)
	{
		OnError();
	});
	
	Connection = Ptr;
	Connection->Start();

	return true;
}

void FMyTcpClient::Shutdown()
{
	if (Connection)
	{
		Connection->Shutdown();
		Connection.Reset();
	}
}

bool FMyTcpClient::IsConnected() const
{
	return Connection && Connection->IsOpen();
}

void FMyTcpClient::Tick(float DeltaTime)
{
}

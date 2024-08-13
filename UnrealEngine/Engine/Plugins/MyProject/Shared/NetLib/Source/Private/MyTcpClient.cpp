#include "MyTcpClient.h"

#include "MyTcpConnection.h"

void FMyTcpClient::Init(const FString& ServerURL, const FString& ServerProtocol)
{
	Url = ServerURL;
	Protocol = ServerProtocol;
}

void FMyTcpClient::Connect()
{
	if (IsConnected())
	{
		UE_LOG(LogNetLib, Error, TEXT("%s: [TcpClient] connect failed, 已经连接=%s."), *FString(__FUNCTION__), *Url);
		return;
	}

	auto Ptr = MakeShared<FPbConnection>(0);

	const auto SocketPtr = MakeShared<FMySocketClient>();
	SocketPtr->Init(Url, Protocol);
	Ptr->Init(SocketPtr);
	
	Connection = Ptr;
	Connection->Start();
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

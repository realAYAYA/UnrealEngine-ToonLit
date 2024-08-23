#include "MGameClient.h"
#include "MyTcpConnection.h"
#include "GameRpcStub.h"

UMGameSession::UMGameSession(): Port(0)
{
	Stub = NewObject<UPbGameRpcStub>();
}

bool UMGameSession::K2_Connect(const FOnMGameSessionReqResult Callback)
{
	const FString TitleName("Default");
	const FString ConfigName("GameClient.ini");
	const FString FilePath = FPaths::SourceConfigDir() + ConfigName;
	GConfig->GetString(*TitleName, TEXT("ServerProtocol"), Protocol, FilePath);
	GConfig->GetString(*TitleName, TEXT("ServerAddress"), Address, FilePath);
	GConfig->GetInt(*TitleName, TEXT("ServerPort"), Port, FilePath);
	
	FString ServerURL = Protocol + TEXT("://") + Address + TEXT(":") + FString::FromInt(Port);
	if (Connect(ServerURL, FString()))
	{
		OnConnectCallback = Callback;
		return true;
	}

	Callback.ExecuteIfBound(1);
	
	return false;
}

void UMGameSession::K2_Disconnect()
{
	Shutdown();
	OnDisconnect.Broadcast();
}

bool UMGameSession::K2_IsConnected() const
{
	return IsConnected();
}

void UMGameSession::OnConnected()
{
	if (OnConnectCallback.ExecuteIfBound(0))
	{
		OnConnectCallback.Clear();
	}
	
	Stub->Setup(&GetRpcManager(), Connection);
}

void UMGameSession::OnDisconnected()
{
	if (Connection)
	{
		Connection->Shutdown();
		Connection.Reset();
	}
	
	Stub->Setup(nullptr, nullptr);
	OnDisconnect.Broadcast();
}

void UMGameSession::OnError()
{
	if (OnConnectCallback.ExecuteIfBound(1))
	{
		OnConnectCallback.Clear();
	}
}

FPbDispatcher& UMGameSession::GetMessageDispatcher()
{
	return GetRpcManager().GetMessageDispatcher();
}

FMRpcManager& UMGameSession::GetRpcManager()
{
	static FMRpcManager RpcManager;
	return RpcManager;
}

void UMGameSession::OnMessage(uint64 InPbTypeId, const FMyDataBufferPtr& InPackage)
{
	if (!IsConnected())
		return;
	
	if (!GetMessageDispatcher().Process(Connection.Get(), InPbTypeId, InPackage))
	{
		
	}
}
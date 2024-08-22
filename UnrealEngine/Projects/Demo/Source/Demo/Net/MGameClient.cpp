#include "MGameClient.h"
#include "MyTcpConnection.h"
#include "GameRpcStub.h"
#include "GameTablesModule.h"
#include "GameTables.h"


UMGameSession::UMGameSession()
{
	Stub = NewObject<UPbGameRpcStub>();

	
}

bool UMGameSession::K2_Connect(const FOnMGameSessionReqResult Callback)
{
	
	FString ServerURL = FGameTablesModule::Get().GetGameTables()->GameClientConfig.server_ip;
	const int32 Port = FGameTablesModule::Get().GetGameTables()->GameClientConfig.server_port;
	ServerURL = ServerURL + TEXT(":") + FString::FromInt(Port);

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
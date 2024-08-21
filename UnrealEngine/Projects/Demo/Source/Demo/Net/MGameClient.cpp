#include "MGameClient.h"
#include "MyTcpConnection.h"
#include "GameRpcStub.h"
#include "GameTablesModule.h"
#include "GameTables.h"


UMGameSession::UMGameSession()
{
	Stub = NewObject<UPbGameRpcStub>();

	
}

bool UMGameSession::K2_Connect(const FString& ServerURL, const FString& ServerProtocol)
{

	const FString Ip = FGameTablesModule::Get().GetGameTables()->GameClientConfig.server_ip;
	const int32 Port = FGameTablesModule::Get().GetGameTables()->GameClientConfig.server_port;
	
	return Connect(ServerURL, ServerProtocol);
}

void UMGameSession::K2_Disconnect()
{
	Shutdown();
}

bool UMGameSession::K2_IsConnected() const
{
	return IsConnected();
}

void UMGameSession::OnConnected()
{
	Stub->Setup(&GetRpcManager(), Connection);
}

void UMGameSession::OnDisconnected()
{
	Stub->Setup(nullptr, nullptr);
}

void UMGameSession::OnError()
{
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
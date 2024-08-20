#include "MGameServer.h"
#include "MGameServerPrivate.h"

#include "IWebSocketNetworkingModule.h"

void UMGameServer::Tick(float DeltaTime)
{
	if (IsRunning())
	{
		WebSocketServer->Tick();
	}
}

bool UMGameServer::IsTickable() const
{
	return true;
}

TStatId UMGameServer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMGameServer, STATGROUP_Tickables);
}

bool UMGameServer::Start(const int32 ServerPort)
{
	FWebSocketClientConnectedCallBack CallBack;
	CallBack.BindUObject(this, &UMGameServer::OnClientConnected);

	if (WebSocketServer && WebSocketServer.IsValid())
	{
		UE_LOG(LogMGameServer, Display, TEXT("%s: WebSocketServer is running."), *FString(__FUNCTION__));
		return false;
	}
	
	WebSocketServer = FModuleManager::Get().LoadModuleChecked<IWebSocketNetworkingModule>(TEXT("WebSocketNetworking")).CreateServer();
	if (!WebSocketServer || !WebSocketServer->Init(ServerPort, CallBack))
	{
		UE_LOG(LogMGameServer, Display, TEXT("%s: WebSocketServer Init Failed."), *FString(__FUNCTION__));
		WebSocketServer.Reset();
		return false;
	}

	UE_LOG(LogMGameServer, Display, TEXT("%s: WebSocketServer Init Succed."), *FString(__FUNCTION__));
	
	return true;
}

void UMGameServer::Stop()
{
	if (IsRunning())
	{
		for (const auto& Conn : Connections)
		{
			UMGameSession* Session = Conn.Value;
			Session->Shutdown();
		}

		Connections.Empty();
		
		WebSocketServer.Reset();

		UE_LOG(LogMGameServer, Display, TEXT("%s"), *FString(__FUNCTION__));

		return;
	}

	UE_LOG(LogMGameServer, Display, TEXT("Server was not running"));
}

bool UMGameServer::CheckConnectionValid(const FGuid InID)
{
	if (!IsRunning())
		return false;

	const UMGameSession* Connection = *Connections.Find(InID);
	if (Connection && Connection->WebSocket)
		return false;
	
	return true;
}

void UMGameServer::DoAliveCheck(const FDateTime& Now)
{
	#if WITH_EDITOR
	return;  // 编辑器模式运行不做超时检查
	#endif
	
	if (NextSessionAliveCheckTime > Now)
		return;
	
	NextSessionAliveCheckTime = Now + FTimespan::FromSeconds(5);
	
	for (auto& Elem : Connections)
	{
		auto Session = Elem.Value;
		if (Session && Session->WebSocket)
		{
			const FDateTime LastTime = FMath::Max(Session->GetLastSentTime(), Session->GetLastReceivedTime());
			const int32 Seconds = (Now - LastTime).GetTotalSeconds();
			if (Seconds >= 60)
			{
				Session->Shutdown();
			}
		}
	}
}

void UMGameServer::DoPrintStats(FDateTime Now)
{
}

void UMGameServer::OnClientConnected(INetworkingWebSocket* InWebSocket)
{
	if (!InWebSocket)
		return;
	
	UMGameSession* Conn = NewObject<UMGameSession>(this);
	Conn->Initialize(InWebSocket, FGuid::NewGuid());
	
	FWebSocketInfoCallBack ConnectedCallBack;
	ConnectedCallBack.BindUObject(this, &UMGameServer::OnConnected, Conn->ID);
	InWebSocket->SetConnectedCallBack(ConnectedCallBack);

	FWebSocketPacketReceivedCallBack ReceivedCallBack;
	ReceivedCallBack.BindUObject(this, &UMGameServer::OnReceive, Conn->ID);
	InWebSocket->SetReceiveCallBack(ReceivedCallBack);

	FWebSocketInfoCallBack ErrorCallBack;
	ErrorCallBack.BindUObject(this, &UMGameServer::OnError, Conn->ID);
	InWebSocket->SetErrorCallBack(ErrorCallBack);

	FWebSocketInfoCallBack ClosedCallBack;
	ClosedCallBack.BindUObject(this, &UMGameServer::OnClosed, Conn->ID);
	InWebSocket->SetSocketClosedCallBack(ClosedCallBack);

	Conn->OnConnected();
	
	Connections.Emplace(Conn->ID, Conn);

	UE_LOG(LogMGameServer, Display, TEXT("%s"), *FString(__FUNCTION__));
}

bool UMGameServer::IsRunning() const
{
	return WebSocketServer && WebSocketServer.Get() && WebSocketServer.IsValid();
	
}

void UMGameServer::OnConnected(const FGuid InID)
{
	UMGameSession* Connection = *Connections.Find(InID);
	if (Connection && Connection->WebSocket)
	{
		Connection->OnConnected();
		UE_LOG(LogMGameServer, Warning, TEXT("User: %s - %s"), *InID.ToString(), *FString(__FUNCTION__));
	}
}

void UMGameServer::OnReceive(void* InData, const int32 DataSize, const FGuid InID)
{
	/*const FGameSessionPtr* Connection = Connections.Find(InID);

	if (Connection && (*Connection)->WebSocket)
	{
		(*Connection)->OnReceive(InData, DataSize);
	}*/
}

void UMGameServer::OnError(const FGuid InID)
{
	UMGameSession* Connection = *Connections.Find(InID);
	if (Connection && Connection->WebSocket)
	{
		Connection->Shutdown();
		UE_LOG(LogMGameServer, Warning, TEXT("User: %s - %s"), *InID.ToString(), *FString(__FUNCTION__));
	}
}

void UMGameServer::OnClosed(const FGuid InID)
{
	UMGameSession* Connection = *Connections.Find(InID);
	if (Connection && Connection->WebSocket)
	{
		if (IsValid(WebSocketClientClosedCallBack.GetUObject()))
			WebSocketClientClosedCallBack.Execute(InID);

		Connection->Shutdown();
		Connections.Remove(InID);
	}
	
	UE_LOG(LogMGameServer, Display, TEXT("%s"), *FString(__FUNCTION__));
}

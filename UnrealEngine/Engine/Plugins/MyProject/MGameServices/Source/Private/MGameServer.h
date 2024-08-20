#pragma once

#include "CoreMinimal.h"
#include "IWebSocketServer.h"

#include "MGameSession.h"
#include "MGameServer.generated.h"

DECLARE_DELEGATE_OneParam(FMWebSocketClientClosedCallBack, const FGuid);
DECLARE_DELEGATE_TwoParams(FMWebSocketReceiveCallBack, const FGuid, FString);

UCLASS()
class UMGameServer : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

public:
	
	bool Start(const int32 ServerPort);
	void Stop();
	bool IsRunning() const;
	
	bool CheckConnectionValid(const FGuid InID);

	void DoAliveCheck(const FDateTime& Now);
	void DoPrintStats(FDateTime Now);// 每隔一段时间打印日志

	FMWebSocketClientClosedCallBack WebSocketClientClosedCallBack;
	FMWebSocketReceiveCallBack WebSocketReceiveCallBack;

protected:

	void OnClientConnected(INetworkingWebSocket* InWebSocket);

	void OnConnected(const FGuid InID);
	void OnReceive(void* InData, const int32 DataSize, const FGuid InID);
	void OnError(const FGuid InID);
	void OnClosed(const FGuid InID);

private:

	TUniquePtr<IWebSocketServer> WebSocketServer;

	UPROPERTY()
	TMap<FGuid, UMGameSession*> Connections;
	
	//TMap<FGuid, > DSs;

	//TMap<FGuid, > Users;

	FDateTime NextSessionAliveCheckTime{0};
};

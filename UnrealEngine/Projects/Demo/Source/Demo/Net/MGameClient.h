#pragma once
#include "MRpcManager.h"
#include "MyTcpClient.h"
#include "PbDispatcher.h"

#include "MGameClient.generated.h"

class UPbGameRpcStub;

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnMGameSessionReqResult, int32, ErrorCode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMGameSessionNotify);

UCLASS(BlueprintType)
class UMGameSession: public UObject, public FMyTcpClient
{

	GENERATED_BODY()
	
public:

	UMGameSession();

	UPROPERTY(BlueprintReadWrite, Category = "ProjectM")
	FString Address;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectM")
	FString Protocol;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectM")
	int32 Port;

	UPROPERTY(BlueprintReadOnly, Category = "ProjectM")
	UPbGameRpcStub* Stub;

	UPROPERTY()
	FOnMGameSessionReqResult OnConnectCallback;
	
	UPROPERTY(BlueprintAssignable, Category = "ProjectM")
	FOnMGameSessionNotify OnDisconnect;
	
	UFUNCTION(BlueprintCallable, Category = "ProjectM")
	bool K2_Connect(FOnMGameSessionReqResult Callback);

	UFUNCTION(BlueprintCallable, Category = "ProjectM")
	void K2_Disconnect();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ProjectM")
	bool K2_IsConnected() const;
	
	virtual void OnConnected() override;
	virtual void OnDisconnected() override;
	virtual void OnError() override;

	static FPbDispatcher& GetMessageDispatcher();
	static FMRpcManager& GetRpcManager();
	
private:

	virtual void OnMessage(uint64 InPbTypeId, const FMyDataBufferPtr& InPackage) override;
};

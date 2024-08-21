#pragma once
#include "MRpcManager.h"
#include "MyTcpClient.h"
#include "PbDispatcher.h"

#include "MGameClient.generated.h"

class UPbGameRpcStub;

UCLASS(BlueprintType)
class UMGameSession: public UObject, public FMyTcpClient
{

	GENERATED_BODY()
	
public:

	UMGameSession();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MGameClient")
	FString Url;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MGameClient")
	FString Protocol;

	UPROPERTY(BlueprintReadOnly, Category = "MGameClient")
	UPbGameRpcStub* Stub;

	UFUNCTION(BlueprintCallable, Category = "MGameClient")
	bool K2_Connect(const FString& ServerURL, const FString& ServerProtocol);

	UFUNCTION(BlueprintCallable, Category = "MGameClient")
	void K2_Disconnect();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MGameClient")
	bool K2_IsConnected() const;
	
	virtual void OnConnected() override;
	virtual void OnDisconnected() override;
	virtual void OnError() override;

	static FPbDispatcher& GetMessageDispatcher();
	static FMRpcManager& GetRpcManager();
	
private:

	virtual void OnMessage(uint64 InPbTypeId, const FMyDataBufferPtr& InPackage) override;

	FPbConnectionPtr Connection;
};

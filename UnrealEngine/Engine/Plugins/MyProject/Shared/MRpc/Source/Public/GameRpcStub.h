#pragma once

#include "MyNetFwd.h"
#include "MRpcManager.h"

#include "PbCommon.h"
#include "PbGame.h"
#include "common.pb.h"
#include "game.pb.h"

#include "GameRpcStub.generated.h"


DECLARE_DYNAMIC_DELEGATE_TwoParams(FPbOnLoginGameResult, EPbRpcErrorCode, InErrorCode, FPbLoginGameAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FPbOnEnterLevelResult, EPbRpcErrorCode, InErrorCode, FPbEnterLevelAck, InData);




UCLASS(BlueprintType, Blueprintable)
class MRPC_API UPbGameRpcStub : public UObject
{
    GENERATED_BODY()

public:

    void Setup(FMRpcManager* InManager, const FPbConnectionPtr& InConn);
    void Cleanup();    

    /**
     * 登录游戏
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="LoginGame")
    void K2_LoginGame(const FPbLoginGameReq& InParams, const FPbOnLoginGameResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::LoginGameAck>&)> FOnLoginGameResult;
    void LoginGame(const TSharedPtr<idlepb::LoginGameReq>& InReqMessage, const FOnLoginGameResult& InCallback) const;    

    /**
     * 进入关卡
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="EnterLevel")
    void K2_EnterLevel(const FPbEnterLevelReq& InParams, const FPbOnEnterLevelResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::EnterLevelAck>&)> FOnEnterLevelResult;
    void EnterLevel(const TSharedPtr<idlepb::EnterLevelReq>& InReqMessage, const FOnEnterLevelResult& InCallback) const;

    
    
private:
    FMRpcManager* Manager = nullptr;
    FPbConnectionPtr Connection;
};

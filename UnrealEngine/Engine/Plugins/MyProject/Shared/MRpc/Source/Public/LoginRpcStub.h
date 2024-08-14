#pragma once

#include "ZNetFwd.h"
#include "ZRpcManager.h"

#include "ZPbCommon.h"
#include "ZPbLogin.h"
#include "common.pb.h"
#include "login.pb.h"

#include "LoginRpcStub.generated.h"


DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnLoginAccountResult, EZRpcErrorCode, InErrorCode, FZLoginAccountAck, InData);




UCLASS(BlueprintType, Blueprintable)
class ZRPC_API UZLoginRpcStub : public UObject
{
    GENERATED_BODY()

public:

    void Setup(FZRpcManager* InManager, const FZPbConnectionPtr& InConn);
    void Cleanup();    

    /**
     * 登录帐号
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="LoginAccount")
    void K2_LoginAccount(const FZLoginAccountReq& InParams, const FZOnLoginAccountResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::LoginAccountAck>&)> OnLoginAccountResult;
    void LoginAccount(const TSharedPtr<idlezt::LoginAccountReq>& InReqMessage, const OnLoginAccountResult& InCallback);

    
    
private:
    FZRpcManager* Manager = nullptr;
    FZPbConnectionPtr Connection;
};

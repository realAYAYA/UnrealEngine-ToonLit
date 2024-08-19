#pragma once

#include "MyNetFwd.h"
#include "MRpcManager.h"

#include "PbCommon.h"
#include "PbLogin.h"
#include "common.pb.h"
#include "login.pb.h"

#include "LoginRpcStub.generated.h"


DECLARE_DYNAMIC_DELEGATE_TwoParams(FPbOnLoginAccountResult, EPbRpcErrorCode, InErrorCode, FPbLoginAccountAck, InData);




UCLASS(BlueprintType, Blueprintable)
class MRPC_API UPbLoginRpcStub : public UObject
{
    GENERATED_BODY()

public:

    void Setup(FMRpcManager* InManager, const FPbConnectionPtr& InConn);
    void Cleanup();    

    /**
     * 登录帐号
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="LoginAccount")
    void K2_LoginAccount(const FPbLoginAccountReq& InParams, const FPbOnLoginAccountResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::LoginAccountAck>&)> FOnLoginAccountResult;
    void LoginAccount(const TSharedPtr<idlepb::LoginAccountReq>& InReqMessage, const FOnLoginAccountResult& InCallback) const;

    
    
private:
    FMRpcManager* Manager = nullptr;
    FPbConnectionPtr Connection;
};

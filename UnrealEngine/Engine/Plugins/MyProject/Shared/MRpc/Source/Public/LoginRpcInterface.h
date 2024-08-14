#pragma once

#include "ZPbCommon.h"
#include "ZPbLogin.h"
#include "common.pb.h"
#include "login.pb.h"

#include "ZTools.h"
#include "ZRpcManager.h"

class ZRPC_API FZLoginRpcInterface
{
public:

    FZLoginRpcInterface(FZRpcManager* InManager);
    virtual ~FZLoginRpcInterface();

    const TCHAR* GetName() const { return TEXT("LoginRpc"); }  
    
    
    /**
     * 登录帐号
    */
    static constexpr uint64 LoginAccount = 0x9939607413dbff1dLL; 
    typedef TSharedPtr<idlezt::LoginAccountReq> FZLoginAccountReqPtr;
    typedef TSharedPtr<idlezt::LoginAccountAck> FZLoginAccountRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZLoginAccountReqPtr&, const FZLoginAccountRspPtr&)> FZLoginAccountCallback;
    static void LoginAccountRegister(FZRpcManager* InManager, const FZLoginAccountCallback& InCallback);
    

};

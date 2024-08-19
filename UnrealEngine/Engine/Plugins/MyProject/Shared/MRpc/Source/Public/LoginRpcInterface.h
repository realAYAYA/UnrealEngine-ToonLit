#pragma once

#include "PbCommon.h"
#include "PbLogin.h"
#include "common.pb.h"
#include "login.pb.h"

#include "MTools.h"
#include "MRpcManager.h"

class MRPC_API FZLoginRpcInterface
{
public:

    explicit FZLoginRpcInterface(FMRpcManager* InManager);
    virtual ~FZLoginRpcInterface();

    static const TCHAR* GetName() { return TEXT("LoginRpc"); }  
    
    
    /**
     * 登录帐号
    */
    static constexpr uint64 LoginAccount = 0x9939607413dbff1dLL; 
    typedef TSharedPtr<idlepb::LoginAccountReq> FZLoginAccountReqPtr;
    typedef TSharedPtr<idlepb::LoginAccountAck> FZLoginAccountRspPtr;
    typedef TFunction<void(FPbMessageSupportBase*, const FZLoginAccountReqPtr&, const FZLoginAccountRspPtr&)> FZLoginAccountCallback;
    static void LoginAccountRegister(FMRpcManager* InManager, const FZLoginAccountCallback& InCallback);
    

};

#pragma once

#include "PbCommon.h"
#include "PbLogin.h"
#include "common.pb.h"
#include "login.pb.h"

#include "MTools.h"
#include "MRpcManager.h"
#include "MyTcpConnection.h"

class MRPC_API FPbLoginRpcInterface
{
public:

    explicit FPbLoginRpcInterface(FMRpcManager* InManager);
    virtual ~FPbLoginRpcInterface();

    static const TCHAR* GetName() { return TEXT("LoginRpc"); }  
    
    
    /**
     * 登录帐号
    */
    static constexpr uint64 LoginAccount = 0x9939607413dbff1dLL; 
    typedef TSharedPtr<idlepb::LoginAccountReq> FPbLoginAccountReqPtr;
    typedef TSharedPtr<idlepb::LoginAccountAck> FPbLoginAccountRspPtr;
    typedef TFunction<void(FPbMessageSupportBase*, const FPbLoginAccountReqPtr&, const FPbLoginAccountRspPtr&)> FPbLoginAccountCallback;
    static void LoginAccountRegister(FMRpcManager* InManager, const FPbLoginAccountCallback& InCallback);
    

};

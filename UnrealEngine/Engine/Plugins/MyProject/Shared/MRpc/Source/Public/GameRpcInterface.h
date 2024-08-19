#pragma once

#include "PbCommon.h"
#include "PbGame.h"
#include "common.pb.h"
#include "game.pb.h"

#include "MTools.h"
#include "MRpcManager.h"
#include "MyTcpConnection.h"

class MRPC_API FPbGameRpcInterface
{
public:

    explicit FPbGameRpcInterface(FMRpcManager* InManager);
    virtual ~FPbGameRpcInterface();

    static const TCHAR* GetName() { return TEXT("GameRpc"); }  
    
    
    /**
     * 登录游戏
    */
    static constexpr uint64 LoginGame = 0xce6f5c055d80fbc0LL; 
    typedef TSharedPtr<idlepb::LoginGameReq> FPbLoginGameReqPtr;
    typedef TSharedPtr<idlepb::LoginGameAck> FPbLoginGameRspPtr;
    typedef TFunction<void(FPbMessageSupportBase*, const FPbLoginGameReqPtr&, const FPbLoginGameRspPtr&)> FPbLoginGameCallback;
    static void LoginGameRegister(FMRpcManager* InManager, const FPbLoginGameCallback& InCallback);
    

};

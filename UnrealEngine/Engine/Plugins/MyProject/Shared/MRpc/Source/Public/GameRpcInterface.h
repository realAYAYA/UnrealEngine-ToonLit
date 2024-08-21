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
    
    /**
     * 进入关卡
    */
    static constexpr uint64 EnterLevel = 0x3871c2e6f4dbfe95LL; 
    typedef TSharedPtr<idlepb::EnterLevelReq> FPbEnterLevelReqPtr;
    typedef TSharedPtr<idlepb::EnterLevelAck> FPbEnterLevelRspPtr;
    typedef TFunction<void(FPbMessageSupportBase*, const FPbEnterLevelReqPtr&, const FPbEnterLevelRspPtr&)> FPbEnterLevelCallback;
    static void EnterLevelRegister(FMRpcManager* InManager, const FPbEnterLevelCallback& InCallback);
    

};

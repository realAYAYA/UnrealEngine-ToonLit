#include "GameRpcInterface.h"

FPbGameRpcInterface::FPbGameRpcInterface(FMRpcManager* InManager)
{
}

FPbGameRpcInterface::~FPbGameRpcInterface()
{
}

void FPbGameRpcInterface::LoginGameRegister(FMRpcManager* InManager, const FPbLoginGameCallback& InCallback)
{
    static constexpr uint64 RpcId = FPbGameRpcInterface::LoginGame;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::LoginGameReq>();
        auto RspMessage = MakeShared<idlepb::LoginGameAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FPbGameRpcInterface::EnterLevelRegister(FMRpcManager* InManager, const FPbEnterLevelCallback& InCallback)
{
    static constexpr uint64 RpcId = FPbGameRpcInterface::EnterLevel;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::EnterLevelReq>();
        auto RspMessage = MakeShared<idlepb::EnterLevelAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

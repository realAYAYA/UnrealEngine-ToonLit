#include "LoginRpcInterface.h"

FZLoginRpcInterface::FZLoginRpcInterface(FMRpcManager* InManager)
{
}

FZLoginRpcInterface::~FZLoginRpcInterface()
{
}

void FZLoginRpcInterface::LoginAccountRegister(FMRpcManager* InManager, const FZLoginAccountCallback& InCallback)
{
    static constexpr uint64 RpcId = FZLoginRpcInterface::LoginAccount;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::LoginAccountReq>();
        auto RspMessage = MakeShared<idlepb::LoginAccountAck>();
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

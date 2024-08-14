#include "LoginRpcInterface.h"

FZLoginRpcInterface::FZLoginRpcInterface(FZRpcManager* InManager)
{
}

FZLoginRpcInterface::~FZLoginRpcInterface()
{
}

void FZLoginRpcInterface::LoginAccountRegister(FZRpcManager* InManager, const FZLoginAccountCallback& InCallback)
{
    static constexpr uint64 RpcId = FZLoginRpcInterface::LoginAccount;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::LoginAccountReq>();
        auto RspMessage = MakeShared<idlezt::LoginAccountAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

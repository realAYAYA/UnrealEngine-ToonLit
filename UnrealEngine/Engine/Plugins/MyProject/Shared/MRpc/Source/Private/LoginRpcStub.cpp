#include "LoginRpcStub.h"
#include "LoginRpcInterface.h"
#include "MRpcManager.h"

void UZLoginRpcStub::Setup(FMRpcManager* InManager, const FZPbConnectionPtr& InConn)
{
    if (Manager)
    {
        Cleanup();
    }

    Manager = InManager;
    Connection = InConn;

    if (Manager)
    {
    }
}

void UZLoginRpcStub::Cleanup()
{
    if (Manager)
    {        
    }
    Manager = nullptr;
    Connection = nullptr;    
}


void UZLoginRpcStub::K2_LoginAccount(const FZLoginAccountReq& InParams, const FZOnLoginAccountResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::LoginAccountReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LoginAccount(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::LoginAccountAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZLoginAccountAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZLoginRpcStub::LoginAccount(const TSharedPtr<idlezt::LoginAccountReq>& InReqMessage, const OnLoginAccountResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZLoginRpcInterface::LoginAccount;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::LoginAccountAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}



       



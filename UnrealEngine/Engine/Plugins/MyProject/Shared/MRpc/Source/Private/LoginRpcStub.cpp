#include "LoginRpcStub.h"
#include "LoginRpcInterface.h"
#include "MRpcManager.h"

void UPbLoginRpcStub::Setup(FMRpcManager* InManager, const FPbConnectionPtr& InConn)
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

void UPbLoginRpcStub::Cleanup()
{
    if (Manager)
    {        
    }
    Manager = nullptr;
    Connection = nullptr;    
}


void UPbLoginRpcStub::K2_LoginAccount(const FPbLoginAccountReq& InParams, const FPbOnLoginAccountResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::LoginAccountReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LoginAccount(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::LoginAccountAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FPbLoginAccountAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UPbLoginRpcStub::LoginAccount(const TSharedPtr<idlepb::LoginAccountReq>& InReqMessage, const FOnLoginAccountResult& InCallback) const
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FPbLoginRpcInterface::LoginAccount;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::LoginAccountAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}



       



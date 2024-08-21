#include "GameRpcStub.h"
#include "GameRpcInterface.h"
#include "MRpcManager.h"

void UPbGameRpcStub::Setup(FMRpcManager* InManager, const FPbConnectionPtr& InConn)
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

void UPbGameRpcStub::Cleanup()
{
    if (Manager)
    {        
    }
    Manager = nullptr;
    Connection = nullptr;    
}


void UPbGameRpcStub::K2_LoginGame(const FPbLoginGameReq& InParams, const FPbOnLoginGameResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::LoginGameReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LoginGame(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::LoginGameAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FPbLoginGameAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UPbGameRpcStub::LoginGame(const TSharedPtr<idlepb::LoginGameReq>& InReqMessage, const FOnLoginGameResult& InCallback) const
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FPbGameRpcInterface::LoginGame;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::LoginGameAck>();               
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


void UPbGameRpcStub::K2_EnterLevel(const FPbEnterLevelReq& InParams, const FPbOnEnterLevelResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::EnterLevelReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    EnterLevel(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::EnterLevelAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FPbEnterLevelAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UPbGameRpcStub::EnterLevel(const TSharedPtr<idlepb::EnterLevelReq>& InReqMessage, const FOnEnterLevelResult& InCallback) const
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FPbGameRpcInterface::EnterLevel;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::EnterLevelAck>();               
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



       



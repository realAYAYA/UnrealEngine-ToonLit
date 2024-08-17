#include "PbNet.h"
#include "net.pb.h"



bool CheckEPbRpcMessageOpValid(int32 Val)
{
    return idlepb::RpcMessageOp_IsValid(Val);
}

const TCHAR* GetEPbRpcMessageOpDescription(EPbRpcMessageOp Val)
{
    switch (Val)
    {
        case EPbRpcMessageOp::RpcMessageOp_Notify: return TEXT("通知");
        case EPbRpcMessageOp::RpcMessageOp_Request: return TEXT("请求");
        case EPbRpcMessageOp::RpcMessageOp_Response: return TEXT("回应");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbRpcErrorCodeValid(int32 Val)
{
    return idlepb::RpcErrorCode_IsValid(Val);
}

const TCHAR* GetEPbRpcErrorCodeDescription(EPbRpcErrorCode Val)
{
    switch (Val)
    {
        case EPbRpcErrorCode::RpcErrorCode_Ok: return TEXT("正常");
        case EPbRpcErrorCode::RpcErrorCode_Unknown: return TEXT("未知错误");
        case EPbRpcErrorCode::RpcErrorCode_Unimplemented: return TEXT("接口未实现");
        case EPbRpcErrorCode::RpcErrorCode_Timeout: return TEXT("调用超时");
        case EPbRpcErrorCode::RpcErrorCode_ReqDataError: return TEXT("请求数据错误");
        case EPbRpcErrorCode::RpcErrorCode_RspDataError: return TEXT("返回数据错误");
    }
    return TEXT("UNKNOWN");
}

FPbPbRpcMessage::FPbPbRpcMessage()
{
    Reset();        
}

FPbPbRpcMessage::FPbPbRpcMessage(const idlepb::PbRpcMessage& Right)
{
    this->FromPb(Right);
}

void FPbPbRpcMessage::FromPb(const idlepb::PbRpcMessage& Right)
{
    op = static_cast<EPbRpcMessageOp>(Right.op());
    sn = Right.sn();
    error_code = static_cast<EPbRpcErrorCode>(Right.error_code());
    rpc_id = Right.rpc_id();
    body_type_id = Right.body_type_id();
    body_data.Empty(body_data.Num());
    body_data.Append(reinterpret_cast<const uint8*>(Right.body_data().c_str()), Right.body_data().size());
}

void FPbPbRpcMessage::ToPb(idlepb::PbRpcMessage* Out) const
{
    Out->set_op(static_cast<idlepb::RpcMessageOp>(op));
    Out->set_sn(sn);
    Out->set_error_code(static_cast<idlepb::RpcErrorCode>(error_code));
    Out->set_rpc_id(rpc_id);
    Out->set_body_type_id(body_type_id);
    Out->set_body_data(body_data.GetData(), body_data.Num());    
}

void FPbPbRpcMessage::Reset()
{
    op = EPbRpcMessageOp();
    sn = int64();
    error_code = EPbRpcErrorCode();
    rpc_id = int64();
    body_type_id = int64();
    body_data = TArray<uint8>();    
}

void FPbPbRpcMessage::operator=(const idlepb::PbRpcMessage& Right)
{
    this->FromPb(Right);
}

bool FPbPbRpcMessage::operator==(const FPbPbRpcMessage& Right) const
{
    if (this->op != Right.op)
        return false;
    if (this->sn != Right.sn)
        return false;
    if (this->error_code != Right.error_code)
        return false;
    if (this->rpc_id != Right.rpc_id)
        return false;
    if (this->body_type_id != Right.body_type_id)
        return false;
    if (this->body_data != Right.body_data)
        return false;
    return true;
}

bool FPbPbRpcMessage::operator!=(const FPbPbRpcMessage& Right) const
{
    return !operator==(Right);
}
#include "MRpcManager.h"

#include "MTools.h"

FMRpcManager::FMRpcManager()
{
	MessageDispatcher.Reg<idlepb::PbRpcMessage>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
	{
		OnRpcMessage(InConn, InMessage);
	});
}

void FMRpcManager::TryInit(FPbDispatcher& InDispatcher)
{
}

uint64 FMRpcManager::CallRpc(FPbMessageSupportBase* InConn, const uint64 InRpcId, const FPbMessagePtr& InMessage, const FResponseCallback& Callback)
{
	const uint64 BodyTypeId = FMyTools::GeneratePbMessageTypeId(&*InMessage);
	const std::string& BodyData = InMessage->SerializeAsString();
	return CallRpc(InConn, InRpcId, BodyTypeId, BodyData.c_str(), BodyData.size(), Callback);
}

uint64 FMRpcManager::CallRpc(FPbMessageSupportBase* InConn, const uint64 InRpcId, const uint64 InBodyTypeId, const char* InBodyDataPtr, const int32 InBodyDataLength, const FResponseCallback& Callback)
{
	const uint64 SerialNum = SendRequest(InConn, InRpcId, InBodyTypeId, InBodyDataPtr, InBodyDataLength);

	FRequestPendingData Data;
	Data.SerialNum = SerialNum;
	Data.Callback = Callback;
	Data.ExpireTimestamp = 0;
	AllRequestPending.Emplace(SerialNum, std::move(Data));

	return SerialNum;
}

void FMRpcManager::AddMethod(uint64 InRpcId, const FMethodCallback& InCallback)
{
	AllMethods.Emplace(InRpcId, InCallback);
}

uint64 FMRpcManager::SendRequest(FPbMessageSupportBase* InConn, const uint64 InRpcId, const uint64 InBodyTypeId, const char* InBodyDataPtr, const int32 InBodyDataLength)
{
	const uint64 SerialNum = NextSn++;

	const auto Message = MakeShared<idlepb::PbRpcMessage>();
	Message->set_op(idlepb::RpcMessageOp_Request);
	Message->set_sn(SerialNum);
	Message->set_error_code(idlepb::RpcErrorCode_Ok);
	Message->set_rpc_id(InRpcId);
	Message->set_body_type_id(InBodyTypeId);
	Message->set_body_data(InBodyDataPtr, InBodyDataLength);
	InConn->SendMessage(Message);
	
	return SerialNum;
}

// static
void FMRpcManager::SendResponse(FPbMessageSupportBase* InConn, const uint64 InRpcId, const uint64 InReqSerialNum, const FPbMessagePtr& InMessage, const idlepb::RpcErrorCode ErrorCode)
{
	const auto OutMessage = MakeShared<idlepb::PbRpcMessage>();
	OutMessage->set_op(idlepb::RpcMessageOp_Response);
	OutMessage->set_sn(InReqSerialNum);
	OutMessage->set_error_code(ErrorCode);
	OutMessage->set_rpc_id(InRpcId);
	if (InMessage)
	{
		const uint64 BodyTypeId = FMyTools::GeneratePbMessageTypeId(&*InMessage);
		const std::string& BodyData = InMessage->SerializeAsString();
		OutMessage->set_body_type_id(BodyTypeId);
		OutMessage->set_body_data(BodyData);
	}
	InConn->SendMessage(OutMessage);
}

FPbDispatcher& FMRpcManager::GetMessageDispatcher()
{
	return MessageDispatcher;
}

void FMRpcManager::OnRpcMessage(FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
{
	switch (InMessage->op())
	{
	case idlepb::RpcMessageOp_Notify:
		{
			OnNotify(InConn, InMessage);
		}
		break;
	case idlepb::RpcMessageOp_Request:
		{
			OnRequest(InConn, InMessage);
		}
		break;
	case idlepb::RpcMessageOp_Response:
		{
			OnResponse(InConn, InMessage);
		}
		break;
	default:
		break;
	}
}

void FMRpcManager::OnNotify(FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
{
	
}

void FMRpcManager::OnRequest(FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
{
	if (const auto Ret = AllMethods.Find(InMessage->rpc_id()))
	{
		(*Ret)(InConn, InMessage);
	}
	else
	{
		SendResponse(InConn, InMessage->rpc_id(), InMessage->sn(), nullptr, idlepb::RpcErrorCode_Unimplemented);
	}
}

void FMRpcManager::OnResponse(FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
{
	const auto ReqSerialNum = InMessage->sn();
	const auto Ret = AllRequestPending.Find(ReqSerialNum);
	if (!Ret)
		return;

	const EPbRpcErrorCode ErrorCode = static_cast<EPbRpcErrorCode>(InMessage->error_code());
	Ret->Callback(ErrorCode, InMessage);

	AllRequestPending.Remove(ReqSerialNum);
}

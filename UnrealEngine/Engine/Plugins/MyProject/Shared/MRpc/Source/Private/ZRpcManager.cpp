#include "ZRpcManager.h"

#include "ZTools.h"

FZRpcManager::FZRpcManager()
{
	MessageDispatcher.Reg<idlezt::ZRpcMessage>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
	{
		OnRpcMessage(InConn, InMessage);
	});
	
}

uint64 FZRpcManager::CallRpc(FZPbMessageSupportBase* InConn, uint64 InRpcId, const FPbMessagePtr& InMessage, const FResponseCallback& Callback)
{
	const uint64 BodyTypeId = ZGeneratePbMessageTypeId(&*InMessage);
	const std::string& BodyData = InMessage->SerializeAsString();
	return CallRpc(InConn, InRpcId, BodyTypeId, BodyData.c_str(), BodyData.size(), Callback);
}

uint64 FZRpcManager::CallRpc(FZPbMessageSupportBase* InConn, uint64 InRpcId, uint64 InBodyTypeId, const char* InBodyDataPtr, int32 InBodyDataLength, const FResponseCallback& Callback)
{
	const uint64 SerialNum = SendRequest(InConn, InRpcId, InBodyTypeId, InBodyDataPtr, InBodyDataLength);

	RequestPendingData Data;
	Data.SerialNum = SerialNum;
	Data.Callback = Callback;
	Data.ExpireTimestamp = 0;
	AllRequestPending.Emplace(SerialNum, std::move(Data));

	return SerialNum;
}

void FZRpcManager::AddMethod(uint64 InRpcId, const FMethodCallback& InCallback)
{
	AllMethods.Emplace(InRpcId, InCallback);
}

uint64 FZRpcManager::SendRequest(FZPbMessageSupportBase* InConn, uint64 InRpcId, uint64 InBodyTypeId, const char* InBodyDataPtr, int32 InBodyDataLength)
{
	const uint64 SerialNum = NextSn++;
		
	auto Message = MakeShared<idlezt::ZRpcMessage>();
	Message->set_op(idlezt::RpcMessageOp_Request);
	Message->set_sn(SerialNum);
	Message->set_error_code(idlezt::RpcErrorCode_Ok);
	Message->set_rpc_id(InRpcId);
	Message->set_body_type_id(InBodyTypeId);
	Message->set_body_data(InBodyDataPtr, InBodyDataLength);
	InConn->SendMessage(Message);
	
	return SerialNum;
}

// static
void FZRpcManager::SendResponse(FZPbMessageSupportBase* InConn, uint64 InRpcId, uint64 InReqSerialNum, const FPbMessagePtr& InMessage, idlezt::RpcErrorCode ErrorCode)
{
	auto OutMessage = MakeShared<idlezt::ZRpcMessage>();
	OutMessage->set_op(idlezt::RpcMessageOp_Response);
	OutMessage->set_sn(InReqSerialNum);
	OutMessage->set_error_code(ErrorCode);
	OutMessage->set_rpc_id(InRpcId);
	if (InMessage)
	{
		const uint64 BodyTypeId = ZGeneratePbMessageTypeId(&*InMessage);
		const std::string& BodyData = InMessage->SerializeAsString();
		OutMessage->set_body_type_id(BodyTypeId);
		OutMessage->set_body_data(BodyData);
	}
	InConn->SendMessage(OutMessage);
}

// void FZRpcManager::OnMessage(FZPbMessageSupportBase* InConn, uint64 InCode, const FZDataBufferPtr& InData)
// {
// 	OnMessage(InConn, InCode, InData->Peek(), InData->ReadableBytes());
// }
//
// void FZRpcManager::OnMessage(FZPbMessageSupportBase* InConn, uint64 InCode, const char* InDataPtr, int32 InDataLen)
// {
// 	MessageDispatcher.Process(InConn, InCode, InDataPtr, InDataLen);	
// }

FZPbDispatcher& FZRpcManager::GetMessageDispatcher()
{
	return MessageDispatcher;
}

void FZRpcManager::OnRpcMessage(FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
{
	switch (InMessage->op())
	{
	case idlezt::RpcMessageOp_Notify:
		{
			OnNotify(InConn, InMessage);
		}
		break;
	case idlezt::RpcMessageOp_Request:
		{
			OnRequest(InConn, InMessage);
		}
		break;
	case idlezt::RpcMessageOp_Response:
		{
			OnResponse(InConn, InMessage);
		}
		break;
	default:
		break;
	}
	
}

void FZRpcManager::OnNotify(FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
{
	
}

void FZRpcManager::OnRequest(FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
{
	if (auto Ret = AllMethods.Find(InMessage->rpc_id()))
	{
		(*Ret)(InConn, InMessage);
	}
	else
	{
		SendResponse(InConn, InMessage->rpc_id(), InMessage->sn(), nullptr, idlezt::RpcErrorCode_Unimplemented);
	}
}

void FZRpcManager::OnResponse(FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
{
	const auto ReqSerialNum = InMessage->sn();
	auto Ret = AllRequestPending.Find(ReqSerialNum);
	if (!Ret)
		return;

	const EZRpcErrorCode ErrorCode = static_cast<EZRpcErrorCode>(InMessage->error_code());
	Ret->Callback(ErrorCode, InMessage);

	AllRequestPending.Remove(ReqSerialNum);
}

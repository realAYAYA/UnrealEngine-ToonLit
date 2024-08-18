#pragma once

#include "common.pb.h"
#include "PbCommon.h"

#include "MyTcpConnection.h"
#include "PbDispatcher.h"

class MRPC_API FMRpcManager
{
public:
	typedef TFunction<void(FZPbMessageSupportBase*, const TSharedPtr<idlezt::ZRpcMessage>&)> FMethodCallback;
	typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ZRpcMessage>&)> FResponseCallback;

	FMRpcManager();

	void TryInit(FZPbDispatcher& InDispatcher);
	
	uint64 CallRpc(FZPbMessageSupportBase* InConn, uint64 InRpcId, const FPbMessagePtr& InMessage, const FResponseCallback& Callback);
	uint64 CallRpc(FZPbMessageSupportBase* InConn, uint64 InRpcId, uint64 InBodyTypeId, const char* InBodyDataPtr, int32 InBodyDataLength, const FResponseCallback& Callback);	
	
	static void SendResponse(FZPbMessageSupportBase* InConn, uint64 InRpcId, uint64 InReqSerialNum, const FPbMessagePtr& InMessage, idlezt::RpcErrorCode ErrorCode);

	void AddMethod(uint64 InRpcId, const FMethodCallback& InCallback);

	// void OnMessage(FZPbMessageSupportBase* InConn,  uint64 InCode, const FZDataBufferPtr& InData);
	// void OnMessage(FZPbMessageSupportBase* InConn,  uint64 InCode, const char* InDataPtr, int32 InDataLen);
	
	FZPbDispatcher& GetMessageDispatcher();
	
private:

	uint64 SendRequest(FZPbMessageSupportBase* InConn, uint64 InRpcId, uint64 InBodyTypeId, const char* InBodyDataPtr, int32 InBodyDataLength);
	
	void OnRpcMessage(FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage);
	void OnNotify(FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage);
	void OnRequest(FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage);
	void OnResponse(FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage);

	FZPbDispatcher MessageDispatcher;
	
	uint64 NextSn = 1;

	struct RequestPendingData
	{
		uint64 SerialNum = 0;
		FResponseCallback Callback;
		int64 ExpireTimestamp = 0;
	};
	
	TMap<uint64, RequestPendingData> AllRequestPending;
	
	TMap<uint64, FMethodCallback> AllMethods;
};

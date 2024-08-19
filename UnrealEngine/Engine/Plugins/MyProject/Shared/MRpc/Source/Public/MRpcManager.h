#pragma once

#include "common.pb.h"
#include "PbCommon.h"
#include "PbDispatcher.h"

class MRPC_API FMRpcManager
{
	
public:
	
	typedef TFunction<void(FPbMessageSupportBase*, const TSharedPtr<idlepb::PbRpcMessage>&)> FMethodCallback;
	typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::PbRpcMessage>&)> FResponseCallback;

	FMRpcManager();

	void TryInit(FPbDispatcher& InDispatcher);
	
	uint64 CallRpc(FPbMessageSupportBase* InConn, uint64 InRpcId, const FPbMessagePtr& InMessage, const FResponseCallback& Callback);
	uint64 CallRpc(FPbMessageSupportBase* InConn, uint64 InRpcId, uint64 InBodyTypeId, const char* InBodyDataPtr, int32 InBodyDataLength, const FResponseCallback& Callback);	
	
	static void SendResponse(FPbMessageSupportBase* InConn, uint64 InRpcId, uint64 InReqSerialNum, const FPbMessagePtr& InMessage, idlepb::RpcErrorCode ErrorCode);

	void AddMethod(uint64 InRpcId, const FMethodCallback& InCallback);
	
	FPbDispatcher& GetMessageDispatcher();
	
private:

	uint64 SendRequest(FPbMessageSupportBase* InConn, uint64 InRpcId, uint64 InBodyTypeId, const char* InBodyDataPtr, int32 InBodyDataLength);
	
	void OnRpcMessage(FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage);
	void OnNotify(FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage);
	void OnRequest(FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage);
	void OnResponse(FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage);

	FPbDispatcher MessageDispatcher;
	
	uint64 NextSn = 1;

	struct FRequestPendingData
	{
		uint64 SerialNum = 0;
		FResponseCallback Callback;
		int64 ExpireTimestamp = 0;
	};
	
	TMap<uint64, FRequestPendingData> AllRequestPending;
	
	TMap<uint64, FMethodCallback> AllMethods;
};

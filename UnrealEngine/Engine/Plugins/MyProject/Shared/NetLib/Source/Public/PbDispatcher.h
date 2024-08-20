#pragma once
#include "Containers/Map.h"

#include "MTools.h"
#include "MyNetFwd.h"
#include "MyDataBuffer.h"

class NETLIB_API FPbDispatcher
{
	
public:

	template <typename PbType>
	void Reg(const TFunction<void(FPbMessageSupportBase*, const TSharedPtr<PbType>&)>& InCallback)
	{
		const static uint64 PbTypeId = FMyTools::GeneratePbMessageTypeId<PbType>();

		// 注册消息原型
		{
			PbType Dummy;
			RegMessagePrototype(PbTypeId, Dummy.GetDescriptor());
		}
		
		Callbacks.Emplace(PbTypeId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<FPbMessage>& InMessage)
		{
			if (auto Message = StaticCastSharedPtr<PbType>(InMessage))
			{
				InCallback(InConn, Message);
			}
		});
	}

	template <typename PbType>
	void UnReg()
	{
		const static uint64 PbTypeId = FMyTools::GeneratePbMessageTypeId<PbType>();
		Callbacks.Remove(PbTypeId);
	}
	
	bool Process(FPbMessageSupportBase* InConn, uint64 InCode, const char* InDataPtr, int32 InDataLen);
	bool Process(FPbMessageSupportBase* InConn, uint64 InCode, const TSharedPtr<FPbMessage>& InMessage);
	bool Process(FPbMessageSupportBase* InSession, uint64 InPbTypeId, const FMyDataBufferPtr& InData);

	void SetUnknownMessageCallback(const TFunction<void(const TSharedPtr<FPbMessage>&)>& Func);

	static TSharedPtr<FPbMessage> ParsePbFromArray(uint64 PbTypeId, const char* DataPtr, uint32 DataLength);
	static TSharedPtr<FPbMessage> ParsePbFromArray(uint64 PbTypeId, const FMyDataBufferPtr& InBuffer);
	
private:
	
	TMap<uint64, TFunction<void(FPbMessageSupportBase*, const TSharedPtr<FPbMessage>&)>> Callbacks;
	TFunction<void(const TSharedPtr<FPbMessage>&)> UnknownMessageCallback;
	
	static void RegMessagePrototype(uint64 InPbTypeId, const google::protobuf::Descriptor* InPbDescriptor);
	static TMap<uint64, const google::protobuf::Message*> MessagePrototypes;  // 消息Code和原型对应表
	
};

// ============================================================================
//  Protobuf 消息分发器
// 
//  对 SessionType 类型需求： 提供静态成员函数 GetMessageDispatcher ，示例如下
//  头文件：
//  class FLoginNetSession {
//      static TPbMessageDispatcher<FLoginNetSession>& GetMessageDispatcher();
//  };
//  源文件：
//  TPbMessageDispatcher<FLoginNetSession>& FLoginNetSession::GetMessageDispatcher() {
//      static TPbMessageDispatcher<FLoginNetSession> Dispatcher;
//      return Dispatcher;
//  }
//

class NETLIB_API FPbMessageDispatcherBase
{
	
public:
	
	FPbMessageDispatcherBase();
	virtual ~FPbMessageDispatcherBase();
	
	static TSharedPtr<FPbMessage> ParsePbFromArray(uint64 PbTypeId, const char* DataPtr, uint32 DataLength);
	static TSharedPtr<FPbMessage> ParsePbFromArray(uint64 PbTypeId, const FMyDataBufferPtr& InBuffer);

protected:
	
	static void RegMessagePrototype(uint64 InPbTypeId, const google::protobuf::Descriptor* InPbDescriptor);
	static TMap<uint64, const google::protobuf::Message*>& GetMessagePrototypes();
};

template <typename SessionType>
class TPbMessageDispatcher : public FPbMessageDispatcherBase
{
public:

	template <typename PbType>
	void Reg(const TFunction<void(SessionType*, const TSharedPtr<PbType>&)>& InCallback)
	{
		uint64 PbTypeId = FMyTools::GeneratePbMessageTypeId<PbType>();

		// 注册消息原型
		{
			PbType Dummy;
			RegMessagePrototype(PbTypeId, Dummy.GetDescriptor());
		}
		
		Callbacks.Emplace(PbTypeId, [InCallback](SessionType* InSession, const TSharedPtr<FPbMessage>& InMessage)
		{
			if (auto Message = StaticCastSharedPtr<PbType>(InMessage))
			{
				InCallback(InSession, Message);
			}
		});
	}

	bool Process(SessionType* InSession, uint64 InPbTypeId, const FMyDataBufferPtr& InData)
	{
		return Process(InSession, InPbTypeId, InData->Peek(), InData->ReadableBytes());
	}
	
	bool Process(SessionType* InSession, uint64 InPbTypeId, const char* InDataPtr, int32 InDataLen)
	{
		if (const auto Message = ParsePbFromArray(InPbTypeId, InDataPtr, InDataLen))
		{
			return Process(InSession, InPbTypeId, Message);
		}
		return false;
	}

	bool Process(SessionType* InSession, uint64 InPbTypeId, const TSharedPtr<FPbMessage>& InMessage)
	{
		auto *Callback = Callbacks.Find(InPbTypeId);
		if (Callback)
		{
			(*Callback)(InSession, InMessage);
			return true;
		}
		else
		{
			if (UnknownMessageCallback)
			{
				UnknownMessageCallback(InMessage);
			}
		}
		
		return false;
	}

	void SetUnknownMessageCallback(const TFunction<void(const TSharedPtr<FPbMessage>&)>& Func)
	{
		UnknownMessageCallback = Func;
	}

private:
	
	TMap<uint64, TFunction<void(SessionType*, const TSharedPtr<FPbMessage>&)>> Callbacks;
	TFunction<void(const TSharedPtr<FPbMessage>&)> UnknownMessageCallback;
};

#ifndef PB_MODULE_NAME
#define PB_MODULE_NAME PbMsgHandleDefault
#endif
#define PB_MESSAGE_HANDLE_REGISTRANT_TYPE_NAME PREPROCESSOR_JOIN(_PB_,PREPROCESSOR_JOIN(PB_MODULE_NAME,PREPROCESSOR_JOIN(_,__LINE__)))
#define PB_MESSAGE_HANDLE_REGISTRANT_VAR_NAME PREPROCESSOR_JOIN(_PBVAR_,PREPROCESSOR_JOIN(PB_MODULE_NAME,PREPROCESSOR_JOIN(_,__LINE__)))

#define PB_MESSAGE_HANDLE(SessionType, PbType, SessionVar, MessageVar) \
    struct PB_MESSAGE_HANDLE_REGISTRANT_TYPENAME \
    { \
        PB_MESSAGE_HANDLE_REGISTRANT_TYPENAME() \
        { \
            SessionType::GetMessageDispatcher().Reg<PbType>([this](FPbMessageSupportBase* InSession, const TSharedPtr<PbType>& InMessage) \
            { \
				SessionType* Session = static_cast<SessionType*>(InSession); \
                this->Handle(Session, InMessage); \
            }); \
        } \
        void Handle(SessionType*, const TSharedPtr<PbType>&); \
    };    \
    static PB_MESSAGE_HANDLE_REGISTRANT_TYPENAME PB_MESSAGE_HANDLE_REGISTRANT_VARNAME; \
    void PB_MESSAGE_HANDLE_REGISTRANT_TYPENAME::Handle(SessionType* SessionVar, const TSharedPtr<PbType>& MessageVar)

#define PB_RPC_HANDLE(SessionType, RpcInterfaceName, RpcMethodName, GameSessionVar, ReqVar, RspVar) \
	struct PREPROCESSOR_JOIN(FPbRpc_,PREPROCESSOR_JOIN(RpcInterfaceName,RpcMethodName)) \
	{ \
		PREPROCESSOR_JOIN(FPbRpc_,PREPROCESSOR_JOIN(RpcInterfaceName,RpcMethodName))() \
		{ \
			PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcInterfaceName,Interface))::PREPROCESSOR_JOIN(RpcMethodName,Register)(&SessionType::GetRpcManager(), [this](FPbMessageSupportBase* InSession, const PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcInterfaceName,Interface))::PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcMethodName,ReqPtr))& InReqPtr, const PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcInterfaceName,Interface))::PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcMethodName,RspPtr))& InRspPtr) {\
				SessionType* Session = static_cast<SessionType*>(InSession); \
				this->Handle(Session, InReqPtr, InRspPtr); \
			});\
		} \
		void Handle(SessionType*, const PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcInterfaceName,Interface))::PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcMethodName,ReqPtr))& ReqPtr, const PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcInterfaceName,Interface))::PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcMethodName,RspPtr))& RspPtr); \
	};	\
	static PREPROCESSOR_JOIN(FPbRpc_,PREPROCESSOR_JOIN(RpcInterfaceName,RpcMethodName)) PREPROCESSOR_JOIN(_PbRpcVar_,PREPROCESSOR_JOIN(RpcInterfaceName,RpcMethodName)); \
	void PREPROCESSOR_JOIN(FPbRpc_,PREPROCESSOR_JOIN(RpcInterfaceName,RpcMethodName))::Handle(SessionType* GameSessionVar, const PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcInterfaceName,Interface))::PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcMethodName,ReqPtr))& ReqVar, const PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcInterfaceName,Interface))::PREPROCESSOR_JOIN(FPb,PREPROCESSOR_JOIN(RpcMethodName,RspPtr))& RspVar)

#include "PbDispatcher.h"
#include "google/protobuf/message.h"

bool FPbDispatcher::Process(FPbMessageSupportBase* InConn, const uint64 InCode, const char* InDataPtr, const int32 InDataLen)
{
	if (const auto Message = ParsePbFromArray(InCode, InDataPtr, InDataLen))
	{
		return Process(InConn, InCode, Message);
	}
	return false;
}

bool FPbDispatcher::Process(FPbMessageSupportBase* InConn, const uint64 InCode, const TSharedPtr<FPbMessage>& InMessage)
{
	if (const auto *Callback = Callbacks.Find(InCode))
	{
		(*Callback)(InConn, InMessage);
		return true;
	}
	
	if (UnknownMessageCallback)
	{
		UnknownMessageCallback(InMessage);
	}
	
	return false;
}

bool FPbDispatcher::Process(FPbMessageSupportBase* InSession, const uint64 InPbTypeId, const FMyDataBufferPtr& InData)
{
	return Process(InSession, InPbTypeId, InData->Peek(), InData->ReadableBytes());
}

void FPbDispatcher::SetUnknownMessageCallback(const TFunction<void(const TSharedPtr<FPbMessage>&)>& Func)
{
	UnknownMessageCallback = Func;
}


// static
TMap<uint64, const google::protobuf::Message*> FPbDispatcher::MessagePrototypes;

// static
TSharedPtr<FPbMessage> FPbDispatcher::ParsePbFromArray(const uint64 PbTypeId, const char* DataPtr, const uint32 DataLength)
{
	const google::protobuf::Message* PbPrototype = nullptr;
	{
		if (const auto* Ret = MessagePrototypes.Find(PbTypeId))
		{
			PbPrototype = *Ret;
		}
	}
	if (!PbPrototype)
	{
		// UE_LOG(LogNetLib, Error, TEXT("[网络模块] ParsePbFromArray 找不到Pb消息原型 Code=%llu Length=%d"), PbCode, DataLength);
		return nullptr;
	}

	TSharedPtr<FPbMessage> Message(PbPrototype->New());
	if (!Message->ParseFromArray(DataPtr, DataLength))
	{
		UE_LOG(LogNetLib, Error, TEXT("[网络模块] ParsePbFromArray 解析消息失败 Code=%llu Length=%d"), PbTypeId, DataLength);
		return nullptr;
	}
	return Message;
}

TSharedPtr<FPbMessage> FPbDispatcher::ParsePbFromArray(uint64 PbTypeId, const FMyDataBufferPtr& InBuffer)
{
	return ParsePbFromArray(PbTypeId, InBuffer->Peek(), InBuffer->ReadableBytes());
}

// static
void FPbDispatcher::RegMessagePrototype(uint64 InPbTypeId, const google::protobuf::Descriptor* InPbDescriptor)
{
	google::protobuf::MessageFactory* PbFactory = google::protobuf::MessageFactory::generated_factory();
	const google::protobuf::Message* PbPrototype = PbFactory->GetPrototype(InPbDescriptor);
	if (!PbPrototype)
	{
		UE_LOG(LogNetLib, Error, TEXT("[网络模块] RegMessagePrototype 找不到消息原型 PbTypeId=%llu"), InPbTypeId);
		return;
	}
	
	MessagePrototypes.Emplace(InPbTypeId, PbPrototype);	
}

// ============================================================================


FPbMessageDispatcherBase::FPbMessageDispatcherBase()
{
}

FPbMessageDispatcherBase::~FPbMessageDispatcherBase()
{
}

TSharedPtr<FPbMessage> FPbMessageDispatcherBase::ParsePbFromArray(const uint64 PbTypeId, const char* DataPtr, const uint32 DataLength)
{
	const google::protobuf::Message* PbPrototype = nullptr;
	{
		if (const auto* Ret = GetMessagePrototypes().Find(PbTypeId))
		{
			PbPrototype = *Ret;
		}
	}
	if (!PbPrototype)
	{
		// UE_LOG(LogNetLib, Error, TEXT("[网络模块] ParsePbFromArray 找不到Pb消息原型 Code=%llu Length=%d"), PbCode, DataLength);
		return nullptr;
	}

	TSharedPtr<FPbMessage> Message(PbPrototype->New());
	if (!Message->ParseFromArray(DataPtr, DataLength))
	{
		UE_LOG(LogNetLib, Error, TEXT("[网络模块] ParsePbFromArray 解析消息失败 Code=%llu Length=%d"), PbTypeId, DataLength);
		return nullptr;
	}
	
	return Message;	
}

TSharedPtr<FPbMessage> FPbMessageDispatcherBase::ParsePbFromArray(const uint64 PbTypeId, const FMyDataBufferPtr& InBuffer)
{
	return ParsePbFromArray(PbTypeId, InBuffer->Peek(), InBuffer->ReadableBytes());
}

void FPbMessageDispatcherBase::RegMessagePrototype(uint64 InPbTypeId, const google::protobuf::Descriptor* InPbDescriptor)
{
	google::protobuf::MessageFactory* PbFactory = google::protobuf::MessageFactory::generated_factory();
	const google::protobuf::Message* PbPrototype = PbFactory->GetPrototype(InPbDescriptor);
	if (!PbPrototype)
	{
		UE_LOG(LogNetLib, Error, TEXT("[网络模块] RegMessagePrototype 找不到消息原型 PbTypeId=%llu"), InPbTypeId);
		return;
	}
	
	GetMessagePrototypes().Emplace(InPbTypeId, PbPrototype);		
}

TMap<uint64, const google::protobuf::Message*>& FPbMessageDispatcherBase::GetMessagePrototypes()
{
	static TMap<uint64, const google::protobuf::Message*> MessagePrototypes;  // 消息Code和原型对应表
	return MessagePrototypes;
}

#include "NatsClientImpl.h"

#include <string>

#include "NatsClientModule.h"
#include "Misc/Fnv.h"


// ============================================================================

static void OnNatsMessage(natsConnection* nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
	// const char* SubjectName = natsMsg_GetSubject(msg);
	// const char* DataPtr = natsMsg_GetData(msg);
	// const int32 DataLength = natsMsg_GetDataLength(msg);

	if (closure)
	{
		auto* Data = static_cast<FNatsSubscriptionData*>(closure);
		Data->OnReceivedMessage(msg);
	}
	

	// hudawei: 放到队列里面， 这里不删除
	// natsMsg_Destroy(msg);
}

static std::string MakeSubjectName(const FString& InName)
{
	static FString Prefix(TEXT("IDLEZ"));
#if WITH_EDITOR
	static FString MachineId;
	if (MachineId.IsEmpty())
	{
		FString Name = FPlatformProcess::ComputerName();
		uint64 HashId = FFnv::MemFnv64(*Name, Name.Len() * sizeof(TCHAR));
		MachineId = BytesToHex(reinterpret_cast<const uint8*>(&HashId), sizeof(HashId));
	}
	
	FString Ret = FString::Printf(TEXT("%s:%s-%s"), *Prefix, *MachineId, *InName);  // 编辑器模式将当前机器名编码到KEY里面
	return TCHAR_TO_UTF8(*Ret);
#else
	FString Ret = FString::Printf(TEXT("%s_%s"), *Prefix, *InName);
	return TCHAR_TO_UTF8(*Ret);
#endif
}

// ============================================================================

FNatsSubscriptionData::~FNatsSubscriptionData()
{
	if (RawSubscription)
	{
		Cleanup();
	}
}


void FNatsSubscriptionData::Cleanup()
{
	// 确保移除所有还未处理的消息
	{
		{
			natsMsg* RawMsg = nullptr;
			while (IncomingQueue.Dequeue(RawMsg))
			{
				natsMsg_Destroy(RawMsg);	
			}
		}
		{
			natsMsg* RawMsg = nullptr;
			while (WorkQueue.Dequeue(RawMsg))
			{
				natsMsg_Destroy(RawMsg);	
			}
		}
	}

	// 退订主题
	if (RawSubscription)
	{
		natsSubscription_Unsubscribe(RawSubscription);
		natsSubscription_Destroy(RawSubscription);
		RawSubscription = nullptr;
	}
}

void FNatsSubscriptionData::OnReceivedMessage(natsMsg* RawMessage)
{
	FScopeLock Lock(&Mutex);
	IncomingQueue.Enqueue(RawMessage);
}

void FNatsSubscriptionData::ReloadWorkQueue()
{
	if (!WorkQueue.IsEmpty())
		return;

	{
		FScopeLock Lock(&Mutex);
		Swap(IncomingQueue, WorkQueue);
	}	
}

void FNatsSubscriptionData::Tick()
{
	do
	{
		ReloadWorkQueue();
		if (WorkQueue.IsEmpty())
			break;
		natsMsg* RawMsg = nullptr;
		while (WorkQueue.Dequeue(RawMsg))
		{
			if (MessageCallback)
			{
				const char* DataPtr = natsMsg_GetData(RawMsg);
				const int32 DataLength = natsMsg_GetDataLength(RawMsg);
				MessageCallback(DataPtr, DataLength);
			}
			natsMsg_Destroy(RawMsg);
		}
	} while(false);
}

// ============================================================================

FNatsClientImpl::FNatsClientImpl()
{
}

FNatsClientImpl::~FNatsClientImpl()
{
}

bool FNatsClientImpl::IsConnected() const
{
	return RawConnection != nullptr;
}

bool FNatsClientImpl::ConnectTo(const FString& Url)
{
	if (RawConnection)
	{
		return false;
	}
	
	LastUrl = Url;
	std::string AnsiUrl = TCHAR_TO_ANSI(*Url);
	
	natsStatus Status = natsConnection_ConnectTo(&RawConnection, AnsiUrl.c_str());
	if (Status != NATS_OK)
	{
		return false;
	}

	return true;
}

void FNatsClientImpl::Close()
{
	if (!RawConnection)
	{
		return;
	}

	natsConnection_Close(RawConnection);
	natsConnection_Destroy(RawConnection);
	RawConnection = nullptr;
}

void FNatsClientImpl::Tick()
{
	for (auto& Elem : Subscriptions)
	{
		Elem.second->Tick();
	}
}

bool FNatsClientImpl::Subscribe(const FString& Subject, const TFunction<void(const char* DataPtr, int32 DataLength)>& Callback)
{
	const std::string SubjectName = MakeSubjectName(Subject);
	if (Subscriptions.find(SubjectName) != Subscriptions.end())
	{
		return false;
	}
	
	auto DataPtr = std::make_shared<FNatsSubscriptionData>();
	
	natsSubscription* Sub = nullptr;
	void* Closure = DataPtr.get();
	natsStatus Status = natsConnection_Subscribe(&Sub, RawConnection, SubjectName.c_str(), OnNatsMessage, Closure);
	if (Status != NATS_OK)
	{
		return false;
	}
	
	DataPtr->Subject = Subject;
	DataPtr->RawSubscription = Sub;
	DataPtr->MessageCallback = Callback;

	Subscriptions.emplace(SubjectName, DataPtr);
	
	return true;
}

bool FNatsClientImpl::Unsubscribe(const FString& Subject)
{
	const std::string SubjectName = MakeSubjectName(Subject);
	auto Ret = Subscriptions.find(SubjectName);
	if (Ret == Subscriptions.end())
	{
		return false;
	}
	
	Ret->second->Cleanup();
	Subscriptions.erase(Ret);
	
	return true;
}

bool FNatsClientImpl::Publish(const FString& Subject, const char* DataPtr, int32 DataLength)
{
	if (!RawConnection)
	{
		return false;
	}

	const std::string SubjectName = MakeSubjectName(Subject);
	natsStatus Status = natsConnection_Publish(RawConnection, SubjectName.c_str(), DataPtr, DataLength);
	if (Status != NATS_OK)
	{
		return false;
	}
	
	return true;
}

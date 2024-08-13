#pragma once

#include "NatsClient.h"
#include "NatsLibrary.h"

#include <string>
#include <map>
#include <memory>

class FNatsSubscriptionData
{
public:
	~FNatsSubscriptionData();

	void Cleanup();
	void OnReceivedMessage(natsMsg* RawMessage);  // 收到消息 [**NATS线程调用**]  
	void Tick();
	
	FString Subject;
	natsSubscription* RawSubscription = nullptr;
	TFunction<void(const char* DataPtr, int32 DataLength)> MessageCallback;

private:
	
	void ReloadWorkQueue();
	FCriticalSection Mutex;
	TQueue<natsMsg*> IncomingQueue;
	TQueue<natsMsg*> WorkQueue;
};

class FNatsClientImpl : public INatsClient
{
public:
	FNatsClientImpl();
	virtual ~FNatsClientImpl() override;

	virtual bool IsConnected() const override;
	
	virtual bool ConnectTo(const FString& Url) override;
	virtual void Close() override;

	virtual void Tick() override;
	
	virtual bool Subscribe(const FString& Subject, const TFunction<void(const char* DataPtr, int32 DataLength)>& Callback) override;
	virtual bool Unsubscribe(const FString& Subject) override;	

	virtual bool Publish(const FString& Subject, const char* DataPtr, int32 DataLength) override;
	
private:

	FString LastUrl;
	natsConnection* RawConnection = nullptr;
	
	std::map<std::string, std::shared_ptr<FNatsSubscriptionData>> Subscriptions;
	
};

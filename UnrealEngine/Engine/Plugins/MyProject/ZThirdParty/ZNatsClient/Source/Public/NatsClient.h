#pragma once

class ZNATSCLIENT_API INatsClient
{
public:
	virtual ~INatsClient() {};

	virtual bool IsConnected() const = 0;
	
	virtual bool ConnectTo(const FString& Url) = 0;
	virtual void Close() = 0;

	virtual void Tick() = 0;
	
	virtual bool Subscribe(const FString& Subject, const TFunction<void(const char* DataPtr, int32 DataLength)>& Callback) = 0;
	virtual bool Unsubscribe(const FString& Subject) = 0;

	virtual bool Publish(const FString& Subject, const char* DataPtr, int32 DataLength) = 0;
	
};

typedef TSharedPtr<INatsClient> FNatsClientPtr;

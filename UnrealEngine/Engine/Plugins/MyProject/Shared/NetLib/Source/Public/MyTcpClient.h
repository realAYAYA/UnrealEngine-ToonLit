#pragma once
#include "MyNetFwd.h"

class NETLIB_API FMyTcpClient
{
	
public:
	
	virtual ~FMyTcpClient() = default;

	bool Connect(const FString& ServerURL, const FString& ServerProtocol);
	void Shutdown();
	bool IsConnected() const;

protected:

	virtual void Tick(float DeltaTime);
	
	virtual void OnMessage(uint64 InPbTypeId, const FMyDataBufferPtr& InPackage) = 0;
	virtual void OnConnected() = 0;
	virtual void OnDisconnected() = 0;
	virtual void OnError() = 0;
	
	FTcpConnectionPtr Connection;
};

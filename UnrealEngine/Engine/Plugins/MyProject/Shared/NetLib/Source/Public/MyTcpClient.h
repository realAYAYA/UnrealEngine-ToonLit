#pragma once
#include "MyNetFwd.h"

class NETLIB_API FMyTcpClient
{
	
public:

	void Init(const FString& ServerURL, const FString& ServerProtocol);
	
	void Connect();
	void Shutdown();
	bool IsConnected() const;
	void Tick(float DeltaTime);

protected:

	FString Url;
	FString Protocol;
	
	FTcpConnectionPtr Connection;
};

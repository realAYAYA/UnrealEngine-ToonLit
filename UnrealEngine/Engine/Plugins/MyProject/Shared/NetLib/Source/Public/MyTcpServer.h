#pragma once
#include "MyNetFwd.h"

struct FTcpServerImpl;

class NETLIB_API FMyTcpServer
{
	
public:
	
	FMyTcpServer();
	virtual ~FMyTcpServer() = default;

	void Start(const int32 ServerPort = 0);
	void Stop();

	bool IsRunning() const;

	void Tick(float DeltaTime);

protected:

	void RemoveConnection(const FTcpConnectionPtr& InConn);
	
	uint64 GenerateConnId();
	virtual FTcpConnectionPtr NewConnection() = 0;
	
private:
	
	TUniquePtr<FTcpServerImpl> ServerImpl;
	int32 ListenPort;
	std::atomic<uint64> NextConnId;

	TMap<uint64, FTcpConnectionPtr> Connections;

	FCriticalSection Mutex;
};


class NETLIB_API FPbTcpServer : public FMyTcpServer
{
	
public:

private:
	
	virtual FTcpConnectionPtr NewConnection() override;
};
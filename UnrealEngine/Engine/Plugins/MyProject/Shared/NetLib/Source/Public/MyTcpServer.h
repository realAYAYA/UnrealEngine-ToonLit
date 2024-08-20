#pragma once
#include "MyNetFwd.h"
#include "MyTcpConnection.h"

struct FTcpServerImpl;

class NETLIB_API FMyTcpServer
{
	
public:
	
	FMyTcpServer();
	virtual ~FMyTcpServer();

	bool Start(const int32 ServerPort = 0);
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

	void SetPackageCallback(const FPbConnection::FPackageCallback& InCallback)
	{
		PackageCallback = InCallback;
	}

	void SetConnectedCallback(const FPbConnection::FConnectedCallback& InCallback)
	{
		ConnectedCallback = InCallback;
	}

	void SetDisconnectedCallback(const FPbConnection::FDisconnectedCallback& InCallback)
	{
		DisconnectedCallback = InCallback;
	}

	void SetAsyncMessageCallback(const FPbConnection::FErrorCallback& InCallback)
	{
		ErrorCallback = InCallback;
	}
	
private:
	
	virtual FTcpConnectionPtr NewConnection() override;

	FPbConnection::FPackageCallback PackageCallback;
	FPbConnection::FConnectedCallback ConnectedCallback;
	FPbConnection::FDisconnectedCallback DisconnectedCallback;
	FPbConnection::FErrorCallback ErrorCallback;
};
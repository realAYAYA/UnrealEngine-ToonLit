#pragma once
#include "MyDataBuffer.h"
#include "MyNetFwd.h"

class IWebSocket;
class INetworkingWebSocket;

typedef TFunction<void(FMyDataBuffer *)>	FMySocketReceivedCallback;
typedef TFunction<void()>					FMySocketConnectedCallback;
typedef TFunction<void()>					FMySocketErrorCallback;
typedef TFunction<void()>					FMySocketClosedCallback;
typedef TFunction<void()>					FMySocketTimerCallback;


// 网络连接类实现，这是一个隔离类，是对外部网络库进行的封装
// 如果希望更换网络库，则请对该类改写
class FMySocket : public TSharedFromThis<FMySocket, ESPMode::ThreadSafe>
{
	
public:

	FMySocket(const uint64 InId = 0);
	virtual ~FMySocket();
	
	FMySocket(const FMySocket&) = delete;
	FMySocket& operator=(const FMySocket&) = delete;

	uint64 GetId() const { return Id; }
	virtual bool IsOpen() const { return false; }

	virtual void Start() {}
	virtual void Shutdown() {}
	
	virtual void Send(const FMyDataBufferPtr& Buffer) {}

	FDateTime GetLastSentTime() const { return FDateTime(LastSentTime); }
	FDateTime GetLastReceivedTime() const { return FDateTime(LastReceivedTime); }

	void SetConnectedCallback(const FMySocketConnectedCallback& Func)	{ ConnectedCallback = Func; }
	void SetReceivedCallback(const FMySocketReceivedCallback& Func)		{ ReceivedCallback = Func; }
	void SetErrorCallback(const FMySocketErrorCallback& Func)			{ ErrorCallback = Func; }
	void SetClosedCallback(const FMySocketClosedCallback& Func)			{ ClosedCallback = Func; }

	void HandleReceived(FMyDataBuffer* Buffer);
	void HandleConnected();
	void HandleClosed();
	void HandleError();
	
	//void SetTimerCallback(const FMySocketTimerCallback& Func);

protected:

	const uint64 Id;
	std::atomic<int64> LastSentTime;// 最后发送数据时间
	std::atomic<int64> LastReceivedTime;// 最后接受数据时间
	
	FMySocketConnectedCallback ConnectedCallback;// 连接成功 Todo 但不知在服务器端何用
	FMySocketReceivedCallback ReceivedCallback;  // 接收到数据之回调 [**IO线程调用**]
	FMySocketErrorCallback ErrorCallback;  // 连接断开之回调 [**IO线程调用**]
	FMySocketClosedCallback ClosedCallback;  // 连接断开之回调 [**IO线程调用**]
	FMySocketTimerCallback TimerCallback;  // 连接定时器之回调 [**IO线程调用**]
};


// 服务器专用Tcp连接类
class FMySocketServer : public FMySocket
{
	
public:

	FMySocketServer(const uint64 InId): FMySocket(InId) {}
	virtual ~FMySocketServer() override {}

	void Init(INetworkingWebSocket* Socket);

	FMySocketServer(const FMySocketServer&) = delete;
	FMySocketServer& operator=(const FMySocketServer&) = delete;
	
	virtual bool IsOpen() const override;

	virtual void Start() override;
	virtual void Shutdown() override;
	
	virtual void Send(const FMyDataBufferPtr& Buffer) override;

private:
	
	TSharedPtr<INetworkingWebSocket> WebSocket = nullptr;
};

// 客户端专用Tcp连接类
class FMySocketClient : public FMySocket
{
	
public:

	void Init(const FString& ServerURL, const FString& ServerProtocol);
	
	virtual bool IsOpen() const override;

	virtual void Start() override;
	virtual void Shutdown() override;
	
	virtual void Send(const FMyDataBufferPtr& Buffer) override;

private:
	
	FString Url;
	FString Protocol;

	TSharedPtr<IWebSocket> WebSocket;
};

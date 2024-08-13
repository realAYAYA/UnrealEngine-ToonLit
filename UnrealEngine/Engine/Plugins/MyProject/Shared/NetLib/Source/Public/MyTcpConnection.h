#pragma once
#include "Templates/SharedPointer.h"

#include "MyNetFwd.h"
#include "MySocket.h"

// Tcp 连接实现

class NETLIB_API FMyTcpConnection : public TSharedFromThis<FMyTcpConnection, ESPMode::ThreadSafe>
{
	
public:

	FMyTcpConnection(const int64 Id);
	virtual ~FMyTcpConnection();
	
	uint64 GetId() const { return Id; }
	bool IsOpen() const { return Socket && Socket->IsOpen(); }

	void Init(const FMySocketPtr& InSocket);
	
	void Start();
	void Shutdown() const;

	FDateTime GetLastSentTime() const;
	FDateTime GetLastReceivedTime() const;

protected:

	void SendRaw(const char* Ptr, uint32 Length) const;
	void SendRaw(const FMyDataBufferPtr& Data) const;

	uint32 GenerateSerialNum();
	
private:

	virtual void HandleReceived(FMyDataBuffer* Buffer) = 0;  // 处理收到的数据 [**IO线程调用**]
	virtual void HandleConnected() = 0;  // [**IO线程调用**]
	virtual void HandleClosed() = 0;  // [**IO线程调用**]
	virtual void HandleError() = 0;  // [**IO线程调用**]
	virtual void HandleTimer() = 0;  // [**IO线程调用**]
	
	const uint64 Id = 0;
	FMySocketPtr Socket;// 网络连接的实例
	uint32 NextSerialNum = 1;
};


// 这是一个具体的基于Protobuf消息的Tcp连接实现
class NETLIB_API FPbConnection : public FMyTcpConnection, public FPbMessageSupportBase
{
	
public:

	FPbConnection(const uint64 Id);
	virtual ~FPbConnection() override;

	typedef TSharedPtr<FPbConnection, ESPMode::ThreadSafe> FSelfType;
	typedef FPbMessagePtr FMessageType;
	typedef TFunction<void(FSelfType, uint64, FMyDataBufferPtr)> FPackageCallback;
	typedef TFunction<void(FSelfType)> FConnectedCallback;
	typedef TFunction<void(FSelfType)> FDisconnectedCallback;
	typedef TFunction<void(FSelfType)> FErrorCallback;
	typedef TFunction<void(FSelfType)> FTimerCallback;

	virtual void SendMessage(const FPbMessagePtr& InMessage) override;
	
	void Send(const FPbMessagePtr& InMessage);
	void SendSerializedPb(uint64 PbTypeId, const char* InDataPtr, int32 InDataLen);

	void SetPackageCallback(const FPackageCallback& InCallback);
	void SetConnectedCallback(const FConnectedCallback& InCallback);
	void SetDisconnectedCallback(const FDisconnectedCallback& InCallback);
	void SetTimerCallback(const FTimerCallback& InCallback);

private:

	virtual void HandleReceived(FMyDataBuffer* InBuffer) override;  // [**IO线程调用**]
	virtual void HandleConnected() override;  // [**IO线程调用**]
	virtual void HandleClosed() override;  // [**IO线程调用**]
	virtual void HandleError() override;  // [**IO线程调用**]
	virtual void HandleTimer() override;  // [**IO线程调用**]

	void DirectSendPb(const FPbMessagePtr& InMessage);
	void DirectSendPb(uint64 PbTypeId, const char* InDataPtr, int32 InDataLen);

	void DirectSendCompressData(uint64 PbTypeId, const char* InDataPtr, int32 InDataLen);
	
	FPackageCallback PackageCallback;
	FConnectedCallback ConnectedCallback;
	FDisconnectedCallback DisconnectedCallback;
	FErrorCallback ErrorCallback;
	FTimerCallback TimerCallback;
};
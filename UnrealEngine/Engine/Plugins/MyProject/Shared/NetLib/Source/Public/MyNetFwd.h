#pragma once
#include "Templates/SharedPointer.h"

class FMyDataBuffer;
typedef TSharedPtr<FMyDataBuffer, ESPMode::ThreadSafe> FMyDataBufferPtr;

class FMySocket;
typedef TSharedPtr<FMySocket, ESPMode::ThreadSafe> FMySocketPtr;

class FMyTcpConnection;
typedef TSharedPtr<FMyTcpConnection, ESPMode::ThreadSafe> FTcpConnectionPtr;

class FPbConnection;
typedef TSharedPtr<FPbConnection, ESPMode::ThreadSafe> FPbConnectionPtr;
typedef TWeakPtr<FPbConnection, ESPMode::ThreadSafe> FPbConnectionWeakPtr;

// ----------------------------------------------------------------------------

namespace google {
	namespace protobuf {
		class Message;
		class Descriptor;
	} /* namespace protobuf */
} /* namespace google */
typedef google::protobuf::Message FPbMessage;
typedef TSharedPtr<FPbMessage> FPbMessagePtr;

// ----------------------------------------------------------------------------

class FPbMessageSupportBase
{
	
public:

	virtual ~FPbMessageSupportBase() {}
	virtual void SendMessage(const FPbMessagePtr& InMessage) {}
};

DECLARE_LOG_CATEGORY_EXTERN(LogNetLib, Log, All);
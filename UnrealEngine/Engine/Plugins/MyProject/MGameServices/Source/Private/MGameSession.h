#pragma once

#include "GameRpcInterface.h"

class UMWorld;
class UMPlayer;
class FMPlayer;


class FMGameSession : public FPbMessageSupportBase
{
	
public:

	FMGameSession(const FPbConnectionPtr& InConn);
	virtual ~FMGameSession() override;

	virtual void SendMessage(const FPbMessagePtr& InMessage) override;

	void Send(const TSharedPtr<FPbMessage>& InMessage) const;
	
	void OnMessage(uint64 InPbTypeId, const FMyDataBufferPtr& InPackage);
	void OnConnected();
	void OnDisconnected();

	FDateTime GetLastSentTime() const;
	FDateTime GetLastReceivedTime() const;

	static FPbDispatcher& GetMessageDispatcher();
	static FMRpcManager& GetRpcManager();
	
	const uint64 ConnId;
	
	FPbConnectionPtr Connection;

	FString Account;
	uint64 UserId = 0;
	
	FMPlayer* Player;
};

typedef TSharedPtr<FMGameSession> FMGameSessionPtr;
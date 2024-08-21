#include "MGameSession.h"

#include "MGameServerPrivate.h"
#include "GameServices/MPlayer.h"

FMGameSession::FMGameSession(const FPbConnectionPtr& InConn): ConnId(InConn->GetId()), Connection(InConn)
{
	Player = nullptr;
}

FMGameSession::~FMGameSession()
{
	if (Player)
	{
		Player->Offline(this);
		Player = nullptr;
	}
}

void FMGameSession::SendMessage(const FPbMessagePtr& InMessage)
{
	if (!Connection)
		return;

	Connection->Send(InMessage);
}

void FMGameSession::Send(const TSharedPtr<FPbMessage>& InMessage) const
{
	if (!Connection)
		return;
	
	Connection->Send(InMessage);
}

void FMGameSession::OnMessage(uint64 InPbTypeId, const FMyDataBufferPtr& InPackage)
{
	if (!Connection)
		return;

	if (!GetMessageDispatcher().Process(this, InPbTypeId, InPackage))
	{
		
	}
}

void FMGameSession::OnConnected()
{
	UE_LOG(LogMGameServices, Display, TEXT("OnConnected ConnId=%llu"), ConnId);
}

void FMGameSession::OnDisconnected()
{
	Connection.Reset();
	UE_LOG(LogMGameServices, Display, TEXT("OnDisconnected ConnId=%llu Account=%s"), ConnId, *Account);

	if (Player)
	{
		Player->Offline(this);
		Player = nullptr;
	}
}

FDateTime FMGameSession::GetLastSentTime() const
{
	if (!Connection)
		return 0;
	return Connection->GetLastSentTime();
}

FDateTime FMGameSession::GetLastReceivedTime() const
{
	if (!Connection)
		return 0;
	return Connection->GetLastReceivedTime();
}

FPbDispatcher& FMGameSession::GetMessageDispatcher()
{
	return GetRpcManager().GetMessageDispatcher();
}

FMRpcManager& FMGameSession::GetRpcManager()
{
	static FMRpcManager RpcManager;
	return RpcManager;
}

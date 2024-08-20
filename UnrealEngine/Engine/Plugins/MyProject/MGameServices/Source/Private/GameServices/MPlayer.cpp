#include "MPlayer.h"

#include "MGameSession.h"
#include "RedisOp.h"

FMPlayer::FMPlayer()
{
}

FMPlayer::~FMPlayer()
{
}

bool FMPlayer::Init(const uint64 InPlayerId, const FString& InAccount)
{
	PlayerId = InPlayerId;
	Account = InAccount;
	return true;
}

void FMPlayer::Cleanup()
{
}

void FMPlayer::Online(FMGameSession* InSession)
{
	check(Session == nullptr);

	Session = InSession;

	{
		// Todo  各个功能模块上线逻辑
	}

	PlayerData.last_online_date = 0;

	// 日志，角色上线
}

void FMPlayer::Offline(const FMGameSession* InSession)
{
	check(Session == nullptr);

	Session = nullptr;

	{
		// Todo  各个功能模块下线逻辑
	}

	MarkNeedSave();
}

FMGameSession* FMPlayer::GetSession() const
{
	return Session;
}

uint64 FMPlayer::GetConnId() const
{
	return Session ? Session->ConnId : 0;
}

void FMPlayer::SendToMe(const TSharedPtr<FPbMessage>& InMessage) const
{
	if (Session)
		Session->Send(InMessage);
}

bool FMPlayer::IsOnline() const
{
	return Session != nullptr;
}

bool FMPlayer::Load()
{
	idlepb::PlayerSaveData SaveData;
	FRedisOp::LoadPlayerData(GetPlayerID(), &SaveData);

	PlayerData.FromPb(SaveData.player_data());

	if (PlayerData.player_id == 0)
	{
		return false;
	}

	if (PlayerData.create_date == 0)
		PlayerData.create_date = FMyTools::Now().GetTicks();
	
	// Todo 初始化各个功能模块

	return true;
}

void FMPlayer::Save()
{
	idlepb::PlayerSaveData SaveData;

	{
		PlayerData.ToPb(SaveData.mutable_player_data());
		
		// Todo 各个功能模块数据序列化
	}

	FRedisOp::SavePlayerData(GetPlayerID(), SaveData);
}

void FMPlayer::MarkNeedSave(bool bImmediately)
{
}


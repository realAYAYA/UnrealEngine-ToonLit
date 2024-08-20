#include "MPlayerManager.h"
#include "MPlayer.h"

FMPlayerManager::~FMPlayerManager()
{
}

void FMPlayerManager::Init()
{
	AllEntities.Empty(2048);
}

void FMPlayerManager::Shutdown()
{
	Cleanup();
}

void FMPlayerManager::Cleanup()
{
	Foreach([this](FMPlayer* InRole) -> bool
	{
		DeletePlayer(InRole);
		return true;
	});

	ProcessJunk();	
}

void FMPlayerManager::Tick(float DeltaTime)
{
	// Todo 玩家延迟离线
	const FDateTime Now = FDateTime::UtcNow();
	if ((Now - LastProcessTime).GetTotalSeconds() > 3)
	{
		ProcessJunk();

		LastProcessTime = Now;
	}
}

FMPlayer* FMPlayerManager::GetByPlayerID(const uint64 Id)
{
	if (const auto Ret = IndexEntities.Find(Id))
		return *Ret;
	
	return nullptr;
}

FMPlayer* FMPlayerManager::CreatePlayer(const uint64 InPlayerId, const FString& InAccount)
{
	if (GetByPlayerID(InPlayerId))
	{
		return nullptr;
	}

	FMPlayer* Player = nullptr;
	if (!Player->Init(InPlayerId, InAccount))
	{
		return nullptr;
	}
	
	if (!AddPlayer(Player))
		return nullptr;
	
	return Player;
}

void FMPlayerManager::DeletePlayer(FMPlayer* InPlayer)
{
	if (!InPlayer)
		return;

	/*if (InPlayer->IsRecycle())
		return;

	InPlayer->MarkRecycle();*/
	
	const uint64 ID = InPlayer->GetPlayerID();
	const auto Ret = IndexEntities.Find(ID);
	if (Ret && *Ret == InPlayer)
	{
		*Ret = nullptr;		

		for (int32 i = 0; i < AllEntities.Num(); ++i)
		{
			if (AllEntities[i] == InPlayer)
			{
				AllEntities[i] = nullptr;
				break;
			}
		}

		Junks.Emplace(InPlayer);
	}
}

void FMPlayerManager::DeletePlayerById(const uint64 InPlayerId)
{
	auto* Player = GetByPlayerID(InPlayerId);
	if (!Player)
		return;
	
	DeletePlayer(Player);
}

void FMPlayerManager::Foreach(const TFunction<bool(FMPlayer*)>& InFunc)
{
	for (const auto& Player : AllEntities)
	{
		if (Player)
		{
			if (!InFunc(Player))
				return;
		}
	}
}

void FMPlayerManager::SendToAll(const FPbMessagePtr& InMessage)
{
	Foreach([this, InMessage](const FMPlayer* Player) -> bool
	{
		Player->SendToMe(InMessage);
		return true;
	});
}

bool FMPlayerManager::AddPlayer(FMPlayer* InPlayer)
{
	const int64 ID = InPlayer->GetPlayerID();

	auto Ret = IndexEntities.Find(ID);
	if (*Ret)
	{
		return false;  // 该ID的角色已存在
	}
	else
	{
		IndexEntities.Add(ID, InPlayer);
	}
	
	{
		// 试着查询一个空位置
		int32 EmptyIdx = INDEX_NONE;
		for (int32 i = 0; i < AllEntities.Num(); ++i)
		{
			const FMPlayer* Ptr = AllEntities[i];
			if (EmptyIdx == INDEX_NONE && Ptr == nullptr)
			{
				EmptyIdx = i;
				break;
			}
		}
		
		if (EmptyIdx == INDEX_NONE)
		{
			EmptyIdx = AllEntities.AddUninitialized();
		}
		
		AllEntities[EmptyIdx] = InPlayer;
	}

	return true;
}

void FMPlayerManager::ProcessJunk()
{
	for (FMPlayer* Player : Junks)
	{
		IndexEntities.Remove(Player->GetPlayerID());
		// Todo 销毁内存
	}
	
	Junks.Empty();
}

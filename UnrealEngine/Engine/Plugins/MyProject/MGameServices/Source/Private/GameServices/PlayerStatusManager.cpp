#include "PlayerStatusManager.h"

#include "MTools.h"
#include "MPlayer.h"

FPlayerStatusManager::FPlayerStatusManager(FMPlayer* InPlayer): Player(InPlayer)
{
}

void FPlayerStatusManager::MarkNeedSave(const bool bImmediately)
{
	if (bImmediately)
	{
		NextSaveTime = 1;
	}
	else if (NextSaveTime <= 0)
	{
		NextSaveTime = FMath::RandRange(10, 20);
	}
	else
	{
		NextSaveTime = NextSaveTime + 1;	// 每多调用一次就提前一点
	}
}

void FPlayerStatusManager::UnmarkNeedSave()
{
	EnumRemoveFlags(StatusFlags, EPlayerStatusFlags::Save);	
}

void FPlayerStatusManager::MarkNeedReplicateQuestDataToSelf()
{
	EnumAddFlags(StatusFlags, EPlayerStatusFlags::ReplicateQuestDataToMe);
}

void FPlayerStatusManager::ReplicateQuestDataToSelf()
{
	EnumRemoveFlags(StatusFlags, EPlayerStatusFlags::ReplicateQuestDataToMe);
}

void FPlayerStatusManager::Tick(float DeltaTime)
{
	NextSaveTime -= DeltaTime;
	if (NextSaveTime <= 0)
	{
		DoSave();
		MarkNeedSave();
	}
	const auto Now = FMyTools::Now();
	OneSecondTime -= DeltaTime;
	if (OneSecondTime <= 0)
	{
		OnOneSecond(Now);
		OneSecondTime = 1;
	}
	HalfSecondTime -= DeltaTime;
	if (HalfSecondTime <= 0)
	{
		OnHalfSecond(Now);
		HalfSecondTime = 0.5;
	}
	
	if (EnumHasAnyFlags(StatusFlags, EPlayerStatusFlags::ReplicateQuestDataToMe))
	{
		ReplicateQuestDataToSelf();
	}
}

void FPlayerStatusManager::OnHalfSecond(FDateTime InNow)
{
}

void FPlayerStatusManager::OnOneSecond(FDateTime InNow)
{
}

void FPlayerStatusManager::DoSave()
{
	UnmarkNeedSave();
	NextSaveTime = 0; // 重置存档时间戳
	Player->Save();
}

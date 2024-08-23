#pragma once

class FMPlayer;

enum class EPlayerStatusFlags : uint64
{
	None = 0,
	Save = (1 << 0),
	ReplicateInventoryDataToSelf = (1 << 1),
	ReplicateCounterDataToMe = (1 << 2),
	ReplicateQuestDataToMe = (1 << 3),
	ReplicateMailDataToMe = (1 << 4),
	ReplicateAttributeDataToMe = (1 << 5)
};
ENUM_CLASS_FLAGS(EPlayerStatusFlags);

class FPlayerStatusManager
{
	
public:

	FPlayerStatusManager(FMPlayer* InPlayer);

	void MarkNeedSave(bool bImmediately = false);
	void UnmarkNeedSave();

	void MarkNeedReplicateQuestDataToSelf();
	void ReplicateQuestDataToSelf();

	void Tick(float DeltaTime);

	void OnHalfSecond(FDateTime InNow);// 半秒
	void OnOneSecond(FDateTime InNow);// 一秒

private:

	FMPlayer* Player;

	EPlayerStatusFlags StatusFlags = EPlayerStatusFlags::None;
	float NextSaveTime = 0;  // 下次存档时间戳
	float HalfSecondTime = 0.5;
	float OneSecondTime = 1;

	void DoSave();
};

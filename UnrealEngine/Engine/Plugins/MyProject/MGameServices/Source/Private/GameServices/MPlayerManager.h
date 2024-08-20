#pragma once

#include "CoreMinimal.h"
#include "MyNetFwd.h"

class FMPlayer;

class FMPlayerManager
{
	
public:
	
	virtual ~FMPlayerManager();

	void Init();
	void Shutdown();
	void Cleanup();

	void Tick(float DeltaTime);

	FMPlayer* GetByPlayerID(const uint64 Id);

	FMPlayer* CreatePlayer(const uint64 InPlayerId, const FString& InAccount);
	void DeletePlayer(FMPlayer* InPlayer);
	void DeletePlayerById(const uint64 InPlayerId);

	void Foreach(const TFunction<bool(FMPlayer*)>& InFunc);

	void SendToAll(const FPbMessagePtr& InMessage);

private:

	bool AddPlayer(FMPlayer* InPlayer);
	
	void ProcessJunk();

	TArray<FMPlayer*> AllEntities;

	TMap<uint64, FMPlayer*> IndexEntities;

	TArray<FMPlayer*> Junks;  // 负责持有待删除的指针

	FDateTime LastProcessTime;
};

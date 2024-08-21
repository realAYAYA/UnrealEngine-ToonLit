#pragma once

#include "CoreMinimal.h"
#include "MyNetFwd.h"

class FMPlayer;

class FMPlayerManager
{
	
public:
	
	virtual ~FMPlayerManager();

	static FMPlayerManager* Get()
	{
		static FMPlayerManager Single;
		return &Single;
	}

	void Init();
	void Shutdown();
	void Cleanup();

	void Tick(float DeltaTime);

	FMPlayer* GetByPlayerId(const uint64 Id);

	FMPlayer* CreatePlayer(const uint64 InPlayerId, const FString& InAccount);
	void DeletePlayer(FMPlayer* InPlayer);
	void DeletePlayerById(const uint64 InPlayerId);

	void Foreach(const TFunction<bool(FMPlayer*)>& InFunc);

	void SendToAll(const FPbMessagePtr& InMessage);

private:

	bool AddPlayer(TSharedPtr<FMPlayer> InPlayer);
	
	void ProcessJunk();
	
	TMap<uint64, TSharedPtr<FMPlayer>> IndexEntities;

	FDateTime LastProcessTime;
};

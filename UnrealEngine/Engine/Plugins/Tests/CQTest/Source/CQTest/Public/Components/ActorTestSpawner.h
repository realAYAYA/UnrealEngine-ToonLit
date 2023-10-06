// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SpawnHelper.h"

class UTestGameInstance;

/// Class for spawning actors in an ActorTest context (no PIE loaded)
struct CQTEST_API FActorTestSpawner : public FSpawnHelper
{
	FActorTestSpawner() = default;
	virtual ~FActorTestSpawner() override;

	void InitializeGameSubsystems();

	UTestGameInstance* GetGameInstance();

protected:
	virtual UWorld* CreateWorld() override;

private:
	UTestGameInstance* GameInstance{ nullptr };
};
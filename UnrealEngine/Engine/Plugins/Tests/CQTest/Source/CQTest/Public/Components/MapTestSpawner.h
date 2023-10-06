// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SpawnHelper.h"

/// Class for spawning Actors in a named Map/Level
struct CQTEST_API FMapTestSpawner : public FSpawnHelper
{
	FMapTestSpawner(const FString& MapDirectory, const FString& MapName)
		: MapDirectory(MapDirectory)
		, MapName(MapName)
	{
	}

	void AddWaitUntilLoadedCommand(FAutomationTestBase* TestRunner);
	APawn* FindFirstPlayerPawn();

protected:
    virtual UWorld* CreateWorld() override;

private:
	FString MapDirectory;
	FString MapName;
	UWorld* PieWorld{ nullptr };
};
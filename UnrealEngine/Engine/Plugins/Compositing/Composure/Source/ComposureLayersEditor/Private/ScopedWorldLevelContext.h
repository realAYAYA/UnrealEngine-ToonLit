// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/World.h"
#include "GameFramework/Actor.h"

class ULevel;

struct FScopedWorldLevelContext
{
	FScopedWorldLevelContext(UWorld* InWorld, AActor* LevelContext)
		: World(InWorld)
	{
		if (InWorld)
		{
			LevelToRestore = InWorld->GetCurrentLevel();

			if (LevelContext)
			{
				InWorld->SetCurrentLevel(LevelContext->GetLevel());
			}
		}
	}

	FScopedWorldLevelContext(UWorld* InWorld, ULevel* Level)
		: World(InWorld)
	{
		if (InWorld)
		{
			LevelToRestore = InWorld->GetCurrentLevel();
			InWorld->SetCurrentLevel(Level);
		}
	}

	~FScopedWorldLevelContext()
	{
		if (World)
		{
			World->SetCurrentLevel(LevelToRestore);
		}
	}

private:
	UWorld* World = nullptr;
	ULevel* LevelToRestore = nullptr;
};
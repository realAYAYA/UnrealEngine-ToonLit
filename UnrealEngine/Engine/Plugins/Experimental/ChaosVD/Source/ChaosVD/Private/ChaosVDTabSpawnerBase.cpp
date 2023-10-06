// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDTabSpawnerBase.h"

#include "ChaosVDEngine.h"
#include "ChaosVDScene.h"
#include "Widgets/SChaosVDMainTab.h"

FChaosVDTabSpawnerBase::FChaosVDTabSpawnerBase(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, SChaosVDMainTab* InOwningTabWidget)
{
	OwningTabWidget = InOwningTabWidget;
	InTabManager->RegisterTabSpawner(InTabID, FOnSpawnTab::CreateRaw(this, &FChaosVDTabSpawnerBase::HandleTabSpawned));
}

UWorld* FChaosVDTabSpawnerBase::GetChaosVDWorld() const
{
	if (TSharedPtr<FChaosVDScene> ScenePtr = GetChaosVDScene().Pin())
	{
		return ScenePtr->GetUnderlyingWorld();
	}

	return nullptr;
}

TWeakPtr<FChaosVDScene> FChaosVDTabSpawnerBase::GetChaosVDScene() const
{
	if (ensure(OwningTabWidget))
	{
		const TSharedRef<FChaosVDEngine> ChaosVDEngine = OwningTabWidget->GetChaosVDEngineInstance();
		if (TSharedPtr<FChaosVDScene> ScenePtr = ChaosVDEngine->GetCurrentScene())
		{
			return ScenePtr;
		}
	}
	return nullptr;
}

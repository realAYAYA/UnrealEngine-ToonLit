// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDTabSpawnerBase.h"

#include "ChaosVDEngine.h"
#include "ChaosVDScene.h"
#include "Widgets/SChaosVDMainTab.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FChaosVDTabSpawnerBase::FChaosVDTabSpawnerBase(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, TWeakPtr<SChaosVDMainTab> InOwningTabWidget)
{
	OwningTabWidget = InOwningTabWidget;
	InTabManager->RegisterTabSpawner(InTabID, FOnSpawnTab::CreateRaw(this, &FChaosVDTabSpawnerBase::HandleTabSpawnRequest));
}

void FChaosVDTabSpawnerBase::HandleTabClosed(TSharedRef<SDockTab> InTabClosed)
{
	OnTabDestroyed().Broadcast(InTabClosed);
	InTabClosed->SetOnTabClosed(SDockTab::FOnTabClosedCallback());
}

void FChaosVDTabSpawnerBase::HandleTabSpawned(TSharedRef<SDockTab> InTabClosed)
{
	OnTabSpawned().Broadcast(InTabClosed);
	InTabClosed->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FChaosVDTabSpawnerBase::HandleTabClosed));
}

TSharedRef<SWidget> FChaosVDTabSpawnerBase::GenerateErrorWidget()
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChaosVDEditorTabSpawner", "Failed to generate Tab content"))
		];
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
	const TSharedPtr<SChaosVDMainTab> MainTabPtr = OwningTabWidget.Pin();
	if (ensure(MainTabPtr))
	{
		const TSharedRef<FChaosVDEngine> ChaosVDEngine = MainTabPtr->GetChaosVDEngineInstance();
		if (TSharedPtr<FChaosVDScene> ScenePtr = ChaosVDEngine->GetCurrentScene())
		{
			return ScenePtr;
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE

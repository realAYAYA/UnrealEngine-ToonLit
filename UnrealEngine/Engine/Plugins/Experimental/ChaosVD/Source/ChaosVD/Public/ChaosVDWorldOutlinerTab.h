// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDSceneSelectionObserver.h"
#include "ChaosVDTabSpawnerBase.h"
#include "Delegates/DelegateCombinations.h"
#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"

struct FChaosVDOutlinerItem;

class AActor;
class ISceneOutliner;
class FChaosVDScene;
class FSpawnTabArgs;
class FTabManager;
class SChaosVDSceneOutliner;
class SDockTab;

enum class EChaosVDParticleType : uint8;

DECLARE_DELEGATE_OneParam(FChaosVDSelectionChanged, AActor* SelectedActor)

/** Spawns and handles and instance for the visual debugger World Outliner */
class FChaosVDWorldOutlinerTab : public FChaosVDTabSpawnerBase, public TSharedFromThis<FChaosVDWorldOutlinerTab>
{
public:
	FChaosVDWorldOutlinerTab(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, SChaosVDMainTab* InOwningTabWidget) : FChaosVDTabSpawnerBase(InTabID, InTabManager, InOwningTabWidget)
	{
	}

	virtual TSharedRef<SDockTab> HandleTabSpawned(const FSpawnTabArgs& Args) override;

private:
	void CreateWorldOutlinerWidget();

	TSharedPtr<ISceneOutliner> SceneOutlinerWidget;
};

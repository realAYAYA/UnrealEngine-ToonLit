// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "Widgets/Docking/SDockTab.h"

class FChaosVDScene;
class SChaosVDMainTab;
class FName;
class FTabManager;
class SDockTab;
class FSpawnTabArgs;

DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDTabSpawned, TSharedRef<SDockTab>)
DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDTabDestroyed, TSharedRef<SDockTab>)

/** Base class for any tab of the Chaos Visual Debugger tool*/
class FChaosVDTabSpawnerBase
{
public:
	virtual ~FChaosVDTabSpawnerBase() = default;

	FChaosVDTabSpawnerBase(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, TWeakPtr<SChaosVDMainTab> InOwningTabWidget);

	virtual TSharedRef<SDockTab> HandleTabSpawnRequest(const FSpawnTabArgs& Args) = 0;
	virtual void HandleTabClosed(TSharedRef<SDockTab> InTabClosed);
	virtual void HandleTabSpawned(TSharedRef<SDockTab> InTabClosed);

	FChaosVDTabSpawned& OnTabSpawned() { return TabSpawnedDelegate; }
	FChaosVDTabDestroyed& OnTabDestroyed() { return TabDestroyedDelegate; }

	TSharedRef<SWidget> GenerateErrorWidget();

protected:

	/** Ptr to the main tab of the owning visual debugger tool instance */
	TWeakPtr<SChaosVDMainTab> OwningTabWidget;

	FChaosVDTabSpawned TabSpawnedDelegate;
	FChaosVDTabDestroyed TabDestroyedDelegate;

	UWorld* GetChaosVDWorld() const;

	TWeakPtr<FChaosVDScene> GetChaosVDScene() const;
};

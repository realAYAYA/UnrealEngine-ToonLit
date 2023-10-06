// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDSceneSelectionObserver.h"
#include "Templates/SharedPointer.h"
#include "ChaosVDTabSpawnerBase.h"
#include "Delegates/IDelegateInstance.h"

struct FChaosVDParticleDebugData;

class AActor;
class IDetailsView;
class FName;
class FSpawnTabArgs;
class FTabManager;
class SDockTab;


/** Spawns and handles and instance for the visual debugger details panel */
class FChaosVDObjectDetailsTab : public FChaosVDTabSpawnerBase, public FChaosVDSceneSelectionObserver, public TSharedFromThis<FChaosVDObjectDetailsTab>
{
public:

	FChaosVDObjectDetailsTab(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, SChaosVDMainTab* InOwningTabWidget) : FChaosVDTabSpawnerBase(InTabID, InTabManager, InOwningTabWidget)
	{
	}

protected:

	virtual TSharedRef<SDockTab> HandleTabSpawned(const FSpawnTabArgs& Args) override;

	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangedSelectionSet) override;

	FDelegateHandle SelectionDelegateHandle;
	TSharedPtr<IDetailsView> DetailsPanel;
};

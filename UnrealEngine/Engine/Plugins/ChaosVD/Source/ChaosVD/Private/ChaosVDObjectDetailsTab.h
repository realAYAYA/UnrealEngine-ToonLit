// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDSceneSelectionObserver.h"
#include "Templates/SharedPointer.h"
#include "ChaosVDTabSpawnerBase.h"
#include "Delegates/IDelegateInstance.h"

struct FChaosVDParticleDebugData;

class AActor;
class FName;
class FSpawnTabArgs;
class FTabManager;
class SChaosVDDetailsView;
class SDockTab;

/** Spawns and handles and instance for the visual debugger details panel */
class FChaosVDObjectDetailsTab : public FChaosVDTabSpawnerBase, public FChaosVDSceneSelectionObserver, public TSharedFromThis<FChaosVDObjectDetailsTab>
{
public:

	FChaosVDObjectDetailsTab(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, TWeakPtr<SChaosVDMainTab> InOwningTabWidget) : FChaosVDTabSpawnerBase(InTabID, InTabManager, InOwningTabWidget)
	{
	}

protected:

	virtual TSharedRef<SDockTab> HandleTabSpawnRequest(const FSpawnTabArgs& Args) override;
	virtual void HandleTabClosed(TSharedRef<SDockTab> InTabClosed) override;

	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangedSelectionSet) override;

	EVisibility GetCollisionDataButtonVisibility() const;
	bool GetCollisionDataButtonEnabled() const;

	TSharedPtr<SWidget> GenerateShowCollisionDataButton();

	FReply ShowCollisionDataForSelectedObject();

	FDelegateHandle SelectionDelegateHandle;
	TSharedPtr<SChaosVDDetailsView> DetailsPanelView;
	TWeakObjectPtr<UObject> CurrentSelectedObject = nullptr;
};

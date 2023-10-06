// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FChaosVDScene;
class SChaosVDMainTab;
class FName;
class FTabManager;
class SDockTab;
class FSpawnTabArgs;

/** Base class for any tab of the Chaos Visual Debugger tool*/
class FChaosVDTabSpawnerBase
{
public:
	virtual ~FChaosVDTabSpawnerBase() = default;

	FChaosVDTabSpawnerBase(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, SChaosVDMainTab* InOwningTabWidget);

	virtual TSharedRef<SDockTab> HandleTabSpawned(const FSpawnTabArgs& Args) = 0;

protected:

	/** Ptr to the main tab of the owning visual debugger tool instance */
	SChaosVDMainTab* OwningTabWidget;

	UWorld* GetChaosVDWorld() const;

	TWeakPtr<FChaosVDScene> GetChaosVDScene() const;
};

// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "ChaosVDTabSpawnerBase.h"

class FChaosVDEditorSettingsTab : public FChaosVDTabSpawnerBase
{
public:
	FChaosVDEditorSettingsTab(const FName& InTabID, const TSharedPtr<FTabManager>& InTabManager, SChaosVDMainTab* InOwningTabWidget)
		: FChaosVDTabSpawnerBase(InTabID, InTabManager, InOwningTabWidget)
	{
	}


	virtual TSharedRef<SDockTab> HandleTabSpawned(const FSpawnTabArgs& Args) override;
};

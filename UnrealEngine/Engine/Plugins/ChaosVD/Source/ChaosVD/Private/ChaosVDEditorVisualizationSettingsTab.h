// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosVDTabSpawnerBase.h"
#include "Widgets/SChaosVDMainTab.h"

class SChaosVDNameListPicker;

class FChaosVDEditorVisualizationSettingsTab : public FChaosVDTabSpawnerBase
{
public:
	FChaosVDEditorVisualizationSettingsTab(const FName& InTabID, const TSharedPtr<FTabManager>& InTabManager, TWeakPtr<SChaosVDMainTab> InOwningTabWidget)
		: FChaosVDTabSpawnerBase(InTabID, InTabManager, InOwningTabWidget)
	{
	}

	virtual TSharedRef<SDockTab> HandleTabSpawnRequest(const FSpawnTabArgs& Args) override;
};

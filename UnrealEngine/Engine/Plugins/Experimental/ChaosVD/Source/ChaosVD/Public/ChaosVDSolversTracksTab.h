// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDTabSpawnerBase.h"

class SChaosVDSolverTracks;

/** Spawns and handles and instance for the visual debugger Solvers Tracks tab
 * Which provides playback controls for each recorded solver
 */
class FChaosVDSolversTracksTab : public FChaosVDTabSpawnerBase, public TSharedFromThis<FChaosVDSolversTracksTab>
{
public:
	FChaosVDSolversTracksTab(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, SChaosVDMainTab* InOwningTabWidget) : FChaosVDTabSpawnerBase(InTabID, InTabManager, InOwningTabWidget)
	{	
	}
	
	virtual TSharedRef<SDockTab> HandleTabSpawned(const FSpawnTabArgs& Args) override;

protected:
	TSharedPtr<SChaosVDSolverTracks> SolverTracksWidget;
};

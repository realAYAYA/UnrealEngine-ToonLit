// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "ChaosVDTabSpawnerBase.h"

class FChaosVDScene;
class SChaosVDPlaybackViewport;

/** Spawns and handles and instance for the visual debugger Playback viewport tab
 * It contains the Playback controls and the 3D viewport
 */
class FChaosVDPlaybackViewportTab : public FChaosVDTabSpawnerBase, public TSharedFromThis<FChaosVDPlaybackViewportTab>
{
public:
	FChaosVDPlaybackViewportTab(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, SChaosVDMainTab* InOwningTabWidget) : FChaosVDTabSpawnerBase(InTabID, InTabManager, InOwningTabWidget)
	{	
	}
	
	virtual TSharedRef<SDockTab> HandleTabSpawned(const FSpawnTabArgs& Args) override;

protected:
	TSharedPtr<SChaosVDPlaybackViewport> PlaybackViewportWidget;
};

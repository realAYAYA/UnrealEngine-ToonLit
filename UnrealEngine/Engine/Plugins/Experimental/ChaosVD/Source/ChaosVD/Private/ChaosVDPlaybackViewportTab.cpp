// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDPlaybackViewportTab.h"

#include "ChaosVDEngine.h"
#include "ChaosVDStyle.h"
#include "Widgets/SChaosVDPlaybackViewport.h"
#include "ChaosVDTabsIDs.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SChaosVDMainTab.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

TSharedRef<SDockTab> FChaosVDPlaybackViewportTab::HandleTabSpawned(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> ViewportTab =
		SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		.Label(LOCTEXT("ViewportTabLabel", "Playback Viewport"))
		.ToolTipText(LOCTEXT("ViewportTabToolTip", "The Chaos Visual debugger Viewport is under development"));
	
	ViewportTab->SetContent
	(
		//TODO: Handle Null cases to not crash the Editor
		SAssignNew(PlaybackViewportWidget, SChaosVDPlaybackViewport, GetChaosVDScene(), OwningTabWidget->GetChaosVDEngineInstance()->GetPlaybackController())
	);

	ViewportTab->SetTabIcon(FChaosVDStyle::Get().GetBrush("TabIconPlaybackViewport"));

	return ViewportTab;
}

#undef LOCTEXT_NAMESPACE
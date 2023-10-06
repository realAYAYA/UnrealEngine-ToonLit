// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDSolversTracksTab.h"

#include "ChaosVDEngine.h"
#include "ChaosVDStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SChaosVDMainTab.h"
#include "Widgets/SChaosVDSolverTracks.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

TSharedRef<SDockTab> FChaosVDSolversTracksTab::HandleTabSpawned(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> ViewportTab =
	SNew(SDockTab)
	.TabRole(ETabRole::PanelTab)
	.Label(LOCTEXT("SolverTracksTabLabel", "Available Solvers"))
	.ToolTipText(LOCTEXT("SolverTracksTabToolTip", "Playback controls for the available solvers on the current Frame"));

	ViewportTab->SetContent
	(
		// TODO: Handle Null cases to not crash the Editor
		SAssignNew(SolverTracksWidget, SChaosVDSolverTracks, OwningTabWidget->GetChaosVDEngineInstance()->GetPlaybackController())
	);

	ViewportTab->SetTabIcon(FChaosVDStyle::Get().GetBrush("TabIconPlaybackViewport"));

	return ViewportTab;
}

#undef LOCTEXT_NAMESPACE

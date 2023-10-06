// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDOutputLogTab.h"

#include "OutputLogCreationParams.h"
#include "OutputLogModule.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

TSharedRef<SDockTab> FChaosVDOutputLogTab::HandleTabSpawned(const FSpawnTabArgs& Args)
{
	FOutputLogCreationParams Params;
	Params.bCreateDockInLayoutButton = true;
	Params.SettingsMenuCreationFlags = EOutputLogSettingsMenuFlags::SkipClearOnPie
		| EOutputLogSettingsMenuFlags::SkipOpenSourceButton
		| EOutputLogSettingsMenuFlags::SkipEnableWordWrapping; // Checkbox relies on saving an editor config file and does not work correctly
	
	TSharedRef<SDockTab> OutputLogTab =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(LOCTEXT("OutputLogTabLabel", "Output Log"));
	
	OutputLogTab->SetContent
	(
		FOutputLogModule::Get().MakeOutputLogWidget(Params)
	);

	return OutputLogTab;
}

#undef LOCTEXT_NAMESPACE

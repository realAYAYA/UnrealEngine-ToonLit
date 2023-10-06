// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDEditorSettingsTab.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDStyle.h"
#include "DetailsViewArgs.h"
#include "Framework/Docking/TabManager.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

TSharedRef<SDockTab> FChaosVDEditorSettingsTab::HandleTabSpawned(const FSpawnTabArgs& Args)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedRef<IDetailsView> DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsPanel->SetObject(GetMutableDefault<UChaosVDEditorSettings>());

	TSharedRef<SDockTab> DetailsPanelTab =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(LOCTEXT("ChaosVDEditorSettings", "Chaos VD Settings"))
		.ToolTipText(LOCTEXT("ChaosVDEditorSettingsTip", "See the available settings for the editor"));

	DetailsPanelTab->SetContent
	(
		DetailsPanel
	);

	DetailsPanelTab->SetTabIcon(FChaosVDStyle::Get().GetBrush("TabIconDetailsPanel"));

	return DetailsPanelTab;
}

#undef LOCTEXT_NAMESPACE 

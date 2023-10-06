// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDObjectDetailsTab.h"

#include "ChaosVDScene.h"
#include "ChaosVDStyle.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Templates/SharedPointer.h"
#include "Editor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Elements/Framework/TypedElementSelectionSet.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

TSharedRef<SDockTab> FChaosVDObjectDetailsTab::HandleTabSpawned(const FSpawnTabArgs& Args)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedPtr<FChaosVDScene> ScenePtr = GetChaosVDScene().Pin();
	check(ScenePtr);

	DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	RegisterSelectionSetObject(ScenePtr->GetElementSelectionSet());

	TSharedRef<SDockTab> DetailsPanelTab =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(LOCTEXT("DetailsPanel", "Details"))
		.ToolTipText(LOCTEXT("DetailsPanelToolTip", "See the details of the selected object"));

	DetailsPanelTab->SetContent
	(
		DetailsPanel.ToSharedRef()
	);

	DetailsPanelTab->SetTabIcon(FChaosVDStyle::Get().GetBrush("TabIconDetailsPanel"));

	return DetailsPanelTab;
}

void FChaosVDObjectDetailsTab::HandlePostSelectionChange(const UTypedElementSelectionSet* ChangedSelectionSet)
{
	TArray<AActor*> SelectedActors = ChangedSelectionSet->GetSelectedObjects<AActor>();

	if (SelectedActors.Num() > 0)
	{
		// We don't support multi selection yet
		ensure(SelectedActors.Num() == 1);

		DetailsPanel->SetObject(SelectedActors[0], true);
	}
}

#undef LOCTEXT_NAMESPACE

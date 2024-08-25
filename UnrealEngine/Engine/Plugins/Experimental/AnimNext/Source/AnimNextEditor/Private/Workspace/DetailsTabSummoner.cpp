// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsTabSummoner.h"
#include "AnimNextWorkspaceEditor.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "DetailsTabSummoner"

namespace UE::AnimNext::Editor
{

FDetailsTabSummoner::FDetailsTabSummoner(TSharedPtr<FWorkspaceEditor> InHostingApp, FOnDetailsViewCreated InOnDetailsViewCreated)
	: FWorkflowTabFactory(WorkspaceTabs::Details, InHostingApp)
{
	TabLabel = LOCTEXT("DetailsTabLabel", "Details");
	TabIcon = FSlateIcon("EditorStyle", "LevelEditor.Tabs.Details");
	ViewMenuDescription = LOCTEXT("DetailsTabMenuDescription", "Details");
	ViewMenuTooltip = LOCTEXT("DetailsTabToolTip", "Shows the details tab for selected objects.");
	bIsSingleton = true;

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	InOnDetailsViewCreated.ExecuteIfBound(DetailsView.ToSharedRef());
}

TSharedRef<SWidget> FDetailsTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return DetailsView.ToSharedRef();
}

FText FDetailsTabSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return ViewMenuTooltip;
}

}

#undef LOCTEXT_NAMESPACE
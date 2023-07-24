// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChangelistReviewModule.h"
#include "ISourceControlModule.h"
#include "ToolMenu.h"
#include "Widgets/Docking/SDockTab.h"
#include "SSourceControlReview.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "ChangelistReviewModule"

static const FName SourceControlReviewTabName = FName(TEXT("SourceControlChangelistReview"));

void FChangelistReviewModule::StartupModule()
{
	UToolMenu* SourceControlMenu = UToolMenus::Get()->ExtendMenu("StatusBar.ToolBar.SourceControl");
	FToolMenuSection& Section = SourceControlMenu->AddSection("SourceControlActions", LOCTEXT("SourceControlMenuHeadingActions", "Actions"));
	
	const FSlateIcon SourceControlIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.ChangelistsTab");
	Section.AddMenuEntry(
		FName(TEXT("Review Changelists")),
		LOCTEXT("ReviewChangelist_Label", "Review Changelists"),
		LOCTEXT("ReviewChangelist_Tooltip", "Opens a dialog to review shelved and submitted changelists"),
		SourceControlIcon,
		FUIAction(
			FExecuteAction::CreateRaw(this, &FChangelistReviewModule::ShowReviewTab),
			FCanExecuteAction(),
			FGetActionCheckState(),
			FIsActionButtonVisible::CreateRaw(this, &FChangelistReviewModule::CanShowReviewTab)
		)
	);
	
	// Register the changelist tab spawner
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SourceControlReviewTabName, FOnSpawnTab::CreateRaw(this, &FChangelistReviewModule::CreateReviewTab))
		.SetDisplayName(LOCTEXT("ChangelistsTabTitle", "Review Changelist"))
		.SetTooltipText(LOCTEXT("ChangelistsTabTooltip", "Opens a dialog to review shelved and submitted changelists."))
		.SetIcon(SourceControlIcon)
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FChangelistReviewModule::ShutdownModule()
{
}

TSharedRef<SDockTab> FChangelistReviewModule::CreateReviewTab(const FSpawnTabArgs& Args)
{
	return SAssignNew(ReviewTab, SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			CreateReviewUI().ToSharedRef()
		];
}

TSharedPtr<SWidget> FChangelistReviewModule::CreateReviewUI()
{
	TSharedPtr<SWidget> ReturnWidget;
	if (IsInGameThread())
	{
		TSharedPtr<SSourceControlReview> SharedPtr = SNew(SSourceControlReview);
		ReturnWidget = SharedPtr;
	}
	 return ReturnWidget;
}

void FChangelistReviewModule::ShowReviewTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(SourceControlReviewTabName));
}

bool FChangelistReviewModule::CanShowReviewTab() const
{
	const ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (SourceControlModule.IsEnabled())
	{
		const ISourceControlProvider& Provider = SourceControlModule.GetProvider();
	
		// we currently only support perforce for the review tool
		return Provider.IsAvailable() && Provider.GetName().ToString() == TEXT("Perforce");
	}
	return false;
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FChangelistReviewModule, ChangelistReview)

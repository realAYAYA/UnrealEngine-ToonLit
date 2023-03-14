// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerSCCHandler.h"
#include "ISourceControlModule.h"
#include "SourceControlOperations.h"
#include "Misc/MessageDialog.h"
#include "FileHelpers.h"
#include "SourceControlWindows.h"
#include "ISourceControlWindowsModule.h"
#include "UncontrolledChangelistsModule.h"
#include "AssetViewUtils.h"

#define LOCTEXT_NAMESPACE "FSceneOutlinerSCCHandler"

TSharedPtr<FSceneOutlinerTreeItemSCC> FSceneOutlinerSCCHandler::GetItemSourceControl(const FSceneOutlinerTreeItemPtr& InItem) const
{
	TSharedPtr<FSceneOutlinerTreeItemSCC>* Result = ItemSourceControls.Find(InItem);
	if (!Result)
	{
		Result = &ItemSourceControls.Add(InItem, MakeShared<FSceneOutlinerTreeItemSCC>(InItem));
	}
	
	check(Result);
	return *Result;
}

bool FSceneOutlinerSCCHandler::AddSourceControlMenuOptions(UToolMenu* Menu, TArray<FSceneOutlinerTreeItemPtr> InSelectedItems)
{
	SelectedItems = InSelectedItems;

	CacheCanExecuteVars();

	if (ISourceControlModule::Get().IsEnabled() || FUncontrolledChangelistsModule::Get().IsEnabled())
	{
		FToolMenuSection& Section = Menu->AddSection("AssetContextSourceControl");

		if (bCanExecuteSCC)
		{
			// SCC sub menu
			Section.AddSubMenu(
				"SourceControlSubMenu",
				LOCTEXT("SourceControlSubMenuLabel", "Source Control"),
				LOCTEXT("SourceControlSubMenuToolTip", "Source control actions."),
				FNewToolMenuDelegate::CreateSP(this, &FSceneOutlinerSCCHandler::FillSourceControlSubMenu),
				FUIAction(
					FExecuteAction(),
					FCanExecuteAction::CreateSP( this, &FSceneOutlinerSCCHandler::CanExecuteSourceControlActions )
					),
				EUserInterfaceActionType::Button,
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.StatusIcon.On")
				);
		}
		else
		{
			// SCC sub menu
			Section.AddMenuEntry(
				"SourceControlSubMenu",
				LOCTEXT("SourceControlSubMenuLabel", "Source Control"),
				LOCTEXT("SourceControlSubMenuDisabledToolTip", "Disabled because one or more selected items are not external packages."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.StatusIcon.Off"),
				FUIAction(
					FExecuteAction(),
					FCanExecuteAction::CreateSP( this, &FSceneOutlinerSCCHandler::CanExecuteSCC )
				)
				);
		}

		return true;
	}

	return false;
}


bool FSceneOutlinerSCCHandler::CanExecuteSourceControlActions() const
{
	return SelectedItems.Num() > 0;
}

void FSceneOutlinerSCCHandler::CacheCanExecuteVars()
{
	bCanExecuteSCC = true;
	bCanExecuteSCCCheckOut = false;
	bCanExecuteSCCCheckIn = false;
	bCanExecuteSCCHistory = false;

	if ( ISourceControlModule::Get().IsEnabled() || FUncontrolledChangelistsModule::Get().IsEnabled())
	{
		for (FSceneOutlinerTreeItemPtr SelectedItem : SelectedItems)
		{
			TSharedPtr<FSceneOutlinerTreeItemSCC> SourceControl = GetItemSourceControl(SelectedItem);
			if (SourceControl == nullptr)
			{
				continue;
			}

			if (!SourceControl->IsExternalPackage())
			{
				// this isn't an external package so we can't do anything with source control
				bCanExecuteSCC = false;

				// skip computing the rest of this package, but don't break
				// since getting a mixed external/internal selection is possible
				// and we change the UI state to disabled to show the user why
				// they aren't getting SCC actions
				continue;
			}

			// Check the SCC state for each package in the selected paths
			FSourceControlStatePtr SourceControlState = SourceControl->GetSourceControlState();
			if(SourceControlState.IsValid())
			{
				if ( SourceControlState->CanCheckout() )
				{
					bCanExecuteSCCCheckOut = true;
				}

				if( SourceControlState->IsSourceControlled() && !SourceControlState->IsAdded() )
				{
					bCanExecuteSCCHistory = true;
				}

				if ( SourceControlState->CanCheckIn() )
				{
					bCanExecuteSCCCheckIn = true;
				}
			}
		}
	}
}

bool FSceneOutlinerSCCHandler::CanExecuteSCC() const
{
	return bCanExecuteSCC;
}

bool FSceneOutlinerSCCHandler::CanExecuteSCCCheckOut() const
{
	return bCanExecuteSCCCheckOut;
}

bool FSceneOutlinerSCCHandler::CanExecuteSCCCheckIn() const
{
	bool bUsesFileRevisions = true;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if (ISourceControlModule::Get().IsEnabled())
	{
		bUsesFileRevisions = SourceControlProvider.UsesFileRevisions();
	}

	return bCanExecuteSCCCheckIn && bUsesFileRevisions;
}

bool FSceneOutlinerSCCHandler::CanExecuteSCCHistory() const
{
	return bCanExecuteSCCHistory;
}

bool FSceneOutlinerSCCHandler::CanExecuteSCCRefresh() const
{
	return ISourceControlModule::Get().IsEnabled();
}

void FSceneOutlinerSCCHandler::FillSourceControlSubMenu(UToolMenu* Menu)
{
	FToolMenuSection& Section = Menu->AddSection("AssetSourceControlActions", LOCTEXT("AssetSourceControlActionsMenuHeading", "Source Control"));

	if ( CanExecuteSCCCheckOut() )
	{
		Section.AddMenuEntry(
			"SCCCheckOut",
			LOCTEXT("SCCCheckOut", "Check Out"),
			LOCTEXT("SCCCheckOutTooltip", "Checks out the selected asset from source control."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.CheckOut"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FSceneOutlinerSCCHandler::ExecuteSCCCheckOut ),
				FCanExecuteAction::CreateSP( this, &FSceneOutlinerSCCHandler::CanExecuteSCCCheckOut )
			)
		);
	}

	if ( CanExecuteSCCCheckIn() )
	{
		Section.AddMenuEntry(
			"SCCCheckIn",
			LOCTEXT("SCCCheckIn", "Check In"),
			LOCTEXT("SCCCheckInTooltip", "Checks in the selected asset to source control."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Submit"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FSceneOutlinerSCCHandler::ExecuteSCCCheckIn ),
				FCanExecuteAction::CreateSP( this, &FSceneOutlinerSCCHandler::CanExecuteSCCCheckIn )
			)
		);
	}

	Section.AddMenuEntry(
		"SCCRefresh",
		LOCTEXT("SCCRefresh", "Refresh"),
		LOCTEXT("SCCRefreshTooltip", "Updates the source control status of the asset."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Refresh"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FSceneOutlinerSCCHandler::ExecuteSCCRefresh ),
			FCanExecuteAction::CreateSP( this, &FSceneOutlinerSCCHandler::CanExecuteSCCRefresh )
			)
		);

	if( CanExecuteSCCHistory() )
	{
		Section.AddMenuEntry(
			"SCCHistory",
			LOCTEXT("SCCHistory", "History"),
			LOCTEXT("SCCHistoryTooltip", "Displays the source control revision history of the selected asset."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.History"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FSceneOutlinerSCCHandler::ExecuteSCCHistory ),
				FCanExecuteAction::CreateSP( this, &FSceneOutlinerSCCHandler::CanExecuteSCCHistory )
			)
		);
	}

	Section.AddSeparator(NAME_None);
	Section.AddMenuEntry(
		"SCCFindInChangelist",
		LOCTEXT("SCCShowInChangelist", "Show in Changelist"),
		LOCTEXT("SCCShowInChangelistTooltip", "Show the selected assets in the Changelist window."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.ChangelistsTab"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FSceneOutlinerSCCHandler::ExecuteSCCShowInChangelist)
		)
	);
}

void FSceneOutlinerSCCHandler::GetSelectedPackageNames(TArray<FString>& OutPackageNames) const
{
	for (FSceneOutlinerTreeItemPtr SelectedItem : SelectedItems)
	{
		const TSharedPtr<FSceneOutlinerTreeItemSCC> SourceControl = GetItemSourceControl(SelectedItem);
		if (SourceControl == nullptr)
		{
			continue;
		}

		FString PackageName = SourceControl->GetPackageName();
		if (!PackageName.IsEmpty()) {
			OutPackageNames.Add(PackageName);
		}
	}
}

void FSceneOutlinerSCCHandler::GetSelectedPackages(TArray<UPackage*>& OutPackages) const
{
	for (FSceneOutlinerTreeItemPtr SelectedItem : SelectedItems)
	{
		const TSharedPtr<FSceneOutlinerTreeItemSCC> SourceControl = GetItemSourceControl(SelectedItem);
		if (SourceControl == nullptr)
		{
			continue;
		}

		UPackage* Package = SourceControl->GetPackage();
		if (Package != nullptr) {
			OutPackages.Add(Package);
		}
	}
}

void FSceneOutlinerSCCHandler::ExecuteSCCShowInChangelist()
{
	TArray<FString> PackageNames;
	GetSelectedPackageNames(PackageNames);

	ISourceControlWindowsModule::Get().SelectFiles(SourceControlHelpers::PackageFilenames(PackageNames));
}

void FSceneOutlinerSCCHandler::ExecuteSCCRefresh()
{
	TArray<FString> PackageNames;
	GetSelectedPackageNames(PackageNames);

	ISourceControlModule::Get().GetProvider().Execute(ISourceControlOperation::Create<FUpdateStatus>(), SourceControlHelpers::PackageFilenames(PackageNames), EConcurrency::Asynchronous);
}

void FSceneOutlinerSCCHandler::ExecuteSCCCheckOut()
{
	TArray<UPackage*> PackagesToCheckOut;
	GetSelectedPackages(PackagesToCheckOut);

	if ( PackagesToCheckOut.Num() > 0 )
	{
		FEditorFileUtils::CheckoutPackages(PackagesToCheckOut);
	}
}

void FSceneOutlinerSCCHandler::ExecuteSCCCheckIn()
{
	TArray<UPackage*> Packages;
	GetSelectedPackages(Packages);

	// Prompt the user to ask if they would like to first save any dirty packages they are trying to check-in
	const FEditorFileUtils::EPromptReturnCode UserResponse = FEditorFileUtils::PromptForCheckoutAndSave( Packages, true, true );

	// If the user elected to save dirty packages, but one or more of the packages failed to save properly OR if the user
	// canceled out of the prompt, don't follow through on the check-in process
	const bool bShouldProceed = ( UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Success || UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Declined );
	if ( bShouldProceed )
	{
		TArray<FString> PackageNames;
		GetSelectedPackageNames(PackageNames);

		const bool bUseSourceControlStateCache = true;
		const bool bCheckinGood = FSourceControlWindows::PromptForCheckin(bUseSourceControlStateCache, PackageNames);

		if (!bCheckinGood)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "SCC_Checkin_Failed", "Check-in failed as a result of save failure."));
		}
	}
	else
	{
		// If a failure occurred, alert the user that the check-in was aborted. This warning shouldn't be necessary if the user cancelled
		// from the dialog, because they obviously intended to cancel the whole operation.
		if ( UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Failure )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "SCC_Checkin_Aborted", "Check-in aborted as a result of save failure.") );
		}
	}
}

void FSceneOutlinerSCCHandler::ExecuteSCCHistory()
{
	TArray<FString> PackageNames;
	GetSelectedPackageNames(PackageNames);
	FSourceControlWindows::DisplayRevisionHistory(SourceControlHelpers::PackageFilenames(PackageNames));
}

#undef LOCTEXT_NAMESPACE

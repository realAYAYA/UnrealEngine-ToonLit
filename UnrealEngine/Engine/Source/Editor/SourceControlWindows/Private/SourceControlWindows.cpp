// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceControlWindows.h"
#include "SSourceControlSubmit.h"

#include "FileHelpers.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SourceControlSettings.h"


#if SOURCE_CONTROL_WITH_SLATE

#define LOCTEXT_NAMESPACE "SourceControlWindows"


//---------------------------------------------------------------------------------------
// FCheckinResultInfo

FCheckinResultInfo::FCheckinResultInfo()
	: Result(ECommandResult::Failed)
	, bAutoCheckedOut(false)
{
}


//---------------------------------------------------------------------------------------
// FSourceControlWindows

TWeakPtr<SNotificationItem> FSourceControlWindows::ChoosePackagesToCheckInNotification;

TArray<FString> FSourceControlWindows::GetSourceControlLocations(const bool bContentOnly)
{
	TArray<FString> SourceControlLocations;

	{
		TArray<FString> RootPaths;
		FPackageName::QueryRootContentPaths(RootPaths);
		for (const FString& RootPath : RootPaths)
		{
			const FString RootPathOnDisk = FPackageName::LongPackageNameToFilename(RootPath);
			SourceControlLocations.Add(FPaths::ConvertRelativePathToFull(RootPathOnDisk));
		}
	}

	if (!bContentOnly)
	{
		SourceControlLocations.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir()));
		SourceControlLocations.Add(FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
	}

	return SourceControlLocations;
}

bool FSourceControlWindows::ChoosePackagesToCheckIn(const FSourceControlWindowsOnCheckInComplete& OnCompleteDelegate)
{
	if (!ISourceControlModule::Get().IsEnabled())
	{
		FCheckinResultInfo ResultInfo;
		ResultInfo.Description = LOCTEXT("SourceControlDisabled", "Source control is not enabled.");
		OnCompleteDelegate.ExecuteIfBound(ResultInfo);

		return false;
	}

	if (!ISourceControlModule::Get().GetProvider().IsAvailable())
	{
		FCheckinResultInfo ResultInfo;
		ResultInfo.Description = LOCTEXT("NoSCCConnection", "No connection to source control available!");

		FMessageLog EditorErrors("EditorErrors");
		EditorErrors.Warning(ResultInfo.Description)->AddToken(
			FDocumentationToken::Create(TEXT("Engine/UI/SourceControl")));
		EditorErrors.Notify();

		OnCompleteDelegate.ExecuteIfBound(ResultInfo);

		return false;
	}

	// Start selection process...

	// make sure we update the SCC status of all packages (this could take a long time, so we will run it as a background task)
	TArray<FString> Filenames;
	if (ISourceControlModule::Get().UsesCustomProjectDir())
	{
		FString SourceControlProjectDir = ISourceControlModule::Get().GetSourceControlProjectDir();
		Filenames.Add(SourceControlProjectDir);
	}
	else
	{
		Filenames = GetSourceControlLocations();
	}
	
	// make sure the SourceControlProvider state cache is populated as well
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlOperationRef Operation = ISourceControlOperation::Create<FUpdateStatus>();
	SourceControlProvider.Execute(
		Operation,
		Filenames,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateStatic(&FSourceControlWindows::ChoosePackagesToCheckInCallback, OnCompleteDelegate));

	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
	}

	FNotificationInfo Info(LOCTEXT("ChooseAssetsToCheckInIndicator", "Checking for assets to check in..."));
	Info.bFireAndForget = false;
	Info.ExpireDuration = 0.0f;
	Info.FadeOutDuration = 1.0f;

	if (SourceControlProvider.CanCancelOperation(Operation))
	{
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			LOCTEXT("ChoosePackagesToCheckIn_CancelButton", "Cancel"),
			LOCTEXT("ChoosePackagesToCheckIn_CancelButtonTooltip", "Cancel the check in operation."),
			FSimpleDelegate::CreateStatic(&FSourceControlWindows::ChoosePackagesToCheckInCancelled, Operation)
			));
	}

	ChoosePackagesToCheckInNotification = FSlateNotificationManager::Get().AddNotification(Info);

	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
	}

	return true;
}


bool FSourceControlWindows::CanChoosePackagesToCheckIn()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	
	return ISourceControlModule::Get().IsEnabled() &&
		ISourceControlModule::Get().GetProvider().IsAvailable() &&
		!ChoosePackagesToCheckInNotification.IsValid()
		;
}

bool FSourceControlWindows::ShouldChoosePackagesToCheckBeVisible()
{
	return GetDefault<USourceControlSettings>()->bEnableSubmitContentMenuAction;
}


bool FSourceControlWindows::PromptForCheckin(FCheckinResultInfo& OutResultInfo, const TArray<FString>& InPackageNames, const TArray<FString>& InPendingDeletePaths, const TArray<FString>& InConfigFiles, bool bUseSourceControlStateCache)
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Get filenames for packages and config to be checked in
	TArray<FString> AllFiles = SourceControlHelpers::PackageFilenames(InPackageNames);
	AllFiles.Append(InConfigFiles);

	// Prepare a list of files to have their states updated
	if (!bUseSourceControlStateCache)
	{
		TArray<FString> UpdateRequest;
		UpdateRequest.Append(AllFiles);

		// If there are pending delete paths to update, add them here.
		UpdateRequest.Append(InPendingDeletePaths);

		// Force an update on everything that's been requested
		if (UpdateRequest.Num() > 0)
		{
			SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), UpdateRequest);
		}
	}

	// Get file status of packages and config
	TArray<FSourceControlStateRef> States;
	SourceControlProvider.GetState(AllFiles, States, EStateCacheUsage::Use);

	if (InPendingDeletePaths.Num() > 0)
	{
		// Get any files pending delete
		TArray<FSourceControlStateRef> PendingDeleteItems = SourceControlProvider.GetCachedStateByPredicate(
			[&States](const FSourceControlStateRef& State) 
			{ 
				return State->IsDeleted() 
					// if the states already contains the pending delete do not bother appending it
					&& !States.Contains(State); 
			}
		);

		// And append them to the list
		States.Append(PendingDeleteItems);
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Exit if no assets needing check in
	if (States.Num() == 0)
	{
		OutResultInfo.Result      = ECommandResult::Succeeded;
		OutResultInfo.Description = LOCTEXT("NoAssetsToCheckIn", "No assets to check in!");

		FMessageLog EditorErrors("EditorErrors");
		EditorErrors.Warning(OutResultInfo.Description);
		EditorErrors.Notify();

		// Consider it a success even if no files were checked in
		return true;
	}


	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Create a submit files window
	TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.Title(NSLOCTEXT("SourceControl.SubmitWindow", "Title", "Submit Files"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(600, 400))
		.SupportsMaximize(true)
		.SupportsMinimize(false);

	TSharedRef<SSourceControlSubmitWidget> SourceControlWidget =
		SNew(SSourceControlSubmitWidget)
		.ParentWindow(NewWindow)
		.Items(States)
		.AllowUncheckFiles_Lambda([&]() 
			{ 
				if (SourceControlProvider.IsAvailable())
				{
					return SourceControlProvider.UsesFileRevisions();
				}
				return true;
			});

	NewWindow->SetContent(
		SourceControlWidget
		);

	FSlateApplication::Get().AddModalWindow(NewWindow, NULL);


	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Exit if cancelled by user
	if (SourceControlWidget->GetResult() == ESubmitResults::SUBMIT_CANCELED)
	{
		OutResultInfo.Result      = ECommandResult::Cancelled;
		OutResultInfo.Description = LOCTEXT("CheckinCancelled", "File check in cancelled.");

		return false;
	}


	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Get description from the dialog
	FChangeListDescription Description;
	SourceControlWidget->FillChangeListDescription(Description);


	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Revert any unchanged files
	if (Description.FilesForSubmit.Num() > 0)
	{
		SourceControlHelpers::RevertUnchangedFiles(SourceControlProvider, Description.FilesForSubmit);

		if (!ISourceControlModule::Get().UsesCustomProjectDir())
		{
			// Make sure all files are still checked out
			for (int32 VerifyIndex = Description.FilesForSubmit.Num() - 1; VerifyIndex >= 0; --VerifyIndex)
			{
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Description.FilesForSubmit[VerifyIndex], EStateCacheUsage::Use);
				if (SourceControlState.IsValid() && !SourceControlState->IsCheckedOut() && !SourceControlState->IsAdded() && !SourceControlState->IsDeleted())
				{
					Description.FilesForSubmit.RemoveAt(VerifyIndex);
				}
			}
		}
		else
		{
			// For project-based source control, we want to go through with a check in attempt even when 
			// files are not checked out by the current user, and generate a warning dialog
		}
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Mark files for add as needed
	
	bool bSuccess = true;  // Overall success
	bool bAddSuccess = true;
	bool bCheckinSuccess = true;
	bool bCheckinCancelled = false;

	TArray<FString> CombinedFileList = Description.FilesForAdd;
	CombinedFileList.Append(Description.FilesForSubmit);

	if (Description.FilesForAdd.Num() > 0)
	{
		bAddSuccess = SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), Description.FilesForAdd) == ECommandResult::Succeeded;
		bSuccess &= bAddSuccess;

		OutResultInfo.FilesAdded = Description.FilesForAdd;

		if (!bAddSuccess)
		{
			// Note that this message may be overwritten with a checkin error below.
			OutResultInfo.Description = LOCTEXT("SCC_Add_Files_Error", "One or more files were not able to be marked for add to version control!");
		}
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Any files to check in?
	if (CombinedFileList.Num() == 0)
	{
		OutResultInfo.Result = bSuccess ? ECommandResult::Succeeded : ECommandResult::Failed;

		if (OutResultInfo.Description.IsEmpty())
		{
			OutResultInfo.Description = LOCTEXT("SCC_No_Files", "No files were selected to check in to version control.");
		}

		return bSuccess;
	}

	FText VirtualizationFailureMsg;
	if (!TryToVirtualizeFilesToSubmit(CombinedFileList, Description.Description, VirtualizationFailureMsg))
	{
		FMessageLog("SourceControl").Notify(VirtualizationFailureMsg);

		OutResultInfo.Result = ECommandResult::Failed;
		OutResultInfo.Description = VirtualizationFailureMsg;

		return false;
	}

	// Check in files
	TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
	CheckInOperation->SetDescription(Description.Description);
	CheckInOperation->SetKeepCheckedOut(SourceControlWidget->WantToKeepCheckedOut());

	ECommandResult::Type CheckInResult = SourceControlProvider.Execute(CheckInOperation, CombinedFileList);
	bCheckinSuccess = CheckInResult == ECommandResult::Succeeded;
	bCheckinCancelled = CheckInResult == ECommandResult::Cancelled;

	bSuccess &= bCheckinSuccess;

	if (bCheckinSuccess)
	{
		// report success with a notification
		FNotificationInfo Info(CheckInOperation->GetSuccessMessage());
		Info.ExpireDuration = 8.0f;
		Info.HyperlinkText = LOCTEXT("SCC_Checkin_ShowLog", "Show Message Log");
		Info.Hyperlink = FSimpleDelegate::CreateStatic([](){ FMessageLog("SourceControl").Open(EMessageSeverity::Info, true); });
		FSlateNotificationManager::Get().AddNotification(Info);

		// also add to the log
		FMessageLog("SourceControl").Info(CheckInOperation->GetSuccessMessage());

		OutResultInfo.Result         = ECommandResult::Succeeded;
		OutResultInfo.Description    = CheckInOperation->GetSuccessMessage();
		OutResultInfo.FilesSubmitted = Description.FilesForSubmit;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Abort if cancelled
	if (bCheckinCancelled)
	{
		FText Message(LOCTEXT("CheckinCancelled", "File check in cancelled."));

		OutResultInfo.Result      = ECommandResult::Cancelled;
		OutResultInfo.Description = Message;

		return false;
	}
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Exit if errors
	if (!bSuccess)
	{
		FText Message(LOCTEXT("SCC_Checkin_Failed", "Failed to check in files!"));
		FMessageLog("SourceControl").Notify(Message);

		OutResultInfo.Result = ECommandResult::Failed;

		if (!bCheckinSuccess || OutResultInfo.Description.IsEmpty())
		{
			OutResultInfo.Description = Message;
		}

		return false;
	}

	SourceControlWidget->ClearChangeListDescription();

	return true;
}


bool FSourceControlWindows::PromptForCheckin(bool bUseSourceControlStateCache, const TArray<FString>& InPackageNames, const TArray<FString>& InPendingDeletePaths, const TArray<FString>& InConfigFiles)
{
	FCheckinResultInfo ResultInfo;

	return PromptForCheckin(ResultInfo, InPackageNames, InPendingDeletePaths, InConfigFiles, bUseSourceControlStateCache);
}


// Note that:
// - FSourceControlWindows::DisplayRevisionHistory() is defined in SSourceControlHistory.cpp
// - FSourceControlWindows::PromptForRevert() is defined in SSourceControlRevert.cpp


void FSourceControlWindows::ChoosePackagesToCheckInCompleted(const TArray<UPackage*>& LoadedPackages, const TArray<FString>& PackageNames, const TArray<FString>& ConfigFiles, FCheckinResultInfo& OutResultInfo)
{
	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
	}
	ChoosePackagesToCheckInNotification.Reset();

	// Prompt the user to ask if they would like to first save any dirty packages they are trying to check-in
	const FEditorFileUtils::EPromptReturnCode UserResponse = FEditorFileUtils::PromptForCheckoutAndSave(LoadedPackages, true, true);

	// If the user elected to save dirty packages, but one or more of the packages failed to save properly OR if the user
	// canceled out of the prompt, don't follow through on the check-in process
	const bool bShouldProceed = (UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Success || UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Declined);
	if (!bShouldProceed)
	{
		// If a failure occurred, alert the user that the check-in was aborted. This warning shouldn't be necessary if the user cancelled
		// from the dialog, because they obviously intended to cancel the whole operation.
		if (UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Failure)
		{
			OutResultInfo.Description = NSLOCTEXT("UnrealEd", "SCC_Checkin_Aborted", "Check-in aborted as a result of save failure.");

			FMessageDialog::Open(EAppMsgType::Ok, OutResultInfo.Description);
		}

		return;
	}

	TArray<FString> PendingDeletePaths;

	bool bUseSourceControlStateCache = true;
	if (ISourceControlModule::Get().UsesCustomProjectDir())
	{
		FString SourceControlProjectDir = ISourceControlModule::Get().GetSourceControlProjectDir();
		PendingDeletePaths.Add(SourceControlProjectDir);
		bUseSourceControlStateCache = false;
	}
	else
	{
		PendingDeletePaths = GetSourceControlLocations();
	}

	PromptForCheckin(OutResultInfo, PackageNames, PendingDeletePaths, ConfigFiles, bUseSourceControlStateCache);
}

void FSourceControlWindows::ChoosePackagesToCheckInCancelled(FSourceControlOperationRef InOperation)
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	SourceControlProvider.CancelOperation(InOperation);

	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
	}
	ChoosePackagesToCheckInNotification.Reset();
}

void FSourceControlWindows::ChoosePackagesToCheckInCallback(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, FSourceControlWindowsOnCheckInComplete OnCompleteDelegate)
{
	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
	}
	ChoosePackagesToCheckInNotification.Reset();

	FCheckinResultInfo ResultInfo;

	if (InResult != ECommandResult::Succeeded)
	{
		switch (InResult)
		{
			case ECommandResult::Cancelled:
				ResultInfo.Result      = ECommandResult::Cancelled;
				ResultInfo.Description = LOCTEXT("CheckInCancelled", "Check in cancelled.");
				break;

			case ECommandResult::Failed:
			{
				ResultInfo.Description = LOCTEXT("CheckInOperationFailed", "Failed checking source control status!");
				FMessageLog EditorErrors("EditorErrors");
				EditorErrors.Warning(ResultInfo.Description);
				EditorErrors.Notify();
			}
		}

		OnCompleteDelegate.ExecuteIfBound(ResultInfo);

		return;
	}

	// Get a list of all the checked out packages
	TArray<FString> PackageNames;
	TArray<UPackage*> LoadedPackages;
	TMap<FString, FSourceControlStatePtr> PackageStates;
	FEditorFileUtils::FindAllSubmittablePackageFiles(PackageStates, true);

	TArray<FString> ConfigFilesToSubmit;

	for (TMap<FString, FSourceControlStatePtr>::TConstIterator PackageIter(PackageStates); PackageIter; ++PackageIter)
	{
		const FString PackageName = *PackageIter.Key();

		UPackage* Package = FindPackage(nullptr, *PackageName);
		if (Package != nullptr)
		{
			LoadedPackages.Add(Package);
		}

		PackageNames.Add(PackageName);
	}

	// Get a list of all the checked out project files
	TMap<FString, FSourceControlStatePtr> ProjectFileStates;
	FEditorFileUtils::FindAllSubmittableProjectFiles(ProjectFileStates);
	for (TMap<FString, FSourceControlStatePtr>::TConstIterator It(ProjectFileStates); It; ++It)
	{
		ConfigFilesToSubmit.Add(It.Key());
	}

	// Get a list of all the checked out config files
	TMap<FString, FSourceControlStatePtr> ConfigFileStates;
	FEditorFileUtils::FindAllSubmittableConfigFiles(ConfigFileStates);
	for (TMap<FString, FSourceControlStatePtr>::TConstIterator It(ConfigFileStates); It; ++It)
	{
		ConfigFilesToSubmit.Add(It.Key());
	}

	ChoosePackagesToCheckInCompleted(LoadedPackages, PackageNames, ConfigFilesToSubmit, ResultInfo);
	OnCompleteDelegate.ExecuteIfBound(ResultInfo);
}


#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE


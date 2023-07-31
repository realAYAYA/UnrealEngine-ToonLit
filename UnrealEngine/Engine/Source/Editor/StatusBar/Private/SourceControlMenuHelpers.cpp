// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceControlMenuHelpers.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "ISourceControlWindowsModule.h"
#include "SourceControlMenuHelpers.h"
#include "SourceControlWindows.h"
#include "UnsavedAssetsTrackerModule.h"
#include "FileHelpers.h"
#include "ToolMenuContext.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "LevelEditorActions.h"
#include "PackageTools.h"
#include "AssetViewUtils.h"
#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "SourceControlCommands"

TSharedRef<FUICommandList> FSourceControlCommands::ActionList(new FUICommandList());

FSourceControlCommands::FSourceControlCommands() 
	: TCommands<FSourceControlCommands>
(
	"SourceControl",
	NSLOCTEXT("Contexts", "SourceControl", "Source Control"),
	"LevelEditor",
	FAppStyle::GetAppStyleSetName()
)
{}

/**
 * Initialize commands
 */
void FSourceControlCommands::RegisterCommands()
{
	UI_COMMAND(ConnectToSourceControl, "Connect to Source Control...", "Connect to source control to allow source control operations to be performed on content and levels.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ChangeSourceControlSettings, "Change Source Control Settings...", "Opens a dialog to change source control settings.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewChangelists, "View Changelists", "Opens a dialog displaying current changelists.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SubmitContent, "Submit Content", "Opens a dialog with check in options for content and levels.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CheckOutModifiedFiles, "Check Out Modified Files", "Opens a dialog to check out any assets which have been modified.", EUserInterfaceActionType::Button, FInputChord());

	ActionList->MapAction(
		ConnectToSourceControl,
		FExecuteAction::CreateStatic(&FSourceControlCommands::ConnectToSourceControl_Clicked)
	);

	ActionList->MapAction(
		ChangeSourceControlSettings,
		FExecuteAction::CreateStatic(&FSourceControlCommands::ConnectToSourceControl_Clicked)
	);

	ActionList->MapAction(
		ViewChangelists,
		FExecuteAction::CreateStatic(&FSourceControlCommands::ViewChangelists_Clicked),
		FCanExecuteAction::CreateStatic(&FSourceControlCommands::ViewChangelists_CanExecute),
		FIsActionChecked::CreateLambda([]() { return false; }),
		FIsActionButtonVisible::CreateStatic(&FSourceControlCommands::ViewChangelists_IsVisible)		
	);

	ActionList->MapAction(
		SubmitContent,
		FExecuteAction::CreateLambda([]() { FSourceControlWindows::ChoosePackagesToCheckIn(); }),
		FCanExecuteAction::CreateStatic(&FSourceControlWindows::CanChoosePackagesToCheckIn),
		FIsActionChecked::CreateLambda([]() { return false; }),
		FIsActionButtonVisible::CreateStatic(&FSourceControlCommands::SubmitContent_IsVisible)
	);

	ActionList->MapAction(
		CheckOutModifiedFiles,
		FExecuteAction::CreateStatic(&FSourceControlCommands::CheckOutModifiedFiles_Clicked),
		FCanExecuteAction::CreateStatic(&FSourceControlCommands::CheckOutModifiedFiles_CanExecute)
	);
}

void FSourceControlCommands::ConnectToSourceControl_Clicked()
{
	// Show login window regardless of current status - its useful as a shortcut to change settings.
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	SourceControlModule.ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless, EOnLoginWindowStartup::PreserveProvider);
}

bool FSourceControlCommands::ViewChangelists_CanExecute()
{
	return ISourceControlWindowsModule::Get().CanShowChangelistsTab();
}

bool FSourceControlCommands::ViewChangelists_IsVisible()
{
	return ISourceControlModule::Get().GetProvider().UsesChangelists();
}

bool FSourceControlCommands::SubmitContent_IsVisible()
{
	if (FSourceControlMenuHelpers::GetSourceControlCheckInStatusVisibility() == EVisibility::Visible)
	{
		return false;
	}

	if (!FSourceControlWindows::ShouldChoosePackagesToCheckBeVisible())
	{
		return false;
	}

	return true;
}

void FSourceControlCommands::ViewChangelists_Clicked()
{
	ISourceControlWindowsModule::Get().ShowChangelistsTab();
}

bool FSourceControlCommands::CheckOutModifiedFiles_CanExecute()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (ISourceControlModule::Get().IsEnabled() &&
		ISourceControlModule::Get().GetProvider().IsAvailable())
	{
		TArray<UPackage*> PackagesToSave;
		FEditorFileUtils::GetDirtyWorldPackages(PackagesToSave);
		FEditorFileUtils::GetDirtyContentPackages(PackagesToSave);

		return PackagesToSave.Num() > 0;
	}

	return false;
}

void FSourceControlCommands::CheckOutModifiedFiles_Clicked()
{
	TArray<UPackage*> PackagesToSave;
	FEditorFileUtils::GetDirtyWorldPackages(PackagesToSave);
	FEditorFileUtils::GetDirtyContentPackages(PackagesToSave);

	const bool bCheckDirty = true;
	const bool bPromptUserToSave = false;
	FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptUserToSave);
}

FSourceControlMenuHelpers& FSourceControlMenuHelpers::Get()
{
	// Singleton instance
	static FSourceControlMenuHelpers SourceControlMenuHelpers;
	return SourceControlMenuHelpers;
}

FSourceControlMenuHelpers::EQueryState FSourceControlMenuHelpers::QueryState = FSourceControlMenuHelpers::EQueryState::NotQueried;

void FSourceControlMenuHelpers::CheckSourceControlStatus()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (SourceControlModule.IsEnabled())
	{
		SourceControlModule.GetProvider().Execute(ISourceControlOperation::Create<FConnect>(),
			EConcurrency::Asynchronous,
			FSourceControlOperationComplete::CreateStatic(&FSourceControlMenuHelpers::OnSourceControlOperationComplete));
		QueryState = EQueryState::Querying;
	}
}

void FSourceControlMenuHelpers::OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	QueryState = EQueryState::Queried;
}

TSharedRef<SWidget> FSourceControlMenuHelpers::GenerateSourceControlMenuContent()
{
	UToolMenu* SourceControlMenu = UToolMenus::Get()->RegisterMenu("StatusBar.ToolBar.SourceControl", NAME_None, EMultiBoxType::Menu, false);

	FToolMenuSection& Section = SourceControlMenu->AddSection("SourceControlActions", LOCTEXT("SourceControlMenuHeadingActions", "Actions"));

	Section.AddMenuEntry(
		FSourceControlCommands::Get().ViewChangelists,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.ChangelistsTab")
	);

	Section.AddMenuEntry(
		FSourceControlCommands::Get().SubmitContent,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Submit")
	);

	Section.AddMenuEntry(
		FSourceControlCommands::Get().CheckOutModifiedFiles,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.CheckOut")
	);

	Section.AddDynamicEntry("ConnectToSourceControl", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
		if (ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable())
		{
			InSection.AddMenuEntry(
				FSourceControlCommands::Get().ChangeSourceControlSettings,
				TAttribute<FText>(),
				TAttribute<FText>(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.ChangeSettings")
			);
		}
		else
		{
			InSection.AddMenuEntry(
				FSourceControlCommands::Get().ConnectToSourceControl,
				TAttribute<FText>(),
				TAttribute<FText>(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Connect")
			);
		}
	}));

	return UToolMenus::Get()->GenerateWidget("StatusBar.ToolBar.SourceControl", FToolMenuContext(FSourceControlCommands::ActionList));
}

FText FSourceControlMenuHelpers::GetSourceControlStatusText()
{
	if (QueryState == EQueryState::Querying)
	{
		return LOCTEXT("SourceControlStatus_Querying", "Contacting Server....");
	}
	else
	{
		ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
		if (SourceControlModule.IsEnabled())
		{
			if (!SourceControlModule.GetProvider().IsAvailable())
			{
				return LOCTEXT("SourceControlStatus_Error_ServerUnavailable", "Server Unavailable");
			}
			else
			{
				return LOCTEXT("SourceControlStatus_Available", "Source Control");
			}
		}
		else
		{
			return LOCTEXT("SourceControlStatus_Error_Off", "Source Control"); // Relies on the icon on the status bar widget to know if the source control is on or off.
		}
	}
}
FText FSourceControlMenuHelpers::GetSourceControlTooltip()
{
	if (QueryState == EQueryState::Querying)
	{
		return LOCTEXT("SourceControlUnknown", "Source control status is unknown");
	}
	else
	{
		return ISourceControlModule::Get().GetProvider().GetStatusText();
	}
}

const FSlateBrush* FSourceControlMenuHelpers::GetSourceControlIcon()
{

	if (QueryState == EQueryState::Querying)
	{
		static const FSlateBrush* QueryBrush = FAppStyle::Get().GetBrush("SourceControl.StatusIcon.Unknown");
		return QueryBrush;
	}
	else
	{
		ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
		if (SourceControlModule.IsEnabled())
		{
			if (!SourceControlModule.GetProvider().IsAvailable())
			{
				static const FSlateBrush* ErrorBrush = FAppStyle::Get().GetBrush("SourceControl.StatusIcon.Error");
				return ErrorBrush;
			}
			else
			{
				static const FSlateBrush* OnBrush = FAppStyle::Get().GetBrush("SourceControl.StatusIcon.On");
				return OnBrush;
			}
		}
		else
		{
			static const FSlateBrush* OffBrush = FAppStyle::Get().GetBrush("SourceControl.StatusIcon.Off");
			return OffBrush;
		}
	}
}

/** Sync Status */

bool FSourceControlMenuHelpers::IsAtLatestRevision()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	return SourceControlModule.IsEnabled() && SourceControlModule.GetProvider().IsAvailable() && 
		SourceControlModule.GetProvider().IsAtLatestRevision().IsSet() && SourceControlModule.GetProvider().IsAtLatestRevision().GetValue();
}

EVisibility FSourceControlMenuHelpers::GetSourceControlSyncStatusVisibility()
{
	bool bDisplaySourceControlSyncStatus = false;
	GConfig->GetBool(TEXT("SourceControlSettings"), TEXT("DisplaySourceControlSyncStatus"), bDisplaySourceControlSyncStatus, GEditorIni);

	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (bDisplaySourceControlSyncStatus && SourceControlModule.IsEnabled() && SourceControlModule.GetProvider().IsAvailable() && SourceControlModule.GetProvider().IsAtLatestRevision().IsSet())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FText FSourceControlMenuHelpers::GetSourceControlSyncStatusText()
{
	if (IsAtLatestRevision())
	{
		return LOCTEXT("SyncLatestButtonAtHeadText", "At Latest");
	}
	return LOCTEXT("SyncLatestButtonNotAtHeadText", "Sync Latest");
}

FText FSourceControlMenuHelpers::GetSourceControlSyncStatusTooltipText()
{
	if (IsAtLatestRevision())
	{
		return LOCTEXT("SyncLatestButtonAtHeadTooltipText", "Currently at the latest Snapshot for this project");
	}
	return LOCTEXT("SyncLatestButtonNotAtHeadTooltipText", "Get the latest Snapshot for this project");
}

const FSlateBrush* FSourceControlMenuHelpers::GetSourceControlSyncStatusIcon()
{
	static const FSlateBrush* AtHeadBrush = FAppStyle::Get().GetBrush("SourceControl.StatusBar.AtLatestRevision");
	static const FSlateBrush* NotAtHeadBrush = FAppStyle::Get().GetBrush("SourceControl.StatusBar.NotAtLatestRevision");

	if (IsAtLatestRevision())
	{
		return AtHeadBrush;
	}
	return NotAtHeadBrush;
}

FReply FSourceControlMenuHelpers::OnSourceControlSyncClicked()
{
	FSourceControlMenuHelpers::Get().SyncProject();

	return FReply::Handled();
}

void FSourceControlMenuHelpers::SyncProject()
{
	const bool bSaved = SaveDirtyPackages();
	if (bSaved)
	{
		AssetViewUtils::SyncPackagesFromSourceControl(ListAllPackages());
	}
	else
	{
		FMessageLog SourceControlLog("SourceControl");
		SourceControlLog.Warning(LOCTEXT("SourceControlMenu_Sync_Unsaved", "Save All Assets before attempting to Sync!"));
		SourceControlLog.Notify();
	}
}

/// Prompt to save or discard all packages
bool FSourceControlMenuHelpers::SaveDirtyPackages()
{
	const bool bPromptUserToSave = true;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	const bool bFastSave = false;
	const bool bNotifyNoPackagesSaved = false;
	const bool bCanBeDeclined = true; // If the user clicks "don't save" this will continue and lose their changes
	bool bHadPackagesToSave = false;

	bool bSaved = FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined, &bHadPackagesToSave);

	// bSaved can be true if the user selects to not save an asset by unchecking it and clicking "save"
	if (bSaved)
	{
		TArray<UPackage*> DirtyPackages;
		FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);
		FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);
		bSaved = DirtyPackages.Num() == 0;
	}

	return bSaved;
}

/// Find all packages in Content directory
TArray<FString> FSourceControlMenuHelpers::ListAllPackages()
{
	const FString ProjectContentDir = FPaths::ProjectContentDir();
	const FString ProjectSourceControlDir = ISourceControlModule::Get().GetSourceControlProjectDir();
	const FString& Root = ProjectSourceControlDir.IsEmpty() ? ProjectContentDir : ProjectSourceControlDir;

	TArray<FString> PackageRelativePaths;
	FPackageName::FindPackagesInDirectory(PackageRelativePaths, FPaths::ConvertRelativePathToFull(Root));

	TArray<FString> PackageNames;
	PackageNames.Reserve(PackageRelativePaths.Num());
	for (const FString& Path : PackageRelativePaths)
	{
		FString PackageName;
		FString FailureReason;
		if (FPackageName::TryConvertFilenameToLongPackageName(Path, PackageName, &FailureReason))
		{
			PackageNames.Add(PackageName);
		}
		else
		{
			FMessageLog("SourceControl").Error(FText::FromString(FailureReason));
		}
	}

	return PackageNames;
}

/** Check-in Status */

int FSourceControlMenuHelpers::GetNumLocalChanges()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (SourceControlModule.IsEnabled() && SourceControlModule.GetProvider().IsAvailable() 
		&& SourceControlModule.GetProvider().GetNumLocalChanges().IsSet())
	{
		return SourceControlModule.GetProvider().GetNumLocalChanges().GetValue();
	}
	return 0;
}

EVisibility FSourceControlMenuHelpers::GetSourceControlCheckInStatusVisibility()
{
	bool bDisplaySourceControlCheckInStatus = false;
	GConfig->GetBool(TEXT("SourceControlSettings"), TEXT("DisplaySourceControlCheckInStatus"), bDisplaySourceControlCheckInStatus, GEditorIni);

	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (bDisplaySourceControlCheckInStatus && SourceControlModule.IsEnabled() && SourceControlModule.GetProvider().IsAvailable() && SourceControlModule.GetProvider().GetNumLocalChanges().IsSet())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FText FSourceControlMenuHelpers::GetSourceControlCheckInStatusText()
{
	if (GetNumLocalChanges() > 0)
	{
		return LOCTEXT("CheckInButtonChangesText", "Check-in Changes");
	}
	return LOCTEXT("CheckInButtonNoChangesText", "No Changes");
}


FText FSourceControlMenuHelpers::GetSourceControlCheckInStatusTooltipText()
{
	if (GetNumLocalChanges() > 0)
	{
		return FText::Format(LOCTEXT("CheckInButtonChangesTooltipText", "Check-in {0} change(s) to this project"), GetNumLocalChanges());
	}
	return LOCTEXT("CheckInButtonNoChangesTooltipText", "No Changes to check in for this project");
}

const FSlateBrush* FSourceControlMenuHelpers::GetSourceControlCheckInStatusIcon()
{
	static const FSlateBrush* NoLocalChangesBrush = FAppStyle::Get().GetBrush("SourceControl.StatusBar.NoLocalChanges");
	static const FSlateBrush* HasLocalChangesBrush = FAppStyle::Get().GetBrush("SourceControl.StatusBar.HasLocalChanges");

	if (GetNumLocalChanges() > 0)
	{
		return HasLocalChangesBrush;
	}
	return NoLocalChangesBrush;
}

FReply FSourceControlMenuHelpers::OnSourceControlCheckInChangesClicked()
{
	if (FSourceControlWindows::CanChoosePackagesToCheckIn())
	{
		FSourceControlWindows::ChoosePackagesToCheckIn();
	}

	return FReply::Handled();
}

TSharedRef<SWidget> FSourceControlMenuHelpers::MakeSourceControlStatusWidget()
{
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			// NOTE: The unsaved can have its own menu extension in the status bar to decouple it from source control, but putting all the buttons in the right order
			//       on the status bar is not deterministic, you need to know the name of the menu that is before or after and the menu is dynamic. Having it here
			//       ensure its position with respect to the source control button.
			FUnsavedAssetsTrackerModule::Get().MakeUnsavedAssetsStatusBarWidget()
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("StatusBar.StatusBarButton"))
			.ToolTipText_Static(&FSourceControlMenuHelpers::GetSourceControlSyncStatusTooltipText)
			.Visibility_Static(&FSourceControlMenuHelpers::GetSourceControlSyncStatusVisibility)
			.IsEnabled_Lambda([]() { return !IsAtLatestRevision(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image_Static(&FSourceControlMenuHelpers::GetSourceControlSyncStatusIcon)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(STextBlock)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
					.Text_Static(&FSourceControlMenuHelpers::GetSourceControlSyncStatusText)
				]
			]
			.OnClicked_Static(&FSourceControlMenuHelpers::OnSourceControlSyncClicked)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(FMargin(8.0f, 0.0f, 4.0f, 0.0f))
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("StatusBar.StatusBarButton"))
			.ToolTipText_Static(&FSourceControlMenuHelpers::GetSourceControlCheckInStatusTooltipText)
			.Visibility_Static(&FSourceControlMenuHelpers::GetSourceControlCheckInStatusVisibility)
			.IsEnabled_Lambda([]() { return GetNumLocalChanges() > 0; })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image_Static(&FSourceControlMenuHelpers::GetSourceControlCheckInStatusIcon)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(STextBlock)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
					.Text_Static(&FSourceControlMenuHelpers::GetSourceControlCheckInStatusText)
				]
			]
			.OnClicked_Static(&FSourceControlMenuHelpers::OnSourceControlCheckInChangesClicked)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SComboButton)
			.ContentPadding(FMargin(6.0f, 0.0f))
			.ToolTipText_Static(&FSourceControlMenuHelpers::GetSourceControlTooltip)
			.MenuPlacement(MenuPlacement_AboveAnchor)
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("StatusBar.StatusBarComboButton"))
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image_Static(&FSourceControlMenuHelpers::GetSourceControlIcon)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(STextBlock)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
					.Text_Static(&FSourceControlMenuHelpers::GetSourceControlStatusText)
				]
			]
			.OnGetMenuContent(FOnGetContent::CreateStatic(&FSourceControlMenuHelpers::GenerateSourceControlMenuContent))
		];
}

#undef LOCTEXT_NAMESPACE
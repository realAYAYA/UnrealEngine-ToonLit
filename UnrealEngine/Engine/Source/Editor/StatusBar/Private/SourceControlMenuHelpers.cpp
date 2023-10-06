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
#include "Logging/MessageLog.h"
#include "ToolMenuContext.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SLayeredImage.h"
#include "LevelEditorActions.h"
#include "PackageTools.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Bookmarks/BookmarkScoped.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "SourceControlCommands"

TSharedRef<FUICommandList> FSourceControlCommands::ActionList(new FUICommandList());

FSourceControlCommands::FSourceControlCommands() 
	: TCommands<FSourceControlCommands>
(
	"SourceControl",
	NSLOCTEXT("Contexts", "SourceControl", "Revision Control"),
	"LevelEditor",
	FAppStyle::GetAppStyleSetName()
)
{}

/**
 * Initialize commands
 */
void FSourceControlCommands::RegisterCommands()
{
	UI_COMMAND(ConnectToSourceControl, "Connect to Revision Control...", "Connect to a revision control system for tracking changes to your content and levels.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ChangeSourceControlSettings, "Change Revision Control Settings...", "Opens a dialog to change revision control settings.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewChangelists, "View Changes", "Opens a dialog displaying current changes.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SubmitContent, "Submit Content", "Opens a dialog with check in options for content and levels.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CheckOutModifiedFiles, "Check Out Modified Files", "Opens a dialog to check out any assets which have been modified.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RevertAll, "Revert All Files", "Opens a dialog to revert any assets which have been modified.", EUserInterfaceActionType::Button, FInputChord());

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

	ActionList->MapAction(
		RevertAll,
		FExecuteAction::CreateStatic(&FSourceControlCommands::RevertAllModifiedFiles_Clicked),
		FCanExecuteAction::CreateStatic(&FSourceControlCommands::RevertAllModifiedFiles_CanExecute)
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
	return ISourceControlModule::Get().GetProvider().UsesChangelists() || ISourceControlModule::Get().GetProvider().UsesUncontrolledChangelists();
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

	FEditorFileUtils::FPromptForCheckoutAndSaveParams SaveParams;
	SaveParams.bCheckDirty = true;
	SaveParams.bPromptToSave = false;
	SaveParams.bIsExplicitSave = true;

	FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, SaveParams);
}

bool FSourceControlCommands::RevertAllModifiedFiles_CanExecute()
{
	// If CHECK-IN CHANGES button is active, the REVERT ALL option should be active.

	if (FUnsavedAssetsTrackerModule::Get().GetUnsavedAssetNum() > 0)
	{
		return true;
	}

	if (FSourceControlWindows::CanChoosePackagesToCheckIn())
	{
		return true;
	}

	return false;
}

void FSourceControlCommands::RevertAllModifiedFiles_Clicked()
{
	FText Message = LOCTEXT("RevertAllModifiedFiles", "Are you sure you want to revert all local changes? By proceeding, your local changes will be discarded, and the state of your last synced snapshot will be restored.");
	FText Title = LOCTEXT("RevertAllModifiedFiles_Title", "Revert all local changes");
	if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No, Message, Title) == EAppReturnType::Yes)
	{
		FSourceControlMenuHelpers::SaveUnsavedFiles();
		FBookmarkScoped BookmarkScoped;
		FSourceControlWindows::RevertAllChangesAndReloadWorld();
	}
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
		FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.ChangelistsTab")
	);

	Section.AddMenuEntry(
		FSourceControlCommands::Get().SubmitContent,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.Submit")
	);

	Section.AddMenuEntry(
		FSourceControlCommands::Get().CheckOutModifiedFiles,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.CheckOut")
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
				FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.ChangeSettings")
			);
		}
		else
		{
			InSection.AddMenuEntry(
				FSourceControlCommands::Get().ConnectToSourceControl,
				TAttribute<FText>(),
				TAttribute<FText>(),
				FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Icon")
			);
		}
	}));

	return UToolMenus::Get()->GenerateWidget("StatusBar.ToolBar.SourceControl", FToolMenuContext(FSourceControlCommands::ActionList));
}

TSharedRef<SWidget> FSourceControlMenuHelpers::GenerateCheckInComboButtonContent()
{
	UToolMenu* SourceControlMenu = UToolMenus::Get()->RegisterMenu("StatusBar.ToolBar.SourceControl.CheckInCombo", NAME_None, EMultiBoxType::Menu, false);

	FToolMenuSection& Section = SourceControlMenu->AddSection("SourceControlComboActions", LOCTEXT("SourceControlComboMenuHeadingActions", "Actions"));

	Section.AddMenuEntry(
		FSourceControlCommands::Get().RevertAll,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Revert")
	);

	Section.AddMenuEntry(
		FSourceControlCommands::Get().ViewChangelists,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.ChangelistsTab")
	);

	return UToolMenus::Get()->GenerateWidget("StatusBar.ToolBar.SourceControl.CheckInCombo", FToolMenuContext(FSourceControlCommands::ActionList));
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
				return LOCTEXT("SourceControlStatus_Available", "Revision Control");
			}
		}
		else
		{
			return LOCTEXT("SourceControlStatus_Error_Off", "Revision Control"); // Relies on the icon on the status bar widget to know if the source control is on or off.
		}
	}
}
FText FSourceControlMenuHelpers::GetSourceControlTooltip()
{
	if (QueryState == EQueryState::Querying)
	{
		return LOCTEXT("SourceControlUnknown", "Revision control status is unknown");
	}
	else
	{
		return ISourceControlModule::Get().GetProvider().GetStatusText();
	}
}

const FSlateBrush* FSourceControlMenuHelpers::GetSourceControlIconBadge()
{
	if (QueryState == EQueryState::Querying)
	{
		return nullptr;
	}
	else
	{
		ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
		if (SourceControlModule.IsEnabled())
		{
			if (!SourceControlModule.GetProvider().IsAvailable())
			{
				static const FSlateBrush* ErrorBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon.WarningBadge");
				return ErrorBrush;
			}
			else
			{
				static const FSlateBrush* OnBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon.ConnectedBadge");
				return OnBrush;
			}
		}
		else
		{
			static const FSlateBrush* OffBrush = nullptr;
			return OffBrush;
		}
	}
}

/** Sync Status */

bool FSourceControlMenuHelpers::IsAtLatestRevision()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	return SourceControlModule.IsEnabled() &&
		SourceControlModule.GetProvider().IsAvailable() &&
		SourceControlModule.GetProvider().IsAtLatestRevision().IsSet() &&
		SourceControlModule.GetProvider().IsAtLatestRevision().GetValue();
}

bool FSourceControlMenuHelpers::CanSourceControlSync()
{
	return !IsAtLatestRevision();
}

EVisibility FSourceControlMenuHelpers::GetSourceControlSyncStatusVisibility()
{
	bool bDisplaySourceControlSyncStatus = false;
	GConfig->GetBool(TEXT("SourceControlSettings"), TEXT("DisplaySourceControlSyncStatus"), bDisplaySourceControlSyncStatus, GEditorIni);

	if (bDisplaySourceControlSyncStatus)
	{
		ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
		if (SourceControlModule.IsEnabled() &&
			SourceControlModule.GetProvider().IsAvailable() &&
			SourceControlModule.GetProvider().IsAtLatestRevision().IsSet()) // Only providers that implement IsAtLatestRevision are supported.
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

FText FSourceControlMenuHelpers::GetSourceControlSyncStatusText()
{
	if (CanSourceControlSync())
	{
		return LOCTEXT("SyncLatestButtonNotAtHeadText", "Sync Latest");
	}
	return LOCTEXT("SyncLatestButtonAtHeadText", "At Latest");
}

FText FSourceControlMenuHelpers::GetSourceControlSyncStatusTooltipText()
{
	if (CanSourceControlSync())
	{
		return LOCTEXT("SyncLatestButtonNotAtHeadTooltipText", "Sync to the latest Snapshot for this project");
	}
	return LOCTEXT("SyncLatestButtonAtHeadTooltipText", "Currently at the latest Snapshot for this project");
}

const FSlateBrush* FSourceControlMenuHelpers::GetSourceControlSyncStatusIcon()
{
	static const FSlateBrush* AtHeadBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.AtLatestRevision");
	static const FSlateBrush* NotAtHeadBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.NotAtLatestRevision");

	if (CanSourceControlSync())
	{
		return NotAtHeadBrush;
	}
	return AtHeadBrush;
}

FReply FSourceControlMenuHelpers::OnSourceControlSyncClicked()
{
	if (FSourceControlWindows::CanSyncLatest())
	{
		FBookmarkScoped BookmarkScoped;
		FSourceControlWindows::SyncLatest();
	}

	return FReply::Handled();
}

/** Check-in Status */

int FSourceControlMenuHelpers::GetNumLocalChanges()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (SourceControlModule.IsEnabled() &&
		SourceControlModule.GetProvider().IsAvailable() &&
		SourceControlModule.GetProvider().GetNumLocalChanges().IsSet())
	{
		return SourceControlModule.GetProvider().GetNumLocalChanges().GetValue();
	}
	return 0;
}

void FSourceControlMenuHelpers::SaveUnsavedFiles()
{
	// Get a list of all the unsaved packages
	TArray<FString> UnsavedFileNames = FUnsavedAssetsTrackerModule::Get().GetUnsavedAssets();
	if (UnsavedFileNames.Num() > 0)
	{
		TArray<UPackage*> UnsavedPackages;
		UnsavedPackages.Reserve(UnsavedFileNames.Num());

		for (FString& FileName : UnsavedFileNames)
		{
			FString PackageName = UPackageTools::FilenameToPackageName(FileName);
			UPackage* Package = FindPackage(nullptr, *PackageName);
			if (Package != nullptr)
			{
				UnsavedPackages.Add(Package);
			}
		}

		UEditorLoadingAndSavingUtils::SavePackages(UnsavedPackages, /*bOnlyDirty=*/true);
	}
}

bool FSourceControlMenuHelpers::CanSourceControlCheckIn()
{
	return (GetNumLocalChanges() > 0);
}

EVisibility FSourceControlMenuHelpers::GetSourceControlCheckInStatusVisibility()
{
	bool bDisplaySourceControlCheckInStatus = false;
	GConfig->GetBool(TEXT("SourceControlSettings"), TEXT("DisplaySourceControlCheckInStatus"), bDisplaySourceControlCheckInStatus, GEditorIni);

	if (bDisplaySourceControlCheckInStatus)
	{
		ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
		if (SourceControlModule.IsEnabled() &&
			SourceControlModule.GetProvider().IsAvailable() &&
			SourceControlModule.GetProvider().GetNumLocalChanges().IsSet()) // Only providers that implement GetNumLocalChanges are supported.
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

FText FSourceControlMenuHelpers::GetSourceControlCheckInStatusText()
{
	if (CanSourceControlCheckIn())
	{
		return LOCTEXT("CheckInButtonChangesText", "Check-in Changes");
	}
	return LOCTEXT("CheckInButtonNoChangesText", "No Changes");
}


FText FSourceControlMenuHelpers::GetSourceControlCheckInStatusTooltipText()
{
	if (CanSourceControlCheckIn())
	{
		return FText::Format(LOCTEXT("CheckInButtonChangesTooltipText", "Check-in {0} change(s) to this project"), GetNumLocalChanges());
	}
	return LOCTEXT("CheckInButtonNoChangesTooltipText", "No Changes to check in for this project");
}

const FSlateBrush* FSourceControlMenuHelpers::GetSourceControlCheckInStatusIcon()
{
	static const FSlateBrush* NoLocalChangesBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.NoLocalChanges");
	static const FSlateBrush* HasLocalChangesBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.HasLocalChanges");

	if (CanSourceControlCheckIn())
	{
		return HasLocalChangesBrush;
	}
	return NoLocalChangesBrush;
}

FReply FSourceControlMenuHelpers::OnSourceControlCheckInChangesClicked()
{
	if (CanSourceControlCheckIn())
	{
		bool bSyncNeeded = FSourceControlWindows::CanSyncLatest();
		bool bSyncSuccess = true;
		if (bSyncNeeded)
		{
			FBookmarkScoped BookmarkScoped;
			bSyncSuccess = FSourceControlWindows::SyncLatest();
		}

		if (bSyncSuccess)
		{
			FSourceControlMenuHelpers::SaveUnsavedFiles();
			FSourceControlWindows::ChoosePackagesToCheckIn();
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> FSourceControlMenuHelpers::MakeSourceControlStatusWidget()
{
	TSharedRef<SLayeredImage> SourceControlIcon =
		SNew(SLayeredImage)
		.ColorAndOpacity(FSlateColor::UseForeground())
		.Image(FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon"));

	SourceControlIcon->AddLayer(TAttribute<const FSlateBrush*>::CreateStatic(&FSourceControlMenuHelpers::GetSourceControlIconBadge));

	
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
		.AutoWidth()
		.Padding(4.0f, -5.0f) // Intentional negative padding to make the separator cover the whole status bar vertically
		[
			SNew(SSeparator)
			.Thickness(2.0f)
			.Orientation(EOrientation::Orient_Vertical)
		]
		+ SHorizontalBox::Slot() // Check In Changes Button
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("StatusBar.StatusBarButton"))
			.ToolTipText_Static(&FSourceControlMenuHelpers::GetSourceControlCheckInStatusTooltipText)
			.Visibility_Static(&FSourceControlMenuHelpers::GetSourceControlCheckInStatusVisibility)
			.IsEnabled_Lambda([]() { return CanSourceControlCheckIn(); })
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
		+SHorizontalBox::Slot() // Check In Kebab Combo button
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SComboButton)
			.ContentPadding(FMargin(7.f, 0.f))
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("StatusBar.StatusBarEllipsisComboButton"))
			.MenuPlacement(MenuPlacement_AboveAnchor)
			.Visibility_Static(&FSourceControlMenuHelpers::GetSourceControlCheckInStatusVisibility)
			.OnGetMenuContent(FOnGetContent::CreateStatic(&FSourceControlMenuHelpers::GenerateCheckInComboButtonContent))
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SSeparator)
			.Visibility_Static(&FSourceControlMenuHelpers::GetSourceControlCheckInStatusVisibility)
			.Thickness(1.0)
			.Orientation(EOrientation::Orient_Vertical)
		]
		+ SHorizontalBox::Slot() // Sync Latest Button
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("StatusBar.StatusBarButton"))
			.ToolTipText_Static(&FSourceControlMenuHelpers::GetSourceControlSyncStatusTooltipText)
			.Visibility_Static(&FSourceControlMenuHelpers::GetSourceControlSyncStatusVisibility)
			.IsEnabled_Lambda([]() { return CanSourceControlSync(); })
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
		.AutoWidth()
		[
			SNew(SSeparator)
			.Visibility_Static(&FSourceControlMenuHelpers::GetSourceControlCheckInStatusVisibility)
			.Thickness(1.0)
			.Orientation(EOrientation::Orient_Vertical)
		]
		+ SHorizontalBox::Slot() // Source Control Menu
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SComboButton)
			.ContentPadding(FMargin(6.0f, 0.0f))
			.ToolTipText_Static(&FSourceControlMenuHelpers::GetSourceControlTooltip)
			.MenuPlacement(MenuPlacement_AboveAnchor)
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SourceControlIcon
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
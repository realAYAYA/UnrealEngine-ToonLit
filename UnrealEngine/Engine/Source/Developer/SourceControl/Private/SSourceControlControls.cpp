// Copyright Epic Games, Inc. All Rights Reserved.

#if SOURCE_CONTROL_WITH_SLATE

#include "SSourceControlControls.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "Misc/ConfigCacheIni.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "SSkeinSourceControlWidgets"

int32 SSourceControlControls::NumConflictsRemaining = 0;

FIsEnabled SSourceControlControls::IsSyncLatestEnabled;
FIsEnabled SSourceControlControls::IsCheckInChangesEnabled;
FIsEnabled SSourceControlControls::IsRestoreAsLatestEnabled;

FIsVisible SSourceControlControls::IsSyncLatestVisible;
FIsVisible SSourceControlControls::IsCheckInChangesVisible;
FIsVisible SSourceControlControls::IsRestoreAsLatestVisible;

FOnClicked SSourceControlControls::OnSyncLatestClicked;
FOnClicked SSourceControlControls::OnCheckInChangesClicked;
FOnClicked SSourceControlControls::OnRestoreAsLatestClicked;

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SSourceControlControls::Construct(const FArguments& InArgs)
{
	IsMiddleSeparatorEnabled = InArgs._IsEnabledMiddleSeparator;
	IsRightSeparatorEnabled = InArgs._IsEnabledRightSeparator;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot() // Check In Changes Button
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("StatusBar.StatusBarButton"))
			.ToolTipText_Static(&SSourceControlControls::GetSourceControlCheckInStatusToolTipText)
			.Visibility_Static(&SSourceControlControls::GetSourceControlCheckInStatusVisibility)
			.IsEnabled_Static(&SSourceControlControls::IsSourceControlCheckInEnabled)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image_Static(&SSourceControlControls::GetSourceControlCheckInStatusIcon)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(STextBlock)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
					.Text_Static(&SSourceControlControls::GetSourceControlCheckInStatusText)
				]
			]
			.OnClicked_Static(&SSourceControlControls::OnSourceControlCheckInChangesClicked)
		]
		+ SHorizontalBox::Slot() // Restore as Latest button
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("StatusBar.StatusBarButton"))
			.ToolTipText_Static(&SSourceControlControls::GetSourceControlRestoreAsLatestToolTipText)
			.Visibility_Static(&SSourceControlControls::GetSourceControlRestoreAsLatestVisibility)
			.IsEnabled_Static(&SSourceControlControls::IsSourceControlRestoreAsLatestEnabled)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image_Static(&SSourceControlControls::GetSourceControlRestoreAsLatestStatusIcon)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(STextBlock)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
					.Text_Static(&SSourceControlControls::GetSourceControlRestoreAsLatestText)
				]
			]
			.OnClicked_Static(&SSourceControlControls::OnSourceControlRestoreAsLatestClicked)
		]
		+SHorizontalBox::Slot() // Check In Kebab Combo button
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SComboButton)
			.ContentPadding(FMargin(7.f, 0.f))
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("StatusBar.StatusBarEllipsisComboButton"))
			.MenuPlacement(MenuPlacement_AboveAnchor)
			.Visibility_Static(&SSourceControlControls::GetSourceControlCheckInStatusVisibility)
			.OnGetMenuContent(InArgs._OnGenerateKebabMenu)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SSeparator)
			.Visibility(this, &SSourceControlControls::GetSourceControlMiddleSeparatorVisibility)
			.Thickness(1.0)
			.Orientation(EOrientation::Orient_Vertical)
		]
		+ SHorizontalBox::Slot() // Sync Latest Button
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("StatusBar.StatusBarButton"))
			.ToolTipText_Static(&SSourceControlControls::GetSourceControlSyncStatusToolTipText)
			.Visibility_Static(&SSourceControlControls::GetSourceControlSyncStatusVisibility)
			.IsEnabled_Static(&SSourceControlControls::IsSourceControlSyncEnabled)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image_Static(&SSourceControlControls::GetSourceControlSyncStatusIcon)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(STextBlock)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
					.Text_Static(&SSourceControlControls::GetSourceControlSyncStatusText)
				]
			]
			.OnClicked_Static(&SSourceControlControls::OnSourceControlSyncClicked)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SSeparator)
			.Visibility(this, &SSourceControlControls::GetSourceControlRightSeparatorVisibility)
			.Thickness(1.0)
			.Orientation(EOrientation::Orient_Vertical)
		]
	];

	CheckSourceControlStatus();
}

void SSourceControlControls::CheckSourceControlStatus()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();

	if (!SourceControlProviderChangedHandle.IsValid())
	{
		SourceControlProviderChangedHandle = SourceControlModule.RegisterProviderChanged(
			FSourceControlProviderChanged::FDelegate::CreateSP(this, &SSourceControlControls::OnSourceControlProviderChanged)
		);
		SourceControlStateChangedHandle = SourceControlModule.GetProvider().RegisterSourceControlStateChanged_Handle(
			FSourceControlStateChanged::FDelegate::CreateSP(this, &SSourceControlControls::OnSourceControlStateChanged)
		);
	}
}

void SSourceControlControls::OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider)
{
	if (SourceControlStateChangedHandle.IsValid())
	{
		OldProvider.UnregisterSourceControlStateChanged_Handle(SourceControlStateChangedHandle);
		SourceControlStateChangedHandle.Reset();
	}

	if (!IsEngineExitRequested())
	{
		SourceControlStateChangedHandle = NewProvider.RegisterSourceControlStateChanged_Handle(
			FSourceControlStateChanged::FDelegate::CreateSP(this, &SSourceControlControls::OnSourceControlStateChanged)
		);
	}
}

void SSourceControlControls::OnSourceControlStateChanged()
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	TArray<FSourceControlStateRef> Conflicts = SourceControlProvider.GetCachedStateByPredicate(
		[](const FSourceControlStateRef& State)
		{
			return State->IsConflicted();
		}
	);

	NumConflictsRemaining = Conflicts.Num(); // Atomic write.
}

int32 SSourceControlControls::GetNumConflictsRemaining()
{
	return NumConflictsRemaining;
}

/** Sync Status */

bool SSourceControlControls::IsAtLatestRevision()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	return SourceControlModule.IsEnabled() &&
		SourceControlModule.GetProvider().IsAvailable() &&
		SourceControlModule.GetProvider().IsAtLatestRevision().IsSet() &&
		SourceControlModule.GetProvider().IsAtLatestRevision().GetValue();
}

bool SSourceControlControls::IsSourceControlSyncEnabled()
{
	if (!HasSourceControlChangesToSync())
	{
		return false;
	}

	if (IsSyncLatestEnabled.IsBound())
	{
		return IsSyncLatestEnabled.Execute();
	}

	return false;
}

bool SSourceControlControls::HasSourceControlChangesToSync()
{
	return !IsAtLatestRevision();
}

EVisibility SSourceControlControls::GetSourceControlSyncStatusVisibility()
{
	if (!GIsEditor)
	{
		// Always visible in the Slate Viewer
		return EVisibility::Visible;
	}

	bool bVisibleSourceControlSyncStatus = true;
	if (IsSyncLatestVisible.IsBound())
	{
		bVisibleSourceControlSyncStatus = IsSyncLatestVisible.Execute();
	}

	bool bDisplaySourceControlSyncStatus = false;
	GConfig->GetBool(TEXT("SourceControlSettings"), TEXT("DisplaySourceControlSyncStatus"), bDisplaySourceControlSyncStatus, GEditorIni);

	if (bVisibleSourceControlSyncStatus && bDisplaySourceControlSyncStatus)
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

EVisibility SSourceControlControls::GetSourceControlRightSeparatorVisibility() const
{
	EVisibility StatusVisibility = GetSourceControlSyncStatusVisibility();
	if (StatusVisibility != EVisibility::Visible)
	{
		return StatusVisibility;
	}

	return IsRightSeparatorEnabled.Get(true) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SSourceControlControls::GetSourceControlSyncStatusText()
{
	if (HasSourceControlChangesToSync())
	{
		return LOCTEXT("SyncLatestButtonNotAtHeadText", "Sync Latest");
	}

	return LOCTEXT("SyncLatestButtonAtHeadText", "At Latest");
}

FText SSourceControlControls::GetSourceControlSyncStatusToolTipText()
{
	if (GetNumConflictsRemaining() > 0)
	{
		return LOCTEXT("SyncLatestButtonNotAtHeadTooltipTextConflict", "Some of your local changes conflict with the latest snapshot of the project. Click here to review these conflicts.");
	}
	if (HasSourceControlChangesToSync())
	{
		return LOCTEXT("SyncLatestButtonNotAtHeadTooltipText", "Sync to the latest Snapshot for this project");
	}

	return LOCTEXT("SyncLatestButtonAtHeadTooltipText", "Currently at the latest Snapshot for this project");
}

const FSlateBrush* SSourceControlControls::GetSourceControlSyncStatusIcon()
{
	static const FSlateBrush* ConflictBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.Conflicted");
	static const FSlateBrush* AtHeadBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.AtLatestRevision");
	static const FSlateBrush* NotAtHeadBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.NotAtLatestRevision");

	if (GetNumConflictsRemaining() > 0)
	{
		return ConflictBrush;
	}
	if (HasSourceControlChangesToSync())
	{
		return NotAtHeadBrush;
	}

	return AtHeadBrush;
}

FReply SSourceControlControls::OnSourceControlSyncClicked()
{
	if (GetNumConflictsRemaining() > 0)
	{
		if (IConsoleObject* CObj = IConsoleManager::Get().FindConsoleObject(TEXT("UnrealRevisionControl.FocusConflictResolution")))
		{
			CObj->AsCommand()->Execute(/*Args=*/TArray<FString>(), /*InWorld=*/nullptr, *GLog);
		}
	}
	else if (HasSourceControlChangesToSync())
	{
		if (OnSyncLatestClicked.IsBound())
		{
			OnSyncLatestClicked.Execute();
		}
	}

	return FReply::Handled();
}

/** Check-in Status */

int SSourceControlControls::GetNumLocalChanges()
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

bool SSourceControlControls::IsSourceControlCheckInEnabled()
{
	if (!HasSourceControlChangesToCheckIn())
	{
		return false;
	}

	if (IsCheckInChangesEnabled.IsBound())
	{
		return IsCheckInChangesEnabled.Execute();
	}

	return false;
}

bool SSourceControlControls::HasSourceControlChangesToCheckIn()
{
	return (GetNumLocalChanges() > 0);
}

EVisibility SSourceControlControls::GetSourceControlCheckInStatusVisibility()
{
	if (!GIsEditor)
	{
		// Always visible in the Slate Viewer
		return EVisibility::Visible;
	}

	bool bVisibleSourceControlCheckInStatus = true;
	if (IsCheckInChangesVisible.IsBound())
	{
		bVisibleSourceControlCheckInStatus = IsCheckInChangesVisible.Execute();
	}

	bool bDisplaySourceControlCheckInStatus = false;
	GConfig->GetBool(TEXT("SourceControlSettings"), TEXT("DisplaySourceControlCheckInStatus"), bDisplaySourceControlCheckInStatus, GEditorIni);

	if (bVisibleSourceControlCheckInStatus && bDisplaySourceControlCheckInStatus)
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

EVisibility SSourceControlControls::GetSourceControlMiddleSeparatorVisibility() const
{
	EVisibility StatusVisibility = GetSourceControlCheckInStatusVisibility();
	if (StatusVisibility != EVisibility::Visible)
	{
		return StatusVisibility;
	}

	return IsMiddleSeparatorEnabled.Get(true) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SSourceControlControls::GetSourceControlCheckInStatusText()
{
	if (HasSourceControlChangesToCheckIn())
	{
		return LOCTEXT("CheckInButtonChangesText", "Check-in Changes");
	}

	return LOCTEXT("CheckInButtonNoChangesText", "No Changes");
}

FText SSourceControlControls::GetSourceControlCheckInStatusToolTipText()
{
	if (GetNumConflictsRemaining() > 0)
	{
		return LOCTEXT("CheckInButtonChangesTooltipTextConflict", "Some of your local changes conflict with the latest snapshot of the project. Click here to review these conflicts.");
	}
	if (HasSourceControlChangesToCheckIn())
	{
		return FText::Format(LOCTEXT("CheckInButtonChangesTooltipText", "Check-in {0} change(s) to this project"), GetNumLocalChanges());
	}

	return LOCTEXT("CheckInButtonNoChangesTooltipText", "No Changes to check in for this project");
}

const FSlateBrush* SSourceControlControls::GetSourceControlCheckInStatusIcon()
{
	static const FSlateBrush* ConflictBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.Conflicted");
	static const FSlateBrush* NoLocalChangesBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.NoLocalChanges");
	static const FSlateBrush* HasLocalChangesBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.HasLocalChanges");

	if (GetNumConflictsRemaining() > 0)
	{
		return ConflictBrush;
	}
	if (HasSourceControlChangesToCheckIn())
	{
		return HasLocalChangesBrush;
	}

	return NoLocalChangesBrush;
}

FReply SSourceControlControls::OnSourceControlCheckInChangesClicked()
{
	if (GetNumConflictsRemaining() > 0)
	{
		if (IConsoleObject* CObj = IConsoleManager::Get().FindConsoleObject(TEXT("UnrealRevisionControl.FocusConflictResolution")))
		{
			CObj->AsCommand()->Execute(/*Args=*/TArray<FString>(), /*InWorld=*/nullptr, *GLog);
		}
	}
	else if (HasSourceControlChangesToCheckIn())
	{
		if (OnCheckInChangesClicked.IsBound())
		{
			OnCheckInChangesClicked.Execute();
		}
	}

	return FReply::Handled();
}

/** Restore as Latest */

bool SSourceControlControls::IsSourceControlRestoreAsLatestEnabled()
{
	if (IsRestoreAsLatestEnabled.IsBound())
	{
		return IsRestoreAsLatestEnabled.Execute();
	}

	return false;
}

EVisibility SSourceControlControls::GetSourceControlRestoreAsLatestVisibility()
{
	if (IsRestoreAsLatestVisible.IsBound())
	{
		return IsRestoreAsLatestVisible.Execute() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}

FText SSourceControlControls::GetSourceControlRestoreAsLatestText()
{
	return LOCTEXT("RestoreAsLatestButtonText", "Restore as Latest");
}

FText SSourceControlControls::GetSourceControlRestoreAsLatestToolTipText()
{
	return LOCTEXT("RestoreAsLatestTooltipText", "Restore this snapshot to be the latest version of the project for all team members.");
}

const FSlateBrush* SSourceControlControls::GetSourceControlRestoreAsLatestStatusIcon()
{
	return FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.Promote");
}

FReply SSourceControlControls::OnSourceControlRestoreAsLatestClicked()
{
	if (OnRestoreAsLatestClicked.IsBound())
	{
		OnRestoreAsLatestClicked.Execute();
	}

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE
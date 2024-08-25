// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if SOURCE_CONTROL_WITH_SLATE

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "Delegates/IDelegateInstance.h"
#include "Framework/SlateDelegates.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_RetVal(bool, FIsVisible);
DECLARE_DELEGATE_RetVal(bool, FIsEnabled);

/** Widget for displaying Source Control Check in Changes and Sync Latest buttons */
class SOURCECONTROL_API SSourceControlControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSourceControlControls) {}
		SLATE_ATTRIBUTE(bool, IsEnabledMiddleSeparator)
		SLATE_ATTRIBUTE(bool, IsEnabledRightSeparator)
		SLATE_EVENT(FOnGetContent, OnGenerateKebabMenu)
	SLATE_END_ARGS()

public:
	/** Construct this widget */
	void Construct(const FArguments& InArgs);

public:
	/** Separators */
	EVisibility GetSourceControlMiddleSeparatorVisibility() const;
	EVisibility GetSourceControlRightSeparatorVisibility() const;

	/** Sync button */
	static bool IsAtLatestRevision();
	static bool IsSourceControlSyncEnabled();
	static bool HasSourceControlChangesToSync();
	static EVisibility GetSourceControlSyncStatusVisibility();
	static FText GetSourceControlSyncStatusText();
	static FText GetSourceControlSyncStatusToolTipText();
	static const FSlateBrush* GetSourceControlSyncStatusIcon();
	static FReply OnSourceControlSyncClicked();

	/** Check-in button */
	static int GetNumLocalChanges();
	static bool IsSourceControlCheckInEnabled();
	static bool HasSourceControlChangesToCheckIn();
	static EVisibility GetSourceControlCheckInStatusVisibility();
	static FText GetSourceControlCheckInStatusText();
	static FText GetSourceControlCheckInStatusToolTipText();
	static const FSlateBrush* GetSourceControlCheckInStatusIcon();
	static FReply OnSourceControlCheckInChangesClicked();

	/** Restore as latest button */
	static bool IsSourceControlRestoreAsLatestEnabled();
	static EVisibility GetSourceControlRestoreAsLatestVisibility();
	static FText GetSourceControlRestoreAsLatestText();
	static FText GetSourceControlRestoreAsLatestToolTipText();
	static const FSlateBrush* GetSourceControlRestoreAsLatestStatusIcon();
	static FReply OnSourceControlRestoreAsLatestClicked();

public:
	static int32 GetNumConflictsRemaining();

public:
	static void SetIsSyncLatestEnabled(const FIsEnabled& InSyncLatestEnabled) { IsSyncLatestEnabled = InSyncLatestEnabled; }
	static void SetIsCheckInChangesEnabled(const FIsEnabled& InCheckInChangesEnabled) { IsCheckInChangesEnabled = InCheckInChangesEnabled; }
	static void SetIsRestoreAsLatestEnabled(const FIsEnabled& InRestoreAsLatestEnabled) { IsRestoreAsLatestEnabled = InRestoreAsLatestEnabled; }

	static void SetIsSyncLatestVisible(const FIsVisible& InSyncLatestVisible) { IsSyncLatestVisible = InSyncLatestVisible; }
	static void SetIsCheckInChangesVisible(const FIsVisible& InCheckInChangesVisible) { IsCheckInChangesVisible = InCheckInChangesVisible; }
	static void SetIsRestoreAsLatestVisible(const FIsVisible& InRestoreAsLatestVisible) { IsRestoreAsLatestVisible = InRestoreAsLatestVisible; }

	static void SetOnSyncLatestClicked(const FOnClicked& InSyncLatestClicked) { OnSyncLatestClicked = InSyncLatestClicked; }
	static void SetOnCheckInChangesClicked(const FOnClicked& InCheckInChangesClicked) { OnCheckInChangesClicked = InCheckInChangesClicked; }
	static void SetOnRestoreAsLatestClicked(const FOnClicked& InRestoreAsLatestClicked) { OnRestoreAsLatestClicked = InRestoreAsLatestClicked; }

private:
	void OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider);
	void OnSourceControlStateChanged();
	
	void CheckSourceControlStatus();

private:
	
	TAttribute<bool> IsMiddleSeparatorEnabled;
	TAttribute<bool> IsRightSeparatorEnabled;

	FDelegateHandle SourceControlProviderChangedHandle;
	FDelegateHandle SourceControlStateChangedHandle;

	static int32 NumConflictsRemaining;

	static FIsEnabled IsSyncLatestEnabled;
	static FIsEnabled IsCheckInChangesEnabled;
	static FIsEnabled IsRestoreAsLatestEnabled;

	static FIsVisible IsSyncLatestVisible;
	static FIsVisible IsCheckInChangesVisible;
	static FIsVisible IsRestoreAsLatestVisible;

	static FOnClicked OnSyncLatestClicked;
	static FOnClicked OnCheckInChangesClicked;
	static FOnClicked OnRestoreAsLatestClicked;
};

#endif // SOURCE_CONTROL_WITH_SLATE
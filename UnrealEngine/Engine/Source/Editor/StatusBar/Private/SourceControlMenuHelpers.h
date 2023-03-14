// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "ISourceControlOperation.h"
#include "ISourceControlProvider.h"

class FUICommandList;
class SWidget;

class FSourceControlCommands : public TCommands<FSourceControlCommands>
{
public:
	FSourceControlCommands();

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

private:

	static void ConnectToSourceControl_Clicked();
	static bool ViewChangelists_CanExecute();
	static bool ViewChangelists_IsVisible();
	static bool SubmitContent_IsVisible();
	static void ViewChangelists_Clicked();
	static bool CheckOutModifiedFiles_CanExecute();
	static void CheckOutModifiedFiles_Clicked();

public:
	/**
	 * Source Control Commands
	 */
	TSharedPtr< FUICommandInfo > ConnectToSourceControl;
	TSharedPtr< FUICommandInfo > ChangeSourceControlSettings;
	TSharedPtr< FUICommandInfo > ViewChangelists;
	TSharedPtr< FUICommandInfo > SubmitContent;
	TSharedPtr< FUICommandInfo > CheckOutModifiedFiles;

	static TSharedRef<FUICommandList> ActionList;
};

class FSourceControlMenuHelpers
{
	friend class FSourceControlCommands;
public:
	/**
	 * Static: Access singleton instance
	 *
	 * @return	Reference to the singleton object
	 */
	static FSourceControlMenuHelpers& Get();

private:
	enum EQueryState
	{
		NotQueried,
		Querying,
		Queried,
	};


	static EQueryState QueryState;

public:
	static void CheckSourceControlStatus();
	static TSharedRef<SWidget> MakeSourceControlStatusWidget();

private:
	static void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
	static TSharedRef<SWidget> GenerateSourceControlMenuContent();
	static FText GetSourceControlStatusText();
	static FText GetSourceControlTooltip();
	static const FSlateBrush* GetSourceControlIcon();	

	/** Sync button */
	static bool IsAtLatestRevision();
	static EVisibility GetSourceControlSyncStatusVisibility();
	static FText GetSourceControlSyncStatusText();
	static FText GetSourceControlSyncStatusTooltipText();
	static const FSlateBrush* GetSourceControlSyncStatusIcon();
	static FReply OnSourceControlSyncClicked();

	void SyncProject();
	bool SaveDirtyPackages();
	TArray<FString> ListAllPackages();

	/** Loaded packages to reload after a Sync or Revert operation */
	TArray<UPackage*> PackagesToReload;

	/** Check-in button */
	static int GetNumLocalChanges();
	static EVisibility GetSourceControlCheckInStatusVisibility();
	static FText GetSourceControlCheckInStatusText();
	static FText GetSourceControlCheckInStatusTooltipText();
	static const FSlateBrush* GetSourceControlCheckInStatusIcon();
	static FReply OnSourceControlCheckInChangesClicked();
};


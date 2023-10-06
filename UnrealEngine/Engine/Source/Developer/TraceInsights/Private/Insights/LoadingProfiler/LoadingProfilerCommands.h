// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/Commands.h"

class FMenuBuilder;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Class that holds all profiler commands.
 */
class FLoadingProfilerCommands : public TCommands<FLoadingProfilerCommands>
{
public:
	/** Default constructor. */
	FLoadingProfilerCommands();

	/** Initialize commands. */
	virtual void RegisterCommands() override;

public:
	//////////////////////////////////////////////////
	// Global commands need to implement following method:
	//     void Map_<CommandName>_Global();
	// Custom commands needs to implement also the following method:
	//     const FUIAction <CommandName>_Custom(...) const;
	//////////////////////////////////////////////////

	/** Toggles visibility for the Timing view. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleTimingViewVisibility;

	/** Toggles visibility for the Event Aggregation tree view. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleEventAggregationTreeViewVisibility;

	/** Toggles visibility for the Object Type Aggregation tree view. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleObjectTypeAggregationTreeViewVisibility;

	/** Toggles visibility for the Package Details tree view. Global and custom command. */
	TSharedPtr<FUICommandInfo> TogglePackageDetailsTreeViewVisibility;

	/** Toggles visibility for the Export Details tree view. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleExportDetailsTreeViewVisibility;

	/** Toggles visibility for the Requests tree view. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleRequestsTreeViewVisibility;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Menu builder. Helper class for adding a customized menu entry using the global UI command info.
 */
class FLoadingProfilerMenuBuilder
{
public:
	/**
	 * Helper method for adding a customized menu entry using the global UI command info.
	 * FUICommandInfo cannot be executed with custom parameters, so we need to create a custom FUIAction,
	 * but sometime we have global and local version for the UI command, so reuse data from the global UI command info.
	 * Ex:
	 *     SessionInstance_ToggleCapture          - Global version will toggle capture process for all active session instances
	 *     SessionInstance_ToggleCapture_OneParam - Local version will toggle capture process only for the specified session instance
	 *
	 * @param MenuBuilder The menu to add items to
	 * @param FUICommandInfo A shared pointer to the UI command info
	 * @param UIAction Customized version of the UI command info stored in an UI action
	 */
	static void AddMenuEntry(FMenuBuilder& MenuBuilder, const TSharedPtr<FUICommandInfo>& UICommandInfo, const FUIAction& UIAction);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Class that provides helper functions for the commands to avoid cluttering profiler manager with many small functions.
 * Can't contain any variables. Directly operates on the profiler manager instance.
 */
class FLoadingProfilerActionManager
{
	friend class FLoadingProfilerManager;

private:
	/** Private constructor. */
	FLoadingProfilerActionManager(class FLoadingProfilerManager* Instance)
		: This(Instance)
	{}

	//////////////////////////////////////////////////
	// Toggle Commands

#define DECLARE_TOGGLE_COMMAND(CmdName)\
public:\
	void Map_##CmdName##_Global(); /**< Maps UI command info CmdName with the specified UI command list. */\
	const FUIAction CmdName##_Custom(); /**< UI action for CmdName command. */\
protected:\
	void CmdName##_Execute(); /**< Handles FExecuteAction for CmdName. */\
	bool CmdName##_CanExecute() const; /**< Handles FCanExecuteAction for CmdName. */\
	ECheckBoxState CmdName##_GetCheckState() const; /**< Handles FGetActionCheckState for CmdName. */

	DECLARE_TOGGLE_COMMAND(ToggleTimingViewVisibility)
	DECLARE_TOGGLE_COMMAND(ToggleEventAggregationTreeViewVisibility)
	DECLARE_TOGGLE_COMMAND(ToggleObjectTypeAggregationTreeViewVisibility)
	DECLARE_TOGGLE_COMMAND(TogglePackageDetailsTreeViewVisibility)
	DECLARE_TOGGLE_COMMAND(ToggleExportDetailsTreeViewVisibility)
	DECLARE_TOGGLE_COMMAND(ToggleRequestsTreeViewVisibility)
#undef DECLARE_TOGGLE_COMMAND

	//////////////////////////////////////////////////

protected:
	/** Reference to the global instance of the profiler manager. */
	class FLoadingProfilerManager* This;
};

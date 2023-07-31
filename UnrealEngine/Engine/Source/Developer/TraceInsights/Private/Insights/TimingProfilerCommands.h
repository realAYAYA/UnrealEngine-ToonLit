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
class FTimingProfilerCommands : public TCommands<FTimingProfilerCommands>
{
public:
	FTimingProfilerCommands();
	virtual ~FTimingProfilerCommands();
	virtual void RegisterCommands() override;

public:
	//////////////////////////////////////////////////
	// Global commands need to implement following method:
	//     void Map_<CommandName>_Global();
	// Custom commands needs to implement also the following method:
	//     const FUIAction <CommandName>_Custom(...) const;
	//////////////////////////////////////////////////

	/** Toggles visibility for the Frames Track. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleFramesTrackVisibility;

	/** Toggles visibility for the Timing View. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleTimingViewVisibility;

	/** Toggles visibility for the Timers View. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleTimersViewVisibility;

	/** Toggles visibility for the Callers Tree View. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleCallersTreeViewVisibility;

	/** Toggles visibility for the Callees Tree View. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleCalleesTreeViewVisibility;

	/** Toggles visibility for the Stats Counters View. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleStatsCountersViewVisibility;

	/** Toggles visibility for the Log View. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleLogViewVisibility;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimingViewCommands : public TCommands<FTimingViewCommands>
{
public:
	FTimingViewCommands();
	virtual ~FTimingViewCommands();
	virtual void RegisterCommands() override;

public:
	/** Toggles visibility of empty tracks. */
	TSharedPtr<FUICommandInfo> AutoHideEmptyTracks;

	/** Toggles "panning on screen edges". */
	TSharedPtr<FUICommandInfo> PanningOnScreenEdges;

	/** Toggles 'compact mode' for timing tracks. */
	TSharedPtr<FUICommandInfo> ToggleCompactMode;

	/** Toggles visibility for Main Graph track. */
	TSharedPtr<FUICommandInfo> ShowMainGraphTrack;

	/** Opens the Quick Find widget. */
	TSharedPtr<FUICommandInfo> QuickFind;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Menu builder. Helper class for adding a customized menu entry using the global UI command info.
 */
class FTimingProfilerMenuBuilder
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
class FTimingProfilerActionManager
{
	friend class FTimingProfilerManager;

private:
	/** Private constructor. */
	FTimingProfilerActionManager(class FTimingProfilerManager* Instance)
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

	DECLARE_TOGGLE_COMMAND(ToggleFramesTrackVisibility)
	DECLARE_TOGGLE_COMMAND(ToggleTimingViewVisibility)
	DECLARE_TOGGLE_COMMAND(ToggleTimersViewVisibility)
	DECLARE_TOGGLE_COMMAND(ToggleCallersTreeViewVisibility)
	DECLARE_TOGGLE_COMMAND(ToggleCalleesTreeViewVisibility)
	DECLARE_TOGGLE_COMMAND(ToggleStatsCountersViewVisibility)
	DECLARE_TOGGLE_COMMAND(ToggleLogViewVisibility)
#undef DECLARE_TOGGLE_COMMAND

	//////////////////////////////////////////////////
	// OpenSettings

public:
	void Map_OpenSettings_Global(); /**< Maps UI command info OpenSettings with the specified UI command list. */
	const FUIAction OpenSettings_Custom() const; /**< UI action for OpenSettings command. */
protected:
	void OpenSettings_Execute(); /**< Handles FExecuteAction for OpenSettings. */
	bool OpenSettings_CanExecute() const; /**< Handles FCanExecuteAction for OpenSettings. */

	//////////////////////////////////////////////////

protected:
	/** Reference to the global instance of the profiler manager. */
	class FTimingProfilerManager* This;
};

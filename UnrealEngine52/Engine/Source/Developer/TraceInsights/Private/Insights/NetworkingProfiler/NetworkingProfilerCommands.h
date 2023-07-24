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
class FNetworkingProfilerCommands : public TCommands<FNetworkingProfilerCommands>
{
public:
	/** Default constructor. */
	FNetworkingProfilerCommands();

	/** Initialize commands. */
	virtual void RegisterCommands() override;

public:
	//////////////////////////////////////////////////
	// Global commands need to implement following method:
	//     void Map_<CommandName>_Global();
	// Custom commands needs to implement also the following method:
	//     const FUIAction <CommandName>_Custom(...) const;
	//////////////////////////////////////////////////

	/** Toggles visibility for the Packet view. Per profiler window, custom command. */
	TSharedPtr<FUICommandInfo> TogglePacketViewVisibility;

	/** Toggles visibility for the Packet Content view. Per profiler window, custom command. */
	TSharedPtr<FUICommandInfo> TogglePacketContentViewVisibility;

	/** Toggles visibility for the Net Stats view. Per profiler window, custom command. */
	TSharedPtr<FUICommandInfo> ToggleNetStatsViewVisibility;

	/** Toggles visibility for the Net Stats Counters view. Per profiler window, custom command. */
	TSharedPtr<FUICommandInfo> ToggleNetStatsCountersViewVisibility;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Menu builder. Helper class for adding a customized menu entry using the global UI command info.
 */
class FNetworkingProfilerMenuBuilder
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
class FNetworkingProfilerActionManager
{
	friend class FNetworkingProfilerManager;

private:
	/** Private constructor. */
	FNetworkingProfilerActionManager(class FNetworkingProfilerManager* Instance)
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

	//DECLARE_TOGGLE_COMMAND(ToggleAAAViewVisibility)
#undef DECLARE_TOGGLE_COMMAND

	//////////////////////////////////////////////////

protected:
	/** Reference to the global instance of the profiler manager. */
	class FNetworkingProfilerManager* This;
};

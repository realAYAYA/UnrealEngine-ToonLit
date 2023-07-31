// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/Commands.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Class that holds all profiler commands.
 */
class FInsightsCommands : public TCommands<FInsightsCommands>
{
public:
	/** Default constructor. */
	FInsightsCommands();

	/** Initialize commands. */
	virtual void RegisterCommands() override;

public:
	//////////////////////////////////////////////////
	// Global commands need to implement following method:
	//     void Map_<CommandName>_Global();
	// Custom commands needs to implement also the following method:
	//     const FUIAction <CommandName>_Custom(...) const;
	//////////////////////////////////////////////////

	/** Load profiler data from a trace file. Global version. */
	TSharedPtr<FUICommandInfo> InsightsManager_Load;

	/** Toggles the debug info. Global and custom command. */
	TSharedPtr<FUICommandInfo> ToggleDebugInfo;

	/** Open settings for the profiler manager. */
	TSharedPtr<FUICommandInfo> OpenSettings;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Class that provides helper functions for the commands to avoid cluttering manager with many small functions.
 * Can't contain any variables. Directly operates on the manager instance.
 */
class FInsightsActionManager
{
	friend class FInsightsManager;

private:
	/** Private constructor. */
	FInsightsActionManager(class FInsightsManager* Instance)
		: This(Instance)
	{}

	//////////////////////////////////////////////////
	// InsightsManager_Load

public:
	void Map_InsightsManager_Load(); /**< Maps UI command info InsightsManager_Load with the specified UI command list. */
protected:
	void InsightsManager_Load_Execute(); /**< Handles FExecuteAction for InsightsManager_Load. */
	bool InsightsManager_Load_CanExecute() const; /**< Handles FCanExecuteAction for InsightsManager_Load. */

	//////////////////////////////////////////////////
	// ToggleDebugInfo

public:
	void Map_ToggleDebugInfo_Global(); /**< Maps UI command info ToggleDebugInfo with the specified UI command list. */
	const FUIAction ToggleDebugInfo_Custom(); /**< UI action for ToggleDebugInfo command. */
protected:
	void ToggleDebugInfo_Execute(); /**< Handles FExecuteAction for ToggleDebugInfo. */
	bool ToggleDebugInfo_CanExecute() const; /**< Handles FCanExecuteAction for ToggleDebugInfo. */
	ECheckBoxState ToggleDebugInfo_GetCheckState() const; /**< Handles FGetActionCheckState for ToggleDebugInfo. */

	//////////////////////////////////////////////////
	// OpenSettings

public:
	void Map_OpenSettings_Global(); /**< Maps UI command info OpenSettings with the specified UI command list. */
	const FUIAction OpenSettings_Custom(); /**< UI action for OpenSettings command. */
protected:
	void OpenSettings_Execute(); /**< Handles FExecuteAction for OpenSettings. */
	bool OpenSettings_CanExecute() const; /**< Handles FCanExecuteAction for OpenSettings. */

	//////////////////////////////////////////////////

protected:
	/** Reference to the global instance of the Insights manager. */
	class FInsightsManager* This;
};

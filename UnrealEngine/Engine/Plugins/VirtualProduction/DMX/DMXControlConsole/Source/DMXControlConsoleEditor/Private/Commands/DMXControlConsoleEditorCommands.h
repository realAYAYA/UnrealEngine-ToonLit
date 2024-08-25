// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"


/** Defines commands for the DMX Control Console. */
class FDMXControlConsoleEditorCommands
	: public TCommands<FDMXControlConsoleEditorCommands>
{
public:
	/** Constructor */
	FDMXControlConsoleEditorCommands();

	/** Registers commands for DMX Control Console */
	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> OpenControlConsole;

	TSharedPtr<FUICommandInfo> PlayDMX;
	TSharedPtr<FUICommandInfo> PauseDMX;
	TSharedPtr<FUICommandInfo> ResumeDMX;
	TSharedPtr<FUICommandInfo> StopDMX;
	TSharedPtr<FUICommandInfo> TogglePlayPauseDMX;
	TSharedPtr<FUICommandInfo> TogglePlayStopDMX;

	TSharedPtr<FUICommandInfo> EditorStopSendsDefaultValues;
	TSharedPtr<FUICommandInfo> EditorStopSendsZeroValues;
	TSharedPtr<FUICommandInfo> EditorStopKeepsLastValues;

	TSharedPtr<FUICommandInfo> RemoveElements;
	TSharedPtr<FUICommandInfo> SelectAll;
	TSharedPtr<FUICommandInfo> ClearAll;
	TSharedPtr<FUICommandInfo> ResetToDefault;
	TSharedPtr<FUICommandInfo> ResetToZero;

	TSharedPtr<FUICommandInfo> Enable;
	TSharedPtr<FUICommandInfo> EnableAll;
	TSharedPtr<FUICommandInfo> Disable;
	TSharedPtr<FUICommandInfo> DisableAll;

	TSharedPtr<FUICommandInfo> AddPatchRight;
	TSharedPtr<FUICommandInfo> AddPatchNextRow;
	TSharedPtr<FUICommandInfo> AddPatchToSelection;
	TSharedPtr<FUICommandInfo> GroupPatchRight;
	TSharedPtr<FUICommandInfo> GroupPatchNextRow;
	TSharedPtr<FUICommandInfo> AddEmptyGroupRight;
	TSharedPtr<FUICommandInfo> AddEmptyGroupNextRow;
};

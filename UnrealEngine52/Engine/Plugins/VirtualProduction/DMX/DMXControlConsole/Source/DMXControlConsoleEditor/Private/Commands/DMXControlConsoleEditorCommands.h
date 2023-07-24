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

	TSharedPtr<FUICommandInfo> CreateNewConsole;
	TSharedPtr<FUICommandInfo> SaveConsole;
	TSharedPtr<FUICommandInfo> SaveConsoleAs;
	TSharedPtr<FUICommandInfo> SendDMX;
	TSharedPtr<FUICommandInfo> StopDMX;
	TSharedPtr<FUICommandInfo> ClearAll;
};

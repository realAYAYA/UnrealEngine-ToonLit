// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class DMXEDITOR_API FDMXConflictMonitorCommands
	: public TCommands<FDMXConflictMonitorCommands>
{
public:
	FDMXConflictMonitorCommands();

	//~ Begin TCommands implementation
	virtual void RegisterCommands() override;
	//~ End TCommands implementation

	TSharedPtr<FUICommandInfo> StartScan;
	TSharedPtr<FUICommandInfo> PauseScan;
	TSharedPtr<FUICommandInfo> ResumeScan;
	TSharedPtr<FUICommandInfo> StopScan;

	TSharedPtr<FUICommandInfo> ToggleAutoPause;
	TSharedPtr<FUICommandInfo> TogglePrintToLog;
	TSharedPtr<FUICommandInfo> ToggleRunWhenOpened;
};

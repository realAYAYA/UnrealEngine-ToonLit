// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FPCGEditorCommands : public TCommands<FPCGEditorCommands>
{
public:
	FPCGEditorCommands();

	// ~Begin TCommands<> interface
	virtual void RegisterCommands() override;
	// ~End TCommands<> interface

	TSharedPtr<FUICommandInfo> CollapseNodes;
	TSharedPtr<FUICommandInfo> ExportNodes;
	TSharedPtr<FUICommandInfo> ConvertToStandaloneNodes;
	TSharedPtr<FUICommandInfo> Find;
	TSharedPtr<FUICommandInfo> PauseAutoRegeneration;
	TSharedPtr<FUICommandInfo> ForceGraphRegeneration;
	TSharedPtr<FUICommandInfo> RunDeterminismNodeTest;
	TSharedPtr<FUICommandInfo> RunDeterminismGraphTest;
	TSharedPtr<FUICommandInfo> EditGraphSettings;
	TSharedPtr<FUICommandInfo> CancelExecution;
	TSharedPtr<FUICommandInfo> ToggleEnabled;
	TSharedPtr<FUICommandInfo> ToggleDebug;
	TSharedPtr<FUICommandInfo> DebugOnlySelected;
	TSharedPtr<FUICommandInfo> DisableDebugOnAllNodes;
	TSharedPtr<FUICommandInfo> ToggleInspect;
};

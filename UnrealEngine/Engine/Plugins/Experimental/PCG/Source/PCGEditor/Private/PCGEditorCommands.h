// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FPCGEditorCommands : public TCommands<FPCGEditorCommands>
{
public:
	FPCGEditorCommands();

	// ~Begin TCommands<> interface
	virtual void RegisterCommands() override;
	// ~End TCommands<> interface

	TSharedPtr<FUICommandInfo> CollapseNodes;
	TSharedPtr<FUICommandInfo> Find;
	TSharedPtr<FUICommandInfo> PauseAutoRegeneration;
	TSharedPtr<FUICommandInfo> ForceGraphRegeneration;
	TSharedPtr<FUICommandInfo> RunDeterminismNodeTest;
	TSharedPtr<FUICommandInfo> RunDeterminismGraphTest;
	TSharedPtr<FUICommandInfo> EditClassDefaults;
	TSharedPtr<FUICommandInfo> StartInspectNode;
	TSharedPtr<FUICommandInfo> StopInspectNode;
	TSharedPtr<FUICommandInfo> ExecutionModeEnabled;
	TSharedPtr<FUICommandInfo> ExecutionModeDebug;
	TSharedPtr<FUICommandInfo> ExecutionModeIsolated;
	TSharedPtr<FUICommandInfo> ExecutionModeDisabled;
};

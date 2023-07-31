// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FOptimusEditorGraphCommands :
	public TCommands<FOptimusEditorGraphCommands>
{
public:
	FOptimusEditorGraphCommands();
	
	// TCommands<> overrides
	void RegisterCommands() override;
	
	TSharedPtr<FUICommandInfo> ConvertToKernelFunction;
	TSharedPtr<FUICommandInfo> ConvertFromKernelFunction;

	TSharedPtr<FUICommandInfo> CollapseNodesToFunction;
	TSharedPtr<FUICommandInfo> CollapseNodesToSubGraph;
	TSharedPtr<FUICommandInfo> ExpandCollapsedNode;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"


/// Optimus editor command set.
class FOptimusEditorCommands :
	public TCommands<FOptimusEditorCommands>
{
public:
	FOptimusEditorCommands();

	// TCommands<> overrides
	void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> Compile;
};
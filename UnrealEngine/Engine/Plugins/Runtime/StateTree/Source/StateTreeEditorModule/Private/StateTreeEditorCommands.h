// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"


/**
 * StateTree editor command set.
 */
class FStateTreeEditorCommands : public TCommands<FStateTreeEditorCommands>
{
public:
	FStateTreeEditorCommands();

	// TCommands<> overrides
	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> Compile;
};
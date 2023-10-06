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
	TSharedPtr<FUICommandInfo> SaveOnCompile_Never;
	TSharedPtr<FUICommandInfo> SaveOnCompile_SuccessOnly;
	TSharedPtr<FUICommandInfo> SaveOnCompile_Always;

	TSharedPtr<FUICommandInfo> AddSiblingState;
	TSharedPtr<FUICommandInfo> AddChildState;
	TSharedPtr<FUICommandInfo> RenameState;
	TSharedPtr<FUICommandInfo> CutStates;
	TSharedPtr<FUICommandInfo> CopyStates;
	TSharedPtr<FUICommandInfo> PasteStatesAsSiblings;
	TSharedPtr<FUICommandInfo> PasteStatesAsChildren;
	TSharedPtr<FUICommandInfo> DuplicateStates;
	TSharedPtr<FUICommandInfo> DeleteStates;
	TSharedPtr<FUICommandInfo> EnableStates;
};
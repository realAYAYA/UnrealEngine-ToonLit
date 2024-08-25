// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FAvaTransitionEditorCommands : public TCommands<FAvaTransitionEditorCommands>
{
public:
	FAvaTransitionEditorCommands();

	//~ Begin TCommands
	virtual void RegisterCommands() override;
	//~ End TCommands

	TSharedPtr<FUICommandInfo> AddComment;
	TSharedPtr<FUICommandInfo> RemoveComment;

	TSharedPtr<FUICommandInfo> AddSiblingState;
	TSharedPtr<FUICommandInfo> AddChildState;
	TSharedPtr<FUICommandInfo> EnableStates;

	TSharedPtr<FUICommandInfo> ImportTransitionTree;
	TSharedPtr<FUICommandInfo> ExportTransitionTree;

	TSharedPtr<FUICommandInfo> Compile;
	TSharedPtr<FUICommandInfo> SaveOnCompile_Never;
	TSharedPtr<FUICommandInfo> SaveOnCompile_SuccessOnly;
	TSharedPtr<FUICommandInfo> SaveOnCompile_Always;

#if WITH_STATETREE_DEBUGGER
	TSharedPtr<FUICommandInfo> ToggleDebug;
#endif
};

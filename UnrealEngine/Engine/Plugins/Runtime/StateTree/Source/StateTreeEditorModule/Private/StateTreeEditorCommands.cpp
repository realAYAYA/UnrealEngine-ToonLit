// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorCommands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

FStateTreeEditorCommands::FStateTreeEditorCommands() 
	: TCommands<FStateTreeEditorCommands>(
		"StateTreeEditor", // Context name for fast lookup
		LOCTEXT("StateTreeEditor", "StateTree Editor"), // Localized context name for displaying
		NAME_None,
		FAppStyle::GetAppStyleSetName()
	)
{
}

void FStateTreeEditorCommands::RegisterCommands()
{
	UI_COMMAND(Compile, "Compile", "Compile the current StateTree.", EUserInterfaceActionType::Button, FInputChord(EKeys::F7));
}

#undef LOCTEXT_NAMESPACE

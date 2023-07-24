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

	UI_COMMAND(SaveOnCompile_Never, "Never", "Sets the save-on-compile option to 'Never', meaning that your StateTree will not be saved when they are compiled", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SaveOnCompile_SuccessOnly, "On Success Only", "Sets the save-on-compile option to 'Success Only', meaning that your StateTree will be saved whenever they are successfully compiled", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SaveOnCompile_Always, "Always", "Sets the save-on-compile option to 'Always', meaning that your StateTree will be saved whenever they are compiled (even if there were errors)", EUserInterfaceActionType::RadioButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE

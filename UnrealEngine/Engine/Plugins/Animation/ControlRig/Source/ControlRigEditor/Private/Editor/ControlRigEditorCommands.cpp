// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditorCommands.h"

#define LOCTEXT_NAMESPACE "ControlRigEditorCommands"

void FControlRigEditorCommands::RegisterCommands()
{
	UI_COMMAND(ConstructionEvent, "Construction Event", "Enable the construction mode for the rig", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ForwardsSolveEvent, "Forwards Solve", "Run the forwards solve graph", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(BackwardsSolveEvent, "Backwards Solve", "Run the backwards solve graph", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(BackwardsAndForwardsSolveEvent, "Backwards and Forwards", "Run backwards solve followed by forwards solve", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

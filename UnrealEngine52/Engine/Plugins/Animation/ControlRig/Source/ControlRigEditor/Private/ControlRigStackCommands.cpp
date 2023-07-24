// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigStackCommands.h"

#define LOCTEXT_NAMESPACE "ControlRigStackCommands"

void FControlRigStackCommands::RegisterCommands()
{
	UI_COMMAND(FocusOnSelection, "Focus On Selection", "Finds the selected operator's node in the graph.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GoToInstruction, "Go To Instruction", "Looks for a specific instruction by index and brings it into focus.", EUserInterfaceActionType::Button, FInputChord(EKeys::G, EModifierKey::Control));
}

#undef LOCTEXT_NAMESPACE

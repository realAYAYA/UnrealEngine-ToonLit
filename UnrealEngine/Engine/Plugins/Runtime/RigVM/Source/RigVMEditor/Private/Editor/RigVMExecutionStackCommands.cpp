// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigVMExecutionStackCommands.h"

#define LOCTEXT_NAMESPACE "RigVMExecutionStackCommands"

void FRigVMExecutionStackCommands::RegisterCommands()
{
	UI_COMMAND(FocusOnSelection, "Focus On Selection", "Finds the selected operator's node in the graph.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GoToInstruction, "Go To Instruction", "Looks for a specific instruction by index and brings it into focus.", EUserInterfaceActionType::Button, FInputChord(EKeys::G, EModifierKey::Control));
	UI_COMMAND(SelectTargetInstructions, "Select Target Instruction(s)", "Looks up the target instructions and selects them.", EUserInterfaceActionType::Button, FInputChord(EKeys::T, EModifierKey::Control));
}

#undef LOCTEXT_NAMESPACE

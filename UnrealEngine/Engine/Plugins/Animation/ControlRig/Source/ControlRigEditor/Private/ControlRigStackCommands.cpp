// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigStackCommands.h"

#define LOCTEXT_NAMESPACE "ControlRigStackCommands"

void FControlRigStackCommands::RegisterCommands()
{
	UI_COMMAND(FocusOnSelection, "Focus On Selection", "Finds the selected operator's node in the graph.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

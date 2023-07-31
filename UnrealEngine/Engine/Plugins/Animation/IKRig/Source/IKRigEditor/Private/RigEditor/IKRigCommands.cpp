// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigCommands.h"

#define LOCTEXT_NAMESPACE "IKRigCommands"

void FIKRigCommands::RegisterCommands()
{
	UI_COMMAND(Reset, "Reset", "Reset state of the rig and goals to initial pose.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

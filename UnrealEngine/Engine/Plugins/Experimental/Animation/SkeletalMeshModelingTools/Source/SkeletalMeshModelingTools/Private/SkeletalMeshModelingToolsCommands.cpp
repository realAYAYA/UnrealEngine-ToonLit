// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshModelingToolsCommands.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshModelingToolsCommands"


void FSkeletalMeshModelingToolsCommands::RegisterCommands()
{
	UI_COMMAND(ToggleModelingToolsMode, "Enable Modeling Tools", "Toggles modeling tools on or off.", EUserInterfaceActionType::ToggleButton, FInputChord());
}

const FSkeletalMeshModelingToolsCommands& FSkeletalMeshModelingToolsCommands::Get()
{
	return TCommands<FSkeletalMeshModelingToolsCommands>::Get();
}

#undef LOCTEXT_NAMESPACE

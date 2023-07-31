// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveFunctionCollapseEditorCommands.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/InputChord.h"

#define LOCTEXT_NAMESPACE "FWaveFunctionCollapseModule"

void FWaveFunctionCollapseEditorCommands::RegisterCommands()
{
	UI_COMMAND(WaveFunctionCollapseWidget, "WaveFunctionCollapse", "Wave Function Collapse Tool", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
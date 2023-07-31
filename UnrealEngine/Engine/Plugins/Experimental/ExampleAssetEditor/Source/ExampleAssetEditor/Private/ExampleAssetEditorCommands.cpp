// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExampleAssetEditorCommands.h"

#include "Framework/Commands/InputChord.h"

#define LOCTEXT_NAMESPACE "FExampleAssetEditorModule"

void FExampleAssetEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "Example Asset Editor", "Bring up an example asset editor window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

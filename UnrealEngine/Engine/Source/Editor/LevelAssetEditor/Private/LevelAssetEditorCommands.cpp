// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelAssetEditorCommands.h"

#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"

#define LOCTEXT_NAMESPACE "FLevelAssetEditorModule"

void FLevelAssetEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "Level Asset Editor", "Bring up an experiment level editor window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

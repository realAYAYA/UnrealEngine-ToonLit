// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusToolCommands.h"
#include "Framework/Docking/TabManager.h"

#define LOCTEXT_NAMESPACE "FOculusEditorModule"

void FOculusToolCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "Oculus Tool", "Show Oculus Tool Window", EUserInterfaceActionType::Button, FInputChord());
}

void FOculusToolCommands::ShowOculusTool()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FOculusEditorModule::OculusPerfTabName);
}

#undef LOCTEXT_NAMESPACE

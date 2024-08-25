// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDCommands.h"

#include "ChaosVDStyle.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FChaosVDCommands::FChaosVDCommands() : TCommands<FChaosVDCommands>(TEXT("ChaosVDEditor"), NSLOCTEXT("ChaosVisualDebugger", "ChaosVisualDebuggerEditor", "Chaos Visual Debugger Editor"), NAME_None, FChaosVDStyle::GetStyleSetName())
{
}

void FChaosVDCommands::RegisterCommands()
{
	UI_COMMAND(TrackUntrackSelectedObject, "Start or Stop tracking the selected object", "Start or Stop tracking the selected object", EUserInterfaceActionType::Button, FInputChord(EKeys::F8));
}

#undef LOCTEXT_NAMESPACE

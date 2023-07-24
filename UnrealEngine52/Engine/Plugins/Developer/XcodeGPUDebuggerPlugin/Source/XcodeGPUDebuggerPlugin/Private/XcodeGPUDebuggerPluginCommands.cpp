// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "XcodeGPUDebuggerPluginCommands.h"

#define LOCTEXT_NAMESPACE "XcodeGPUDebuggerPlugin"

void FXcodeGPUDebuggerPluginCommands::RegisterCommands()
{
	UI_COMMAND(CaptureFrameCommand, "Capture Frame", "Captures the next frame and launches Xcode (Shift+E)", EUserInterfaceActionType::Button, FInputChord(EKeys::E, EModifierKey::Shift));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR

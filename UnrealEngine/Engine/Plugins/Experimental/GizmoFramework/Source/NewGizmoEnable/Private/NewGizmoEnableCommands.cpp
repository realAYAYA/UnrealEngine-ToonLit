// Copyright Epic Games, Inc. All Rights Reserved.

#include "NewGizmoEnableCommands.h"

#include "Framework/Commands/InputChord.h"

#define LOCTEXT_NAMESPACE "FNewGizmoEnableModule"

void FNewGizmoEnableCommands::RegisterCommands()
{
	UI_COMMAND(ToggleNewGizmos, "ToggleNewGizmos", "Toggle New Gizmos", EUserInterfaceActionType::Check , FInputChord(EModifierKey::Control, EKeys::T));
}

#undef LOCTEXT_NAMESPACE

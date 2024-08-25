// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEffectorsEditorCommands.h"
#include "AvaEffectorsEditorStyle.h"

#define LOCTEXT_NAMESPACE "AvaEffectorsEditorCommands"

FAvaEffectorsEditorCommands::FAvaEffectorsEditorCommands()
	: TCommands<FAvaEffectorsEditorCommands>(
		TEXT("AvaEffectorsEditor")
		, LOCTEXT("MotionDesignEffectorsEditor", "Motion Design Effects Editor")
		, NAME_None
		, FAvaEffectorsEditorStyle::Get().GetStyleSetName()
	)
{
}

void FAvaEffectorsEditorCommands::RegisterCommands()
{
	UI_COMMAND(Tool_Actor_Effector
		, "Effector Actor"
		, "Create an Effector Actor in the viewport."
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());

	UI_COMMAND(Tool_Actor_Cloner
		, "Cloner Actor"
		, "Create a Cloner Actor in the viewport."
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());
}

#undef LOCTEXT_NAMESPACE

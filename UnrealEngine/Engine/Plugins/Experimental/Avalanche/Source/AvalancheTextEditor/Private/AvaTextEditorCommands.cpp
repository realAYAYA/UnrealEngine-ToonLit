// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTextEditorCommands.h"
#include "AvaTextEditorStyle.h"

#define LOCTEXT_NAMESPACE "AvaTextEditorCommands"

FAvaTextEditorCommands::FAvaTextEditorCommands()
	: TCommands<FAvaTextEditorCommands>(
		TEXT("AvaTextEditor")
		, LOCTEXT("MotionDesignTextEditor", "Motion Design Text Editor")
		, NAME_None
		, FAvaTextEditorStyle::Get().GetStyleSetName()
	)
{
}

void FAvaTextEditorCommands::RegisterCommands()
{
	UI_COMMAND(Tool_Actor_Text3D
		, "Text Actor"
		, "Create a Text Actor in the viewport."
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());
}

#undef LOCTEXT_NAMESPACE

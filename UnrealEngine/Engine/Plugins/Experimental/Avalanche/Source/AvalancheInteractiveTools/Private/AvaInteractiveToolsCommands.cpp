// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaInteractiveToolsCommands.h"
#include "AvaInteractiveToolsStyle.h"

#define LOCTEXT_NAMESPACE "AvaInteractiveToolsCommands"

FAvaInteractiveToolsCommands::FAvaInteractiveToolsCommands()
	: TCommands<FAvaInteractiveToolsCommands>(
		TEXT("AvaInteractiveTools")
		, LOCTEXT("MotionDesignInteractiveTools", "Motion Design Interactive Tools")
		, NAME_None
		, FAvaInteractiveToolsStyle::Get().GetStyleSetName()
	)
{
}

void FAvaInteractiveToolsCommands::RegisterCommands()
{
	RegisterCategoryCommands();
	Register2DCommands();
	Register3DCommands();
	RegisterActorCommands();
	RegisterLayoutCommands();
}

void FAvaInteractiveToolsCommands::RegisterCategoryCommands()
{
	UI_COMMAND(Category_2D
		, "2D Shapes"
		, "Tools for creating 2D shapes in the viewport."
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());

	UI_COMMAND(Category_3D
		, "3D Shapes"
		, "Tools for creating 3D shapes in the viewport."
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());

	UI_COMMAND(Category_Actor
		, "Actors"
		, "Tools for creating Actors in the viewport."
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());

	UI_COMMAND(Category_Layout
		, "Layout Helpers"
		, "Tools for creating Layout Helpers in the viewport."
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());
}

void FAvaInteractiveToolsCommands::Register2DCommands()
{
}

void FAvaInteractiveToolsCommands::Register3DCommands()
{
}

void FAvaInteractiveToolsCommands::RegisterActorCommands()
{
	UI_COMMAND(Tool_Actor_Null
		, "Null Actor"
		, "Create a Null Actor in the viewport."
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());

	UI_COMMAND(Tool_Actor_Spline
		, "Spline Actor"
		, "Create a Spline Actor in the viewport."
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());
}

void FAvaInteractiveToolsCommands::RegisterLayoutCommands()
{
}

#undef LOCTEXT_NAMESPACE

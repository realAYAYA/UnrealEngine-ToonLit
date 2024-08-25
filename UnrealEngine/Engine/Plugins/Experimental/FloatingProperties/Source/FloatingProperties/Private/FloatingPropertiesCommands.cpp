// Copyright Epic Games, Inc. All Rights Reserved.

#include "FloatingPropertiesCommands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FloatingPropertiesCommands"

FFloatingPropertiesCommands::FFloatingPropertiesCommands()
	: TCommands<FFloatingPropertiesCommands>(TEXT("FloatingProperties")
		, LOCTEXT("FloatingProperties", "FloatingProperties")
		, NAME_None
		, FAppStyle::GetAppStyleSetName()
	)
{
}

void FFloatingPropertiesCommands::RegisterCommands()
{
	UI_COMMAND(ToggleEnabled
		, "Floating Properties"
		, "Toggle floating properties on and off."
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());	
}

#undef LOCTEXT_NAMESPACE

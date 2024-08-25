// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaFontSelectorCommands.h"

#define LOCTEXT_NAMESPACE "AvaFontFilteringCommands"

void FAvaFontSelectorCommands::RegisterCommands()
{
	UI_COMMAND(ShowMonospacedFonts
		, "Monospaced"
		, "Filter monospaced fonts"
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());

	UI_COMMAND(ShowBoldFonts
		, "Bold"
		, "Filter fonts with bold typefaces available"
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());

	UI_COMMAND(ShowItalicFonts
		, "Italic"
		, "Filter fonts with italic typefaces available"
		, EUserInterfaceActionType::ToggleButton
		, FInputChord());
}

#undef LOCTEXT_NAMESPACE

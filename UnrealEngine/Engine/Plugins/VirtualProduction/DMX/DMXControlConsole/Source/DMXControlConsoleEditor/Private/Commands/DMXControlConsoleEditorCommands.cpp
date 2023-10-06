// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorCommands.h"

#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorCommands"

FDMXControlConsoleEditorCommands::FDMXControlConsoleEditorCommands()
	: TCommands<FDMXControlConsoleEditorCommands>
	(
		TEXT("DMXControlConsoleEditor"),
		LOCTEXT("DMXControlConsoleEditor", "DMX DMX Control Console"),
		NAME_None,
		FAppStyle::GetAppStyleSetName()
	)
{}

void FDMXControlConsoleEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenControlConsole, "Open Control Console", "Opens the DMX Control Console", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(CreateNewConsole, "New Console", "Creates a new Control Console", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::N));
	UI_COMMAND(SaveConsole, "Save Console", "Saves the Control Console", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::S));
	UI_COMMAND(SaveConsoleAs, "Save Console As...", "Saves the Console as a new asset", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::S));
	UI_COMMAND(ToggleSendDMX, "Toggle Send DMX", "Starts/Stops sending DMX.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::N));
	UI_COMMAND(RemoveElements, "Remove Elements", "Removes selected elements from current Control Console", EUserInterfaceActionType::None, FInputChord(EKeys::Delete));
	UI_COMMAND(SelectAll, "Select All", "Selects all visible Elements in Control Console", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::A));
	UI_COMMAND(ClearAll, "Clear All", "Clears the entire console", EUserInterfaceActionType::Button, FInputChord());
	
	UI_COMMAND(Mute, "Mute", "Mutes selected Fader Groups.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MuteAll, "Mute All", "Mutes all Fader Groups.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Unmute, "Unmute", "Unmutes selected Fader Groups.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(UnmuteAll, "Unmute All", "Mutes all Fader Groups.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(AddPatchNext, "Add Patches to the right", "Adds selected Fixture Patches to the right on the same row.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddPatchNextRow, "Add Patches on new row", "Adds selected Fixture Patches to next row.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddPatchToSelection, "Set Patch", "Uses the selected Fixture Patch in the selected Fader Group. Clears the previous patch.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE 

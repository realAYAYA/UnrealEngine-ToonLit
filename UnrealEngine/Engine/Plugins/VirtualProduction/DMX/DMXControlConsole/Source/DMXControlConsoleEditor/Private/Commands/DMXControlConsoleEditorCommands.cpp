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

	UI_COMMAND(PlayDMX, "Play DMX", "Plays DMX", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PauseDMX, "Pause DMX", "Pauses playing DMX.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResumeDMX, "Resume DMX", "Resumes playing DMX after being paused.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopDMX, "Stop DMX", "Stops playing DMX.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(TogglePlayPauseDMX, "Toggle Play/Pause DMX", "Toggles between playing and pausing DMX", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::SpaceBar));
	UI_COMMAND(TogglePlayStopDMX, "Toggle Play/Stop DMX", "Toggles between playing and stopping DMX", EUserInterfaceActionType::Button, FInputChord(EKeys::SpaceBar));

	UI_COMMAND(EditorStopSendsZeroValues, "Stop Sends Zero Values", "When stop is clicked in the editor, zero values are sent to all patches and raw DMX channels", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(EditorStopSendsDefaultValues, "Stop Sends Default Values", "When stop is clicked in the editor, default values are sent to  all patches and raw DMX channels", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(EditorStopKeepsLastValues, "Stop Keeps Last Values", "When stop is clicked in the editor, the control console leaves the last sent values untouched", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(RemoveElements, "Remove Elements", "Removes the selected elements from the current Control Console", EUserInterfaceActionType::None, FInputChord(EKeys::Delete));
	UI_COMMAND(SelectAll, "Select All", "Selects all the visible elements in the Control Console", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::A));
	UI_COMMAND(ClearAll, "Clear All", "Clears the entire console", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResetToDefault, "Reset to Default", "Resets all the elements in the Control Console to their default values", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResetToZero, "Reset to Zero", "Resets all the elements in the Control Console to zero", EUserInterfaceActionType::Button, FInputChord());
	
	UI_COMMAND(Enable, "Enable", "Enables the selected Fader Groups.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(EnableAll, "Enable All", "Enables all the Fader Groups.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Disable, "Disable", "Disables the selected Fader Groups.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DisableAll, "Disable All", "Disables all the Fader Groups.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(AddPatchRight, "Add Patches to the right", "Adds the selected Fixture Patches to the right on the same row.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddPatchNextRow, "Add Patches on new row", "Adds the selected Fixture Patches to the next row.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddPatchToSelection, "Set Patch", "Uses the selected Fixture Patch in the selected Fader Group. Clears the previous patch.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GroupPatchRight, "Group Patches to the right", "Groups the selected Fixture Patches to the right on the same row.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GroupPatchNextRow, "Group Patches on new row", "Groups the selected Fixture Patches to the next row.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddEmptyGroupRight, "Add Empty Fader Group to the right", "Adds a new Empty Fader Group to the right on the same row.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddEmptyGroupNextRow, "Add Empty Fader Group on new row", "Adds a new Empty Fader Group to the next row.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE 

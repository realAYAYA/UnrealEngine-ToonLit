// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/RemoteControlCommands.h"

#include "UI/RemoteControlPanelStyle.h"

#define LOCTEXT_NAMESPACE "RemoteControlCommands"

FRemoteControlCommands::FRemoteControlCommands()
	: TCommands<FRemoteControlCommands>
	(
		TEXT("RemoteControl"),
		LOCTEXT("RemoteControl", "Remote Control API"),
		NAME_None,
		FRemoteControlPanelStyle::GetStyleSetName()
	)
{
}

void FRemoteControlCommands::RegisterCommands()
{
	// Save Preset
	UI_COMMAND(SavePreset, "Save", "Saves this preset", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::S));

	// Find Preset
	UI_COMMAND(FindPresetInContentBrowser, "Browse to Preset", "Browses to the associated preset and selects it in the most recently used Content Browser (summoning one if necessary)", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::B));

	// Toggle Protocol Mappings
	UI_COMMAND(ToggleProtocolMappings, "Protocols", "View list of protocols mapped to active selection.", EUserInterfaceActionType::ToggleButton, FInputChord());

	// Toggle Logic Editor
	UI_COMMAND(ToggleLogicEditor, "Logic", "View the logic applied to active selection.", EUserInterfaceActionType::ToggleButton, FInputChord());

	// Delete Entity
	UI_COMMAND(DeleteEntity, "Delete", "Delete the selected  group/exposed entity from the list.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));

	// Rename Entity
	UI_COMMAND(RenameEntity, "Rename", "Rename the selected  group/exposed entity.", EUserInterfaceActionType::Button, FInputChord(EKeys::F2));

	// Copy Item
	UI_COMMAND(CopyItem, "Copy", "Copy the selected UI item", EUserInterfaceActionType::Button, FInputChord(EKeys::C, EModifierKey::Control));

	// Paste Item
	UI_COMMAND(PasteItem, "Paste", "Paste the selected UI item", EUserInterfaceActionType::Button, FInputChord(EKeys::V, EModifierKey::Control));

	// Duplicate Item
	UI_COMMAND(DuplicateItem, "Duplicate", "Duplicate the selected UI item", EUserInterfaceActionType::Button, FInputChord(EKeys::D, EModifierKey::Control));

	// Update Value
	UI_COMMAND(UpdateValue, "Update Value", "Update the selected UI item value with the one in the fields list", EUserInterfaceActionType::Button, FInputChord(EKeys::U, EModifierKey::Control));
}

#undef LOCTEXT_NAMESPACE

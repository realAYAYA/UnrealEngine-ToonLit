// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingEditorCommands.h"

#include "Styling/AppStyle.h"
#include "Framework/Commands/Commands.h"

#define LOCTEXT_NAMESPACE "DMXPixelMappingEditorCommands"

FDMXPixelMappingEditorCommands::FDMXPixelMappingEditorCommands()
	: TCommands<FDMXPixelMappingEditorCommands>
	(
		TEXT("DMXPixelMappingEditor"),
		NSLOCTEXT("Contexts", "DMXPixelMappingEditor", "DMX Pixel Mapping"),
		NAME_None,
		FAppStyle::GetAppStyleSetName()
	)
{
}

void FDMXPixelMappingEditorCommands::RegisterCommands()
{
	UI_COMMAND(SaveThumbnailImage, "Thumbnail", "Generate Thumbnail", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(AddMapping, "Add Mapping", "Add Mapping", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(PlayDMX, "Play DMX", "Play DMX.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopPlayingDMX, "Stop Playing DMX", "Stop Playing DMX.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(TogglePlayDMXAll, "Toggle Play DMX All", "Toggles playing all render components and all their childs", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(SizeComponentToTexture, "Size Component to Texture", "Sizes the selected group to Texture.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::S));
	
	UI_COMMAND(ToggleScaleChildrenWithParent, "Scale Children with Parent", "Sets if children are scaled with their parent, when the parent is resized.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::Q));
	UI_COMMAND(ToggleAlwaysSelectGroup, "Always select Group", "Sets if the parent is selected, if a child is clicked.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::W));
	UI_COMMAND(ToggleApplyLayoutScriptWhenLoaded, "Apply Layout Script instantly", "Sets if layout script are applied as soon as they are loaded.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::E));
	UI_COMMAND(ToggleShowComponentNames, "Show Component Names", "Sets if the name of components are displayed. Note, may impact editor performance.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::A));
	UI_COMMAND(ToggleShowPatchInfo, "Show Patch Info", "Sets if information about the pach is displayed. Note, may impact editor performance.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::S));
	UI_COMMAND(ToggleShowCellIDs, "Show Cell IDs", "Sets if the cell IDs of matrix cells are displayed.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::D));
}

#undef LOCTEXT_NAMESPACE 

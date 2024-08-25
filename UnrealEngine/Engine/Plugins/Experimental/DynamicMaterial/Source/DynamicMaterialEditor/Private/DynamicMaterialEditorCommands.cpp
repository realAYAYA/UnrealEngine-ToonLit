// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicMaterialEditorCommands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "MaterialDesignerCommands"

FDynamicMaterialEditorCommands::FDynamicMaterialEditorCommands()
	: TCommands<FDynamicMaterialEditorCommands>(TEXT("MaterialDesigner")
		, LOCTEXT("MaterialDesigner", "MaterialDesigner")
		, NAME_None
		, FAppStyle::GetAppStyleSetName()
	)
{
}

void FDynamicMaterialEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenEditorSettingsWindow
		, "Open Material Designer Settings..."
		, "Opens the Material Designer settings window."
		, EUserInterfaceActionType::Button
		, FInputChord());

	UI_COMMAND(AddDefaultLayer
		, "Add Layer"
		, "Adds a new default texture layer at the top of the slot currently being edited."
		, EUserInterfaceActionType::Button
		, FInputChord(EKeys::Insert));

	UI_COMMAND(InsertDefaultLayerAbove
		, "Insert Layer Above"
		, "Inserts a new default texture layer above the selected layer in the slot currently being edited."
		, EUserInterfaceActionType::Button
		, FInputChord(EKeys::Insert, EModifierKey::Shift));

	UI_COMMAND(SelectLayerBaseStage
		, "Select Base Stage"
		, "Selects the base stage of the currently selected layer."
		, EUserInterfaceActionType::Button
		, FInputChord(EKeys::Comma));

	UI_COMMAND(SelectLayerMaskStage
		, "Select Mask Stage"
		, "Selects the mask stage of the currently selected layer."
		, EUserInterfaceActionType::Button
		, FInputChord(EKeys::Period));

	UI_COMMAND(MoveLayerUp
		, "Move Layer Up"
		, "Moves a layer up in the order, moving it closer to the top of the layer stack."
		, EUserInterfaceActionType::Button
		, FInputChord(EKeys::LeftBracket, EModifierKey::Alt));

	UI_COMMAND(MoveLayerDown
		, "Move Layer Down"
		, "Moves a layer down in the order, moving it closer to the bottom of the layer stack."
		, EUserInterfaceActionType::Button
		, FInputChord(EKeys::RightBracket, EModifierKey::Alt));
}

#undef LOCTEXT_NAMESPACE

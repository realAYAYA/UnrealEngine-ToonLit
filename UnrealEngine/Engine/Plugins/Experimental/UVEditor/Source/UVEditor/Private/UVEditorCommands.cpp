// Copyright Epic Games, Inc. All Rights Reserved.

#include "UVEditorCommands.h"

#include "Styling/AppStyle.h"
#include "Framework/Commands/InputChord.h"
#include "UVEditorStyle.h"

#define LOCTEXT_NAMESPACE "FUVEditorCommands"
	
FUVEditorCommands::FUVEditorCommands()
	: TCommands<FUVEditorCommands>("UVEditor",
		LOCTEXT("ContextDescription", "UV Editor"), 
		NAME_None, // Parent
		FUVEditorStyle::Get().GetStyleSetName()
		)
{
}

void FUVEditorCommands::RegisterCommands()
{
	// These are part of the asset editor UI
	UI_COMMAND(OpenUVEditor, "UV Editor", "Open the UV Editor window.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ApplyChanges, "Apply", "Apply changes to original meshes", EUserInterfaceActionType::Button, FInputChord());

	// These get linked to various tool buttons.
	UI_COMMAND(BeginLayoutTool, "Layout", "Pack existing UVs", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginTransformTool, "Transform", "Transform existing UVs", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginAlignTool, "Align", "Align existing UVs", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginDistributeTool, "Distribute", "Distribute existing UVs", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginParameterizeMeshTool, "AutoUV", "Auto-unwrap and pack UVs", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginChannelEditTool, "Channels", "Modify UV channels", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginSeamTool, "Seam", "Add UV seams", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginRecomputeUVsTool, "Unwrap", "Perform UV unwrapping", EUserInterfaceActionType::ToggleButton, FInputChord());

	// These currently get linked to actions inside the select tool, but will eventually have their own buttons among the tools
	// once selection is pulled out to mode-level.
	UI_COMMAND(SewAction, "Sew", "Sew edges highlighted in red to edges highlighted in green", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SplitAction, "Split",
	           "Given an edge selection, split those edges. Given a vertex selection, split any selected bowtie vertices. Given a triangle selection, split along selection boundaries.",
	           EUserInterfaceActionType::Button, FInputChord());

	// These allow us to link up to pressed keys
	UI_COMMAND(AcceptOrCompleteActiveTool, "Accept", "Accept the active tool", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter));
	UI_COMMAND(CancelOrCompleteActiveTool, "Cancel", "Cancel the active tool or clear current selection", EUserInterfaceActionType::Button, FInputChord(EKeys::Escape));

	// These get used in viewport buttons
	UI_COMMAND(VertexSelection, "Vertex Selection", "Select vertices", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::One));
	UI_COMMAND(EdgeSelection, "Edge Selection", "Select edges", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Two));
	UI_COMMAND(TriangleSelection, "Triangle Selection", "Select triangles", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Three));
	UI_COMMAND(IslandSelection, "Island Selection", "Select islands", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Four));
	UI_COMMAND(FullMeshSelection, "Mesh Selection", "Select meshes", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Five));
	UI_COMMAND(SelectAll, "Select All", "Select everything based on current selection mode", EUserInterfaceActionType::None, FInputChord(EKeys::A, EModifierKey::Control));

	UI_COMMAND(EnableOrbitCamera, "Orbit", "Enable orbit camera", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(EnableFlyCamera, "Fly", "Enable fly camera", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(SetFocusCamera, "Focus Camera", "Focus camera around the currently selected UVs", EUserInterfaceActionType::Button, FInputChord(EKeys::F, EModifierKey::Alt));

	UI_COMMAND(ToggleBackground, "Toggle Background", "Toggle background display", EUserInterfaceActionType::ToggleButton,
	           FInputChord(EModifierKey::Alt, EKeys::B));
}


#undef LOCTEXT_NAMESPACE

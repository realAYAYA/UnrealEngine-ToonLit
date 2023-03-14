// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshPaintModeCommands.h"
#include "Framework/Commands/UIAction.h"
#include "MeshPaintMode.h"
#include "SingleSelectionTool.h"
#include "MeshVertexPaintingTool.h"

#define LOCTEXT_NAMESPACE "MeshPaintEditorModeCommands"



void FMeshPaintingToolActionCommands::GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs)
{
	ToolCDOs.Add(GetMutableDefault<USingleSelectionTool>());
	ToolCDOs.Add(GetMutableDefault<UMeshColorPaintingTool>());
}




void FMeshPaintingToolActionCommands::RegisterAllToolActions()
{
	FMeshPaintingToolActionCommands::Register();
}

void FMeshPaintingToolActionCommands::UnregisterAllToolActions()
{
	FMeshPaintingToolActionCommands::Unregister();
}

void FMeshPaintingToolActionCommands::UpdateToolCommandBinding(UInteractiveTool* Tool, TSharedPtr<FUICommandList> UICommandList, bool bUnbind)
{
	if (FMeshPaintingToolActionCommands::IsRegistered())
	{
		!bUnbind ? FMeshPaintingToolActionCommands::Get().BindCommandsForCurrentTool(UICommandList, Tool) : FMeshPaintingToolActionCommands::Get().UnbindActiveCommands(UICommandList);
	}
}




void FMeshPaintEditorModeCommands::RegisterCommands()
{
	TArray<TSharedPtr<FUICommandInfo>> ColorCommands;
	TArray<TSharedPtr<FUICommandInfo>> WeightCommands;
	TArray<TSharedPtr<FUICommandInfo>> VertexCommands;
	TArray<TSharedPtr<FUICommandInfo>> TextureCommands;
	 
	UI_COMMAND(VertexSelect, "Select", "Select the mesh for vertex painting", EUserInterfaceActionType::ToggleButton, FInputChord());
	ColorCommands.Add(VertexSelect);
	WeightCommands.Add(VertexSelect);

	UI_COMMAND(ColorPaint, "Paint", "Paint the mesh", EUserInterfaceActionType::ToggleButton, FInputChord());
	ColorCommands.Add(ColorPaint);
	UI_COMMAND(WeightPaint, "Paint", "Paint the mesh", EUserInterfaceActionType::ToggleButton, FInputChord());
	WeightCommands.Add(WeightPaint);
	UI_COMMAND(SwitchForeAndBackgroundColor, "Swap", "Switches the Foreground and Background Colors used for Vertex Painting", EUserInterfaceActionType::Button, FInputChord(EKeys::X));
	VertexCommands.Add(SwitchForeAndBackgroundColor);
	UI_COMMAND(Fill, "Fill", "Fills the selected Meshes with the Paint Color", EUserInterfaceActionType::Button, FInputChord());
	VertexCommands.Add(Fill);
	UI_COMMAND(Propagate, "Apply", "Propagates Instance Vertex Colors to the Source Meshes", EUserInterfaceActionType::Button, FInputChord());
	VertexCommands.Add(Propagate);
 	UI_COMMAND(Import, "Import", "Imports Vertex Colors from a TGA Texture File to the Selected Meshes", EUserInterfaceActionType::Button, FInputChord());
 	VertexCommands.Add(Import);
 	UI_COMMAND(Save, "Save", "Saves the Source Meshes for the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
	VertexCommands.Add(Save);
	UI_COMMAND(Copy, "Copy", "Copies Vertex Colors from the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
 	VertexCommands.Add(Copy);
	UI_COMMAND(Paste, "Paste", "Tried to Paste Vertex Colors on the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
 	VertexCommands.Add(Paste);
	UI_COMMAND(Remove, "Remove", "Removes Vertex Colors from the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
	VertexCommands.Add(Remove);
	UI_COMMAND(Fix, "Fix", "If necessary fixes Vertex Colors applied to the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
	VertexCommands.Add(Fix);
	UI_COMMAND(PropagateVertexColorsToLODs, "All LODs", "Applied the Vertex Colors from LOD0 to all LOD levels", EUserInterfaceActionType::Button, FInputChord());
	VertexCommands.Add(PropagateVertexColorsToLODs);

	UI_COMMAND(CycleToPreviousLOD, "Previous LOD", "Cycles to the previous possible Mesh LOD to Paint on", EUserInterfaceActionType::Button, FInputChord(EKeys::B));
	UI_COMMAND(CycleToNextLOD, "Next LOD", "Cycles to the next possible Mesh LOD to Paint on", EUserInterfaceActionType::Button, FInputChord(EKeys::N));

	ColorCommands.Append(VertexCommands);
	WeightCommands.Append(VertexCommands);
	Commands.Add(UMeshPaintMode::MeshPaintMode_Color, ColorCommands);
	Commands.Add(UMeshPaintMode::MeshPaintMode_Weights, WeightCommands);

	UI_COMMAND(TextureSelect, "Select", "Select the mesh for texture painting", EUserInterfaceActionType::ToggleButton, FInputChord());
	TextureCommands.Add(TextureSelect);
	UI_COMMAND(TexturePaint, "Paint", "Paint the mesh", EUserInterfaceActionType::ToggleButton, FInputChord());
	TextureCommands.Add(TexturePaint);
	
	UI_COMMAND(PreviousTexture, "Previous Texture", "Cycle To Previous Texture", EUserInterfaceActionType::Button, FInputChord(EKeys::Comma));
	UI_COMMAND(NextTexture, "Next Texture", "Cycle To Next Texture", EUserInterfaceActionType::Button, FInputChord(EKeys::Period));

	UI_COMMAND(PropagateTexturePaint, "Commit", "Commits Texture Painting Changes", EUserInterfaceActionType::Button, FInputChord(EKeys::C, EModifierKey::Control | EModifierKey::Shift));
	TextureCommands.Add(PropagateTexturePaint);
	UI_COMMAND(SaveTexturePaint, "Save", "Saves the Modified Textures for the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
	TextureCommands.Add(SaveTexturePaint);

	Commands.Add(UMeshPaintMode::MeshPaintMode_Texture, TextureCommands);




}

#undef LOCTEXT_NAMESPACE


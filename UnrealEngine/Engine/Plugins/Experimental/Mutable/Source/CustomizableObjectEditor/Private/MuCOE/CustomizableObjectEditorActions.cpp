// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectEditorActions.h"

#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


FCustomizableObjectEditorCommands::FCustomizableObjectEditorCommands() 
	: TCommands<FCustomizableObjectEditorCommands>
(
	"CustomizableObjectEditor", // Context name for fast lookup
	NSLOCTEXT("Contexts", "CustomizableObjectEditor", "CustomizableObject Editor"), // Localized context name for displaying
	NAME_None, // Parent
	FCustomizableObjectEditorStyle::GetStyleSetName()
	)
{
}


void FCustomizableObjectEditorCommands::RegisterCommands()
{
	UI_COMMAND(Compile, "Compile", "Compile the source graph of the customizable object and update the previews.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CompileOnlySelected, "Compile Only Selected", "Compile the source graph of the customizable object and update the previews, only for the selected options in the preview. The rest of options are discarded. If they are selected, press again this button to see their effect in the preview.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResetCompileOptions, "Reset Compilation Options", "Set reasonable defaults for the compilation options.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CompileOptions_EnableTextureCompression, "Enable texture compression.", "Only for debug. Do not use.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(CompileOptions_UseParallelCompilation, "Enable compiling in multiple threads.", "This is faster but use more memory.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(CompileOptions_UseDiskCompilation, "Enable compiling using the disk as memory.", "This is very slow but supports compiling huge objects. It requires a lot of free space in the OS disk.", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(Debug, "Debug", "Debug the object.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DebugOptions_OnlySelected, "Debug Only Selected", "Debug only for the selected options in the preview. The rest of options are discarded.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DebugOptions_EnableTextureCompression, "Enable texture compression.", "Only for debug. Do not use.", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(PerformanceReport, "Performance Report", "Open the Performance Report window to set up and perform all tests relevant to Customizable Objects and access worst cases data.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(TextureAnalyzer, "Texture Memory Analyzer", "Open the Texture Analyzer window to check all the information of the textures created by Mutable.", EUserInterfaceActionType::Button, FInputChord());
}


FCustomizableObjectEditorViewportCommands::FCustomizableObjectEditorViewportCommands() 
	: TCommands<FCustomizableObjectEditorViewportCommands>
(
	"CustomizableObjectEditorViewport", // Context name for fast lookup
	NSLOCTEXT("Contexts", "CustomizableObjectEditorViewport", "CustomizableObject Editor Viewport"), // Localized context name for displaying
	NAME_None, // Parent
	FCustomizableObjectEditorStyle::GetStyleSetName()
	)
{
}


void FCustomizableObjectEditorViewportCommands::RegisterCommands()
{
	UI_COMMAND( SetDrawUVs, "UV", "Toggles display of the static mesh's UVs for the specified channel.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetShowGrid, "Grid", "Displays the viewport grid.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND( SetShowSky, "Sky", "Displays the viewport sky.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetShowBounds, "Bounds", "Toggles display of the bounds of the static mesh.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetShowCollision, "Collision", "Toggles display of the simplified collision mesh of the static mesh, if one has been assigned.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetCameraLock, "Camera Lock", "Toggles viewport navigation between orbit and freely moving about.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SaveThumbnail, "Save Thumbnail", "Saves the viewpoint position and orientation in the Preview Pane for use as the thumbnail preview in the Content Browser.", EUserInterfaceActionType::Button, FInputChord() );

	UI_COMMAND( SetShowNormals, "Normals", "Toggles display of vertex normals in the Preview Pane.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetShowTangents, "Tangents", "Toggles display of vertex tangents in the Preview Pane.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetShowBinormals, "Binormals", "Toggles display of vertex binormals (orthogonal vector to normal and tangent) in the Preview Pane.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetShowPivot, "Show Pivot", "Display the pivot location of the static mesh.", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND(BakeInstance, "Bake Instance from Object", "Create baked unreal resources for the current preview instance.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StateChangeTest, "State test", "Make a state change test, iterating through all possible states and runtime parameters for this instance", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StateChangeShowData, "Show or hide test results", "Show or hide test results", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(StateChangeShowGeometryData, "Show instance geometry data", "Show instance geometry data", EUserInterfaceActionType::ToggleButton, FInputChord());
}


#undef LOCTEXT_NAMESPACE


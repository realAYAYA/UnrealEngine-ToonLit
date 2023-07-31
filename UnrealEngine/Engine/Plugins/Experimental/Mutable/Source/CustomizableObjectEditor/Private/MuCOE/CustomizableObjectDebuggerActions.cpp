// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectDebuggerActions.h"

#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


FCustomizableObjectDebuggerCommands::FCustomizableObjectDebuggerCommands() 
	: TCommands<FCustomizableObjectDebuggerCommands>
(
	"CustomizableObjectDebugger", // Context name for fast lookup
	NSLOCTEXT("Contexts", "CustomizableObjectDebugger", "CustomizableObject Debugger"),
	NAME_None, // Parent
	FCustomizableObjectEditorStyle::GetStyleSetName()
	)
{
}


void FCustomizableObjectDebuggerCommands::RegisterCommands()
{
	UI_COMMAND(GenerateMutableGraph, "Unreal to Mutable Graph", "Generate a mutable graph from the customizable obejct source graph.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CompileMutableCode, "Unreal to Mutable Code", "Compile the mutable graph of the customizable object and update the previews.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CompileOptions_EnableTextureCompression, "Enable texture compression.", "Only for debug. Do not use.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(CompileOptions_UseParallelCompilation, "Enable compiling in multiple threads.", "This is faster but use more memory.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(CompileOptions_UseDiskCompilation, "Enable compiling using the disk as memory.", "This is very slow but supports compiling huge objects. It requires a lot of free space in the OS disk.", EUserInterfaceActionType::ToggleButton, FInputChord());
}


#undef LOCTEXT_NAMESPACE


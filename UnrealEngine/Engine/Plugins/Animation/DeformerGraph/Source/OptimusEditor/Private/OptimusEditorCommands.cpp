// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusEditorCommands.h"

#include "Styling/AppStyle.h"


#define LOCTEXT_NAMESPACE "OptimusEditorCommands"


FOptimusEditorCommands::FOptimusEditorCommands() 
	: TCommands<FOptimusEditorCommands>(
		"OptimusEditor", // Context name for fast lookup
		NSLOCTEXT("Contexts", "DeformerGraphEditor", "Deformer Graph Editor"), // Localized context name for displaying
		NAME_None,
		FAppStyle::GetAppStyleSetName()
	)
{

}


void FOptimusEditorCommands::RegisterCommands()
{
	UI_COMMAND(Compile, "Compile", "Compile the current deformer graph into a compute kernel graph.", EUserInterfaceActionType::Button, FInputChord(EKeys::F7));
}


#undef LOCTEXT_NAMESPACE

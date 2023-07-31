// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectEditorNodeContextCommands.h"

#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditorNodeContextCommands"

void FCustomizableObjectEditorNodeContextCommands::RegisterCommands()
{
	UI_COMMAND(RefreshMaterialNodesInAllChildren, "Refresh all material nodes in all children", "Refresh all material nodes in all children.", EUserInterfaceActionType::Button, FInputChord());
	
	FKey CommentSpawnKey = FKey(TEXT("C"));
	UI_COMMAND(CreateComment, "Create a Comment node", "Create a Comment node", EUserInterfaceActionType::Button, FInputChord(CommentSpawnKey));
}
#undef LOCTEXT_NAMESPACE

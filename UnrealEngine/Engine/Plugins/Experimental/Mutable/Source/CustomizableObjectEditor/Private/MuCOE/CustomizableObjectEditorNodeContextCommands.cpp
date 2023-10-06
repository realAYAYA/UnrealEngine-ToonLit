// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectEditorNodeContextCommands.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectEditorNodeContextCommands"

void FCustomizableObjectEditorNodeContextCommands::RegisterCommands()
{
	FKey CommentSpawnKey = FKey(TEXT("C"));
	UI_COMMAND(CreateComment, "Create a Comment node", "Create a Comment node", EUserInterfaceActionType::Button, FInputChord(CommentSpawnKey));
}
#undef LOCTEXT_NAMESPACE

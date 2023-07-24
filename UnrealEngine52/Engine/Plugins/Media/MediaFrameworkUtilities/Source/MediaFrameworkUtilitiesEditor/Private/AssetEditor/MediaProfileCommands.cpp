// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaProfileCommands.h"




#define LOCTEXT_NAMESPACE "MediaProfileCommands"

void FMediaProfileCommands::RegisterCommands()
{
	UI_COMMAND(Apply, "Apply", "Apply changes to the media profile.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Edit, "Edit", "Edit the current media profile.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

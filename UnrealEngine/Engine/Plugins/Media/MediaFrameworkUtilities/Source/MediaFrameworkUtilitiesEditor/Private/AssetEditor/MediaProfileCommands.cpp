// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaProfileCommands.h"

#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Profile/IMediaProfileManager.h"
#include "Profile/MediaProfile.h"



#define LOCTEXT_NAMESPACE "MediaProfileCommands"

void FMediaProfileCommands::RegisterCommands()
{
	UI_COMMAND(Apply, "Apply", "Apply changes to the media profile.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Edit, "Edit", "Edit the current media profile.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
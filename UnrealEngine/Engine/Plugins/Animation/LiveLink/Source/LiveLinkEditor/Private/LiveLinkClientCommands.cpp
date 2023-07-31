// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkClientCommands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "LiveLinkClientCommands"

FLiveLinkClientCommands::FLiveLinkClientCommands()
	: TCommands<FLiveLinkClientCommands>("LiveLinkClient.Common", LOCTEXT("LiveLinkCommandsLabel", "Live Link"), NAME_None, FName(TEXT("LiveLinkStyle")))
{
}

void FLiveLinkClientCommands::RegisterCommands()
{
	UI_COMMAND(RemoveSource, "Remove Selected Source(s)", "Remove selected live link source", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveAllSources, "Remove All Sources", "Remove all live link sources", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveSubject, "Remove Subject", "Remove selected live link subject", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

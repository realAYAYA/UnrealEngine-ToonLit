// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdvancedRenamerCommands.h"
#include "AdvancedRenamerStyle.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Styling/ISlateStyle.h"

#define LOCTEXT_NAMESPACE "AdvancedRenamerCommands"

FAdvancedRenamerCommands::FAdvancedRenamerCommands()
	: TCommands<FAdvancedRenamerCommands>(
		TEXT("AdvancedRenamer"),
		LOCTEXT("AdvancedRenamer", "Advanced Renamer"),
		NAME_None,
		FAdvancedRenamerStyle::Get().GetStyleSetName()
	)
{
}

void FAdvancedRenamerCommands::RegisterCommands()
{
	UI_COMMAND(RenameSelectedActors
		, "Rename Selected Actors"
		, "Opens the Advanced Renamer Panel to rename all selected actors."
		, EUserInterfaceActionType::Button
		, FInputChord())

	UI_COMMAND(RenameSharedClassActors
		, "Rename Actors of Selected Actor Classes"
		, "Opens the Advanced Renamer Panel to rename all actors sharing a class with any selected actor."
		, EUserInterfaceActionType::Button
		, FInputChord())
}

#undef LOCTEXT_NAMESPACE

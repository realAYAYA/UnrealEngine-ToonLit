// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseEditorCommands.h"

#define LOCTEXT_NAMESPACE "PoseSearchDatabaseEditorCommands"

namespace UE::PoseSearch
{
	void FDatabaseEditorCommands::RegisterCommands()
	{
		UI_COMMAND(BuildSearchIndex, "Build Index", "Build Index", EUserInterfaceActionType::Button, FInputChord());

		UI_COMMAND(
			ShowPoseFeaturesNone, "None", "Hide all features",
			EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(
			ShowPoseFeaturesAll, "All Features", "Show all features",
			EUserInterfaceActionType::RadioButton, FInputChord());

		UI_COMMAND(
			ShowAnimationNone, "None", "Don't show animations",
			EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(
			ShowAnimationOriginalOnly, "Original Only", "Show only original animations",
			EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(
			ShowAnimationOriginalAndMirrored, "Original and mirrored", "Show original and mirrored animations",
			EUserInterfaceActionType::RadioButton, FInputChord());

	}
}

#undef LOCTEXT_NAMESPACE

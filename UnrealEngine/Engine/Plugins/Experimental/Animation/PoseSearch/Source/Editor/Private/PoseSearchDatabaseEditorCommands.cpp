// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseEditorCommands.h"

#define LOCTEXT_NAMESPACE "PoseSearchDatabaseEditorCommands"

namespace UE::PoseSearch
{
	void FDatabaseEditorCommands::RegisterCommands()
	{
		UI_COMMAND(ShowPoseFeaturesNone, "None", "Hide all features", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(ShowPoseFeaturesAll, "All Features", "Show all features", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(ShowPoseFeaturesDetailed, "Detailed Features", "Show all features with mode details", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(ShowAnimationOriginalOnly, "Original Only", "Show only original animations", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(ShowAnimationOriginalAndMirrored, "Original and mirrored", "Show original and mirrored animations", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(ShowDisplayRootMotionSpeed, "Display Root Motion Speed", "Show root motion speed for the selected item", EUserInterfaceActionType::ToggleButton, FInputChord());
		UI_COMMAND(ShowQuantizeAnimationToPoseData, "Quantize Animation To Pose Data", "Animations will not playing continuously, only using the associated pose quantized time", EUserInterfaceActionType::ToggleButton, FInputChord());
		UI_COMMAND(ShowBones, "Show All Bones", "Debug Draw All Bones", EUserInterfaceActionType::ToggleButton, FInputChord());
	}
}

#undef LOCTEXT_NAMESPACE

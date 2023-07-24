// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseCorrectivesCommands.h"

#define LOCTEXT_NAMESPACE "PoseCorrectivesCommands"

void FPoseCorrectivesCommands::RegisterCommands()
{
	UI_COMMAND(AddCorrectivePose, "Add Corrective", "Add current source and corrective pose.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SaveCorrective, "Save Corrective", "Save current corrective pose.", EUserInterfaceActionType::Button, FInputChord())
	UI_COMMAND(CancelCorrective, "Cancel Changes", "Cancel current changes to corrective pose.", EUserInterfaceActionType::Button, FInputChord())
	
	UI_COMMAND(MultiSelectCorrectiveCurvesCommand, "Select Corrective Curves", "Select the highlighted corrective curves", EUserInterfaceActionType::Button, FInputChord())
	UI_COMMAND(MultiSelectDriverCurvesCommand, "Select Driver Curves", "Select the highlighted driver curves", EUserInterfaceActionType::Button, FInputChord())
	UI_COMMAND(MultiSelectCorrectiveBonesCommand, "Select Corrective Bones", "Select the highlighted corrective bones", EUserInterfaceActionType::Button, FInputChord())
	UI_COMMAND(MultiSelectDriverBonesCommand, "Select Driver Bones", "Select the highlighted driver bones", EUserInterfaceActionType::Button, FInputChord())
	
	UI_COMMAND(MultiDeselectCorrectiveCurvesCommand, "Deselect Corrective Curves", "Deselect the highlighted corrective curves", EUserInterfaceActionType::Button, FInputChord())
	UI_COMMAND(MultiDeselectDriverCurvesCommand, "Deselect Driver Curves", "Deselect the highlighted driver curves", EUserInterfaceActionType::Button, FInputChord())
	UI_COMMAND(MultiDeselectCorrectiveBonesCommand, "Deselect Corrective Bones", "Deselect the highlighted corrective bones", EUserInterfaceActionType::Button, FInputChord())
	UI_COMMAND(MultiDeselectDriverBonesCommand, "Deselect Driver Bones", "Deselect the highlighted driver bones", EUserInterfaceActionType::Button, FInputChord())
}

#undef LOCTEXT_NAMESPACE

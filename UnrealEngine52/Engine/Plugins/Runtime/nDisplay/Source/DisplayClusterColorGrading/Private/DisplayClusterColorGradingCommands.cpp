// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterColorGradingCommands.h"

#define LOCTEXT_NAMESPACE "DisplayClusterColorGrading"

void FDisplayClusterColorGradingCommands::RegisterCommands()
{
	UI_COMMAND(SaturationColorWheelVisibility, "Saturation", "Show or hide the saturation color wheel", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ContrastColorWheelVisibility, "Constrast", "Show or hide the constrast color wheel", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ColorWheelSliderOrientationHorizontal, "Right", "Puts the color wheel sliders to the right of the color wheel", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ColorWheelSliderOrientationVertical, "Below", "Puts the color wheel sliders below the color wheel", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(OpenColorGradingDrawer, "Open Color Grading Drawer", "Opens the color grading drawer from the status bar", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::SpaceBar));
}

#undef LOCTEXT_NAMESPACE

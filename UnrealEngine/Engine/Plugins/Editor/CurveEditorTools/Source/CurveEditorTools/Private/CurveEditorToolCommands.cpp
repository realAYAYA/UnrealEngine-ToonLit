// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveEditorToolCommands.h"

#define LOCTEXT_NAMESPACE "CurveEditorToolCommands"

void FCurveEditorToolCommands::RegisterCommands()
{
	// Focus Tools
	UI_COMMAND(SetFocusPlaybackTime, "Focus Playback Time", "Focuses the Curve Editor on the current Playback Time without changing zoom level.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetFocusPlaybackRange, "Focus Playback Range", "Focuses the Curve Editor on the current Playback Range with zoom based on visible curves.", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::A));

	// Tool Modes
	UI_COMMAND(ActivateTransformTool, "Transform Tool", "Activates the Transform tool which allows translation, scale and rotation of selected keys.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Control, EKeys::T));
	UI_COMMAND(ActivateRetimeTool, "Retime Tool", "Activates the Retime tool which allows you to define a one dimensional lattice to non-uniformly rescale key times.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Control, EKeys::E));
	UI_COMMAND(ActivateMultiScaleTool, "Multi Scale Tool", "Activates the Multi Select tool which allows fast scaling of multiple curves from unique pivots", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Control, EKeys::M));
}

#undef LOCTEXT_NAMESPACE

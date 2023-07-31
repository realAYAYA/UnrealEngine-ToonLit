// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigCurveContainerCommands.h"

#define LOCTEXT_NAMESPACE "CurveContainerCommands"

void FCurveContainerCommands::RegisterCommands()
{
	UI_COMMAND( AddCurve, "Add Curve", "Add a curve to the ControlRig", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

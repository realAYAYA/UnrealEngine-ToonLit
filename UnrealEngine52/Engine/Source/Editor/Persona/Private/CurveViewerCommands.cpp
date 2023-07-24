// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveViewerCommands.h"

#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"

#define LOCTEXT_NAMESPACE "CurveViewerCommands"

void FCurveViewerCommands::RegisterCommands()
{
	UI_COMMAND( AddCurve, "Add Curve", "Add a curve to the Skeleton", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

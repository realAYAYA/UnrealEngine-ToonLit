// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionSelectionCommands.h"

#define LOCTEXT_NAMESPACE "GeometryCollectionSelectionCommands"

void FGeometryCollectionSelectionCommands::RegisterCommands()
{
	UI_COMMAND(SelectAllGeometry, "Select All Geometry In Hierarchy", "Select all geometry in hierarchy", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SelectNone, "Deselect All Geometry In Hierarchy", "Deselect all geometry in hierarchy", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SelectInverseGeometry, "Select Inverse Geometry In Hierarchy", "Select inverse geometry in hierarchy", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
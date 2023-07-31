// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXEditorTabNames.h"


const FName FDMXEditorTabNames::ChannelsMonitor(TEXT("ChannelsMonitorTabName"));
const FName FDMXEditorTabNames::ActivityMonitor(TEXT("ActivityMonitorTabName"));
const FName FDMXEditorTabNames::OutputConsole(TEXT("OutputConsoleTabName"));
const FName FDMXEditorTabNames::PatchTool(TEXT("PatchToolTabName"));

// 'GenericEditor_Properties' was the name of the original controller editor tab. 
// We keep the old name to show the library editor tab in place of the controller editor in existing projects.
// This is less confusing for users that upgrade their project from 4.26 to 4.27, otherwise the tab that shows
// ports wouldn't be visible by default after the update.
const FName FDMXEditorTabNames::DMXLibraryEditor(TEXT("GenericEditor_Properties"));

const FName FDMXEditorTabNames::DMXFixtureTypesEditor("GenericEditor_DMXFixtureTypesEditor");
const FName FDMXEditorTabNames::DMXFixturePatchEditor("GenericEditor_DMXFixturePatchEditor");


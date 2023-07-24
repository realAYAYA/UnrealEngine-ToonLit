// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintModes/WidgetBlueprintApplicationModes.h"

#include "HAL/IConsoleManager.h"
#include "Internationalization/Internationalization.h"

// Mode constants
const FName FWidgetBlueprintApplicationModes::DesignerMode("DesignerName");
const FName FWidgetBlueprintApplicationModes::GraphMode("GraphName");
const FName FWidgetBlueprintApplicationModes::DebugMode("DebugName");

FText FWidgetBlueprintApplicationModes::GetLocalizedMode(const FName InMode)
{
	if (InMode == FWidgetBlueprintApplicationModes::DesignerMode)
	{
		return NSLOCTEXT("WidgetBlueprintModes", "DesignerMode", "Designer");
	}
	else if (InMode == FWidgetBlueprintApplicationModes::GraphMode)
	{
		return NSLOCTEXT("WidgetBlueprintModes", "GraphMode", "Graph");
	}
	else if (InMode == FWidgetBlueprintApplicationModes::DebugMode)
	{
		return NSLOCTEXT("WidgetBlueprintModes", "DebugMode", "Debug");
	}
	return FText::GetEmpty();
}

static bool EnableDebugMode = false;
static FAutoConsoleVariableRef CVarEnableDebugMode(
	TEXT("UMG.EnableDebugMode"), 
	EnableDebugMode, 
	TEXT("Whether or not to enable the UMG Debug mode.")
);

bool FWidgetBlueprintApplicationModes::IsDebugModeEnabled()
{
	return EnableDebugMode;
}

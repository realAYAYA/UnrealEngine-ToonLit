// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintModes/WidgetBlueprintApplicationModes.h"

#include "Internationalization/Internationalization.h"

// Mode constants
const FName FWidgetBlueprintApplicationModes::DesignerMode("DesignerName");
const FName FWidgetBlueprintApplicationModes::GraphMode("GraphName");

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
	return FText::GetEmpty();
}

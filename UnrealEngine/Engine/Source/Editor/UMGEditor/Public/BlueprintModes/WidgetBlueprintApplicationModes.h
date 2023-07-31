// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "UObject/NameTypes.h"

/////////////////////////////////////////////////////
// FWidgetBlueprintApplicationModes

// This is the list of IDs for widget blueprint editor modes
struct UMGEDITOR_API FWidgetBlueprintApplicationModes
{
	// Mode constants
	static const FName DesignerMode;
	static const FName GraphMode;

	static FText GetLocalizedMode(const FName InMode);

private:
	FWidgetBlueprintApplicationModes() = delete;
};

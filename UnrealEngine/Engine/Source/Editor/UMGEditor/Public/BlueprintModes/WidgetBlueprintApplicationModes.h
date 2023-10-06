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
	UE_DEPRECATED(5.3, "DebugMode is deprecated. Use PreviewMode instead.")
	static const FName DebugMode;
	static const FName PreviewMode;

	static FText GetLocalizedMode(const FName InMode);

	UE_DEPRECATED(5.3, "IsDebugModeEnabled is deprecated. Use IsPreviewModeEnabled instead.")
	static bool IsDebugModeEnabled();
	static bool IsPreviewModeEnabled();	

private:
	FWidgetBlueprintApplicationModes() = delete;
};

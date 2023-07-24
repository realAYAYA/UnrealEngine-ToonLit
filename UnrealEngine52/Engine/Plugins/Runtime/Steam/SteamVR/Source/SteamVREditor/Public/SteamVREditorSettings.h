// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SteamVREditorSettings.generated.h"

/**
 * 
 */
UCLASS(config = Editor, defaultconfig, deprecated, meta = (DeprecationMessage = "SteamVR plugin is deprecated; please use the OpenXR plugin."))
class STEAMVREDITOR_API UDEPRECATED_USteamVREditorSettings : public UObject
{
	GENERATED_BODY()
public:
	/** Whether or not to show the SteamVR Input settings toolbar button */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(config, EditAnywhere, Category = "SteamVR Editor Settings", meta = (DeprecatedProperty))
	bool bShowSteamVrInputToolbarButton;
};

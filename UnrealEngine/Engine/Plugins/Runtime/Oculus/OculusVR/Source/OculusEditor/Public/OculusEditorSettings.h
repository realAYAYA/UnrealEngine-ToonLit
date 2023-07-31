// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "OculusEditorSettings.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UENUM()
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EOculusPlatform : uint8
{
	PC UMETA(DisplayName="PC"),
	Mobile UMETA(DisplayName="Mobile"),
	Length UMETA(DisplayName="Invalid")
};

/**
 * 
 */
UCLASS(config=Editor, deprecated, meta = (DeprecationMessage = "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace."))
class OCULUSEDITOR_API UDEPRECATED_UOculusEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UDEPRECATED_UOculusEditorSettings();

	UPROPERTY(config, EditAnywhere, Category = Oculus)
	TMap<FName, bool> PerfToolIgnoreList;
	
	UPROPERTY(config, EditAnywhere, Category = Oculus)
	EOculusPlatform PerfToolTargetPlatform;

	UPROPERTY(globalconfig, EditAnywhere, Category = Oculus)
	bool bAddMenuOption;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

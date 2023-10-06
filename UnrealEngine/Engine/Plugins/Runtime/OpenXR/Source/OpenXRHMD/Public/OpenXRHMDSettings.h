// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OpenXRHMDSettings.generated.h"

/**
* Implements the settings for the OpenXR HMD plugin.
*/
UCLASS(config = Game, defaultconfig)
class OPENXRHMD_API UOpenXRHMDSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Enables foveation provided by the XR_FB_foveation OpenXR extension. */
	UPROPERTY(config, EditAnywhere, Category = "Foveation", meta = (DisplayName = "Enable XR_FB_foveation extension"))
	bool bIsFBFoveationEnabled = false;
};

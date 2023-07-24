// Copyright Epic Games, Inc. All Rights Reserved.

/**
*
* A configuration class used by the PIE preview device system to save settings across sessions.
*/

#pragma once

#include "HAL/Platform.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "PIEPreviewSettings.generated.h"

UCLASS(hidecategories = Object, config = PIEPreviewSettings)
class PIEPREVIEWDEVICEPROFILESELECTOR_API UPIEPreviewSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(config)
	int32 WindowPosX;

	UPROPERTY(config)
	int32 WindowPosY;

	UPROPERTY(config)
	float WindowScalingFactor;
};
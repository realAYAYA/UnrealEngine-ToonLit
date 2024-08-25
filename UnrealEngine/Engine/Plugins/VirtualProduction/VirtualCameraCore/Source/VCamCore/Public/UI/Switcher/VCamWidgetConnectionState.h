// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetConnectionConfig.h"
#include "VCamWidgetConnectionState.generated.h"

struct FVCamConnectionTargetSettings;

USTRUCT(BlueprintType)
struct VCAMCORE_API FVCamWidgetConnectionState
{
	GENERATED_BODY()

	/** A list of widgets to update */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
	TArray<FWidgetConnectionConfig> WidgetConfigs;
};
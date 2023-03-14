// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioExtensionPlugin.h"
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "ITDSpatializationSourceSettings.generated.h"

UCLASS()
class SPATIALIZATION_API UITDSpatializationSourceSettings : public USpatializationPluginSourceSettingsBase
{
	GENERATED_BODY()

public:

	/* Whether we should use any level difference between the left and right channel in our spatialization algorithm. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = SpatializationSettings, meta = (DisplayName = "Enable Level Panning"))
	bool bEnableILD;

	/* This curve will map the intensity of panning (y-axis, 0.0-1.0) as a factor of distance (in Unreal Units) */
	UPROPERTY(GlobalConfig, EditAnywhere, BlueprintReadWrite, Category = SpatializationSettings)
	FRuntimeFloatCurve PanningIntensityOverDistance;
};


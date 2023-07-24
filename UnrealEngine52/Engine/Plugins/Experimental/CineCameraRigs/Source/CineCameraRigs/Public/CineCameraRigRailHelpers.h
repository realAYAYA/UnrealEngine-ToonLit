// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Texture2D.h"

#include "CineCameraRigRailHelpers.generated.h"

UCLASS()
class CINECAMERARIGS_API UCineCameraRigRailHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/* Create a transient heatmap texture from data values*/
	UFUNCTION(BlueprintCallable, Category = "CineCameraRigRail")
	static void CreateOrUpdateSplineHeatmapTexture(UPARAM(ref) UTexture2D*& Texture, const TArray<float>& DataValues, const float LowValue, const float AverageValue, const float HighValue);
};

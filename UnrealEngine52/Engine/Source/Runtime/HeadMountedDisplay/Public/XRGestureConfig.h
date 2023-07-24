// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "XRGestureConfig.generated.h"

UENUM()
enum class ESpatialInputGestureAxis : uint8
{
	None = 0,
	Manipulation = 1,
	Navigation = 2,
	NavigationRails = 3
};


USTRUCT(BlueprintType)
struct HEADMOUNTEDDISPLAY_API FXRGestureConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures")
	bool bTap = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures")
	bool bHold = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures")
	ESpatialInputGestureAxis AxisGesture = ESpatialInputGestureAxis::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures")
	bool bNavigationAxisX = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures")
	bool bNavigationAxisY = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gestures")
	bool bNavigationAxisZ = true;
};


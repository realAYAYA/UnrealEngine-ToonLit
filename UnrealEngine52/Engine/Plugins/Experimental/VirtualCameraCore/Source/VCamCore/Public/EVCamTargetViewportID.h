// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EVCamTargetViewportID.generated.h"

UENUM(BlueprintType, meta=(DisplayName = "VCam Target Viewport ID"))
enum class EVCamTargetViewportID : uint8
{
	Viewport1 = 0,
	Viewport2 = 1,
	Viewport3 = 2,
	Viewport4 = 3,

	Count UMETA(Hidden)
};
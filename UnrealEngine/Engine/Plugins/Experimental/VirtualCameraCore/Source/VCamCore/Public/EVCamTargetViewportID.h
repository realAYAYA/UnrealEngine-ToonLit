// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
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

namespace UE::VCamCore
{
	inline int32 ViewportIdToOrdinality(EVCamTargetViewportID TargetViewport)
	{
		return static_cast<int32>(TargetViewport) + 1;
	}

	inline FString ViewportIdToString(EVCamTargetViewportID TargetViewport)
	{
		return FString(TEXT("Viewport ")) + FString::FromInt(ViewportIdToOrdinality(TargetViewport));
	}
}
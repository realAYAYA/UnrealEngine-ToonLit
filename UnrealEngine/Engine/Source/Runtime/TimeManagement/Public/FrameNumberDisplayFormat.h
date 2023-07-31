// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FrameNumberDisplayFormat.generated.h"

UENUM()
enum class EFrameNumberDisplayFormats : uint8
{
	/* Non-Drop Frame Timecode */
	NonDropFrameTimecode UMETA(DisplayName="NDF Timecode"),
	
	/* Drop Frame Timecode */
	DropFrameTimecode UMETA(DisplayName="DF Timecode"),
	
	Seconds,
	
	Frames,
	
	MAX_Count UMETA(Hidden)
};
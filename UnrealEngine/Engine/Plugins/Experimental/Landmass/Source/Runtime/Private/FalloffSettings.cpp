// Copyright Epic Games, Inc. All Rights Reserved.

#include "FalloffSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FalloffSettings)

FLandmassFalloffSettings::FLandmassFalloffSettings()
	: FalloffMode(EBrushFalloffMode::Angle)
	, FalloffAngle(45.0f)
	, FalloffWidth(1024.0f)
	, EdgeOffset(0)
	, ZOffset(0)
{

}


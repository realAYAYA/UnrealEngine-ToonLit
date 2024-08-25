// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ECastRayTracedShadow
{
	enum Type : int;
};

// Public ManyLights interface
namespace ManyLights
{
	bool IsEnabled();

	bool IsUsingClosestHZB();
	bool IsUsingGlobalSDF();
	bool IsUsingLightFunctions();

	bool IsLightSupported(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow);
	bool UseHardwareRayTracing();
	bool UseInlineHardwareRayTracing();
};
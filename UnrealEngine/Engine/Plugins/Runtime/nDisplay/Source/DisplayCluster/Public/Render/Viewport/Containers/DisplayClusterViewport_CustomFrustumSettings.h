// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterViewport_Enums.h"

/**
* Custom frustum settings of viewport
*/
struct FDisplayClusterViewport_CustomFrustumSettings
{
	// Enable custom frustum
	bool bEnabled = false;

	// Enable adaptive resolution
	bool bAdaptResolution = false;

	// Units type of values
	EDisplayClusterViewport_FrustumUnit Unit = EDisplayClusterViewport_FrustumUnit::Pixels;

	// custum frustum values
	float Left = 0;
	float Right = 0;
	float Top = 0;
	float Bottom = 0;
};

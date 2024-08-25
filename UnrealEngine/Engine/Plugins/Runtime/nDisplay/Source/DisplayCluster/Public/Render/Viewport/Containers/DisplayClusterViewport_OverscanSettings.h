// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterViewport_Enums.h"

/**
* Overscan settings of viewport
*/
struct FDisplayClusterViewport_OverscanSettings
{
	// Enable overscan
	bool bEnabled = false;

	// Set to True to render at the overscan resolution, set to false to render at the resolution in the configuration and scale for overscan
	bool bOversize = false;

	// Units type of overscan values
	EDisplayClusterViewport_FrustumUnit Unit = EDisplayClusterViewport_FrustumUnit::Pixels;

	// Overscan values
	float Left = 0;
	float Right = 0;
	float Top = 0;
	float Bottom = 0;
};

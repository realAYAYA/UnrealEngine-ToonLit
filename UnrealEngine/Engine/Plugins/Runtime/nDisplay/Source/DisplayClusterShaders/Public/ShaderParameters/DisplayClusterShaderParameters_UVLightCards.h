// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FPrimitiveSceneProxy;

/** A data struct that contains parameters for performing the UV light card render pass */
struct FDisplayClusterShaderParameters_UVLightCards
{
	/** A list of primitive scene proxies to render */
	TArray<FPrimitiveSceneProxy*> PrimitivesToRender;

	/** The size of the plane in world units to project the UV space onto */
	float ProjectionPlaneSize;

	/** The gamma to apply to the light cards when gamma correcting them */
	float LightCardGamma = 1.0;

	/** Indicates whether to render the final color of the light cards or not */
	bool bRenderFinalColor;
};

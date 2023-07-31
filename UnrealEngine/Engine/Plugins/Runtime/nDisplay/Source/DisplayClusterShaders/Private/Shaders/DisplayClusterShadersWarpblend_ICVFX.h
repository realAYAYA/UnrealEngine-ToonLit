// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHICommandList.h"

class FDisplayClusterShaderParameters_ICVFX;
struct FDisplayClusterShaderParameters_WarpBlend;


class FDisplayClusterShadersWarpblend_ICVFX
{
public:
	static bool RenderWarpBlend_ICVFX(FRHICommandListImmediate& RHICmdList, const FDisplayClusterShaderParameters_WarpBlend& InWarpBlendParameters, const FDisplayClusterShaderParameters_ICVFX& InICVFXParameters);
};

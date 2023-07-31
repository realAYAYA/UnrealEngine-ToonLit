// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================

=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "Math/SHMath.h"
#include "ScenePrivate.h"

namespace MobileReflectionEnvironmentCapture
{
	void ComputeAverageBrightness(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture, float* OutAverageBrightness);
	FRDGTexture* FilterReflectionEnvironment(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture, FSHVectorRGB3* OutIrradianceEnvironmentMap);
	void CopyToSkyTexture(FRDGBuilder& GraphBuilder, FScene* Scene, FRDGTexture* CubemapTexture, FTexture* ProcessedTexture);
}

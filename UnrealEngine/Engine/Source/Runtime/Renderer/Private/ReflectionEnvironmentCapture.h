// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Functionality for capturing the scene into reflection capture cubemaps, and prefiltering
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Math/SHMath.h"
#include "RHI.h"
#include "GlobalShader.h"
#include "RenderGraphDefinitions.h"

extern void ComputeDiffuseIrradiance(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* LightingSource,  FSHVectorRGB3* OutIrradianceEnvironmentMap);

FMatrix CalcCubeFaceViewRotationMatrix(ECubeFace Face);
FMatrix GetCubeProjectionMatrix(float HalfFovDeg, float CubeMapSize, float NearPlane);

inline uint32 GetNumMips(uint32 MipSize)
{
	return FMath::CeilLogTwo(MipSize) + 1;
}
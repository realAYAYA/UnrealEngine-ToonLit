// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BlueNoise.cpp: Resources for Blue-Noise vectors on the GPU.
=============================================================================*/

#include "BlueNoise.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "SystemTextures.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FBlueNoise, "BlueNoise");

FBlueNoiseParameters GetBlueNoiseDummyParameters()
{
	FBlueNoiseParameters Out;
	Out.Dimensions = FIntVector(1,1,1);
	Out.ModuloMasks = FIntVector(1,1,1);
	Out.ScalarTexture = GSystemTextures.BlackDummy->GetRHI();
	Out.Vec2Texture = GSystemTextures.BlackDummy->GetRHI();
	return Out;
}

FBlueNoiseParameters GetBlueNoiseParameters()
{
	FBlueNoiseParameters Out;
	check(GEngine);
	check(GEngine->BlueNoiseScalarTexture && GEngine->BlueNoiseVec2Texture);

	Out.Dimensions = FIntVector(
		GEngine->BlueNoiseScalarTexture->GetSizeX(),
		GEngine->BlueNoiseScalarTexture->GetSizeX(),
		GEngine->BlueNoiseScalarTexture->GetSizeY() / FMath::Max<int32>(1, GEngine->BlueNoiseScalarTexture->GetSizeX()));

	Out.ModuloMasks = FIntVector(
		(1u << FMath::FloorLog2(Out.Dimensions.X)) - 1,
		(1u << FMath::FloorLog2(Out.Dimensions.Y)) - 1,
		(1u << FMath::FloorLog2(Out.Dimensions.Z)) - 1);

	check((Out.ModuloMasks.X + 1)  == Out.Dimensions.X
		&&(Out.ModuloMasks.Y + 1) == Out.Dimensions.Y
		&&(Out.ModuloMasks.Z + 1) == Out.Dimensions.Z);

	Out.ScalarTexture = GEngine->BlueNoiseScalarTexture->GetResource()->TextureRHI;
	Out.Vec2Texture = GEngine->BlueNoiseVec2Texture->GetResource()->TextureRHI;
	return Out;
}

FBlueNoise GetBlueNoiseGlobalParameters()
{
	FBlueNoise Out;
	Out.BlueNoise = GetBlueNoiseParameters();
	return Out;
}
// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BlueNoise.cpp: Resources for Blue-Noise vectors on the GPU.
=============================================================================*/

#include "BlueNoise.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FBlueNoise, "BlueNoise");

FBlueNoise GetBlueNoiseParameters()
{
	FBlueNoise BlueNoise;
	check(GEngine);
	check(GEngine->BlueNoiseScalarTexture && GEngine->BlueNoiseVec2Texture);

	BlueNoise.Dimensions = FIntVector(
		GEngine->BlueNoiseScalarTexture->GetSizeX(),
		GEngine->BlueNoiseScalarTexture->GetSizeX(),
		GEngine->BlueNoiseScalarTexture->GetSizeY() / FMath::Max<int32>(1, GEngine->BlueNoiseScalarTexture->GetSizeX()));

	BlueNoise.ModuloMasks = FIntVector(
		(1u << FMath::FloorLog2(BlueNoise.Dimensions.X)) - 1,
		(1u << FMath::FloorLog2(BlueNoise.Dimensions.Y)) - 1,
		(1u << FMath::FloorLog2(BlueNoise.Dimensions.Z)) - 1);

	check((BlueNoise.ModuloMasks.X + 1) == BlueNoise.Dimensions.X
		&& (BlueNoise.ModuloMasks.Y + 1) == BlueNoise.Dimensions.Y
		&& (BlueNoise.ModuloMasks.Z + 1) == BlueNoise.Dimensions.Z);

	BlueNoise.ScalarTexture = GEngine->BlueNoiseScalarTexture->GetResource()->TextureRHI;
	BlueNoise.Vec2Texture = GEngine->BlueNoiseVec2Texture->GetResource()->TextureRHI;
	return BlueNoise;
}
// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
/**
 * 
 */
class TEXTUREGRAPHENGINE_API MathUtils
{
public:
	MathUtils() = delete;

	static const float					GMeterToCm;

	static float						MinFloat(); 
	static float						MaxFloat(); 
	
	static FVector						MinFVector();
	static FVector						MaxFVector();
	static FVector2f					MinFVector2();
	static FVector2f					MaxFVector2();
	static void						UpdateBounds(FBox& bounds, const FVector& point);
	static void						EncapsulateBound(FBox& bounds, FBox& otherBounds);
	static FVector						GetDirection(float yzAngle, float xAngle, int xSign = 1);
	static FBox						GetCombinedBounds(TArray<FBox> inputBounds);
};

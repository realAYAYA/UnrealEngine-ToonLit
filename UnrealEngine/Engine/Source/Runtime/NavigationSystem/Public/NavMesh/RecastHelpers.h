// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Coord system utilities
 *
 * Translates between Unreal and Recast coords.
 * Unreal: x, y, z
 * Recast: -x, z, -y
 */

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "Math/Box.h"
#include "Math/Color.h"
#include "Math/Matrix.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Math/UnrealMathSSE.h"
#endif
#include "Math/Vector.h"


extern NAVIGATIONSYSTEM_API FVector Unreal2RecastPoint(const FVector::FReal* UnrealPoint);
UE_DEPRECATED(5.0, "UnrealPoint should now be a FReal pointer!")
extern NAVIGATIONSYSTEM_API FVector Unreal2RecastPoint(const float* UnrealPoint);
extern NAVIGATIONSYSTEM_API FVector Unreal2RecastPoint(const FVector& UnrealPoint);
extern NAVIGATIONSYSTEM_API FBox Unreal2RecastBox(const FBox& UnrealBox);
extern NAVIGATIONSYSTEM_API FMatrix Unreal2RecastMatrix();

extern NAVIGATIONSYSTEM_API FVector Recast2UnrealPoint(const FVector::FReal* RecastPoint);
UE_DEPRECATED(5.0, "RecastPoint should now be a FReal pointer!")
extern NAVIGATIONSYSTEM_API FVector Recast2UnrealPoint(const float* RecastPoint);
extern NAVIGATIONSYSTEM_API FVector Recast2UnrealPoint(const FVector& RecastPoint);

extern NAVIGATIONSYSTEM_API FBox Recast2UnrealBox(const FVector::FReal* RecastMin, const FVector::FReal* RecastMax);
UE_DEPRECATED(5.0, "RecastMin and RecastMax should now be a FReal pointers!")
extern NAVIGATIONSYSTEM_API FBox Recast2UnrealBox(const float* RecastMin, const float* RecastMax);
extern NAVIGATIONSYSTEM_API FBox Recast2UnrealBox(const FBox& RecastBox);
extern NAVIGATIONSYSTEM_API FMatrix Recast2UnrealMatrix();
extern NAVIGATIONSYSTEM_API FColor Recast2UnrealColor(const unsigned int RecastColor);

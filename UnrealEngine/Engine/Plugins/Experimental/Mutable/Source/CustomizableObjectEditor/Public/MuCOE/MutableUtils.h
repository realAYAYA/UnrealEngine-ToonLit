// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Vector2D.h"

class USkeletalMesh;
class UStaticMesh;


/** Returns the mesh UV. */
TArray<FVector2f> GetUV(const USkeletalMesh& SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, const int32 UVIndex);


/** Returns the mesh UV. */
TArray<FVector2f> GetUV(const UStaticMesh& StaticMesh, const int32 LODIndex, const int32 SectionIndex, const int32 UVIndex);


/** Returns if the given point is inside the 0-1 bounds. */
bool HasNormalizedBounds(const FVector2f& Point);


/** Check if a given LOD and material in a skeletal mesh is closed. */
bool IsMeshClosed(const USkeletalMesh* Mesh, int LOD, int MaterialIndex);


/** Check if a given LOD and material in a skeletal mesh is closed. */
bool IsMeshClosed(const UStaticMesh* Mesh, int LOD, int MaterialIndex);

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#endif

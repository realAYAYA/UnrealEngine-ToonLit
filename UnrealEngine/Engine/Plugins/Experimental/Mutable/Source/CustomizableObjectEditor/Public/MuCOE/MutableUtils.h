// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "HAL/Platform.h"
#include "Math/Vector2D.h"


/** Returns the mesh UV. */
TArray<FVector2f> GetUV(const USkeletalMesh& SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, const int32 UVIndex);


/** Returns the mesh UV. */
TArray<FVector2f> GetUV(const UStaticMesh& StaticMesh, const int32 LODIndex, const int32 SectionIndex, const int32 UVIndex);


/** Returns if the given point is inside the 0-1 bounds. */
bool HasNormalizedBounds(const FVector2f& Point);


/** Returns if the given UV is normalized. */
bool IsUVNormalized(const USkeletalMesh& SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, const int32 UVIndex);


/** Returns if the given UV is normalized. */
bool IsUVNormalized(const UStaticMesh& SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, const int32 UVIndex);


/** Check if a given LOD and material in a skeletal mesh is closed. */
bool IsMeshClosed(const USkeletalMesh* Mesh, int LOD, int MaterialIndex);


/** Check if a given LOD and material in a skeletal mesh is closed. */
bool IsMeshClosed(const UStaticMesh* Mesh, int LOD, int MaterialIndex);

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/ObjectMacros.h"
#include "ClothCollisionPrim.generated.h"

/** Data for a single sphere primitive in the clothing simulation. This can either be a 
 *  sphere on its own, or part of a capsule referenced by the indices in FClothCollisionPrim_Capsule
 */
USTRUCT()
struct FClothCollisionPrim_Sphere
{
	GENERATED_BODY()

	FClothCollisionPrim_Sphere(float InRadius = 0.0f, const FVector& InLocalPosition = FVector::ZeroVector, int32 InBoneIndex = INDEX_NONE)
		: BoneIndex(InBoneIndex)
		, Radius(InRadius)
		, LocalPosition(InLocalPosition)
	{}

	UPROPERTY()
	int32 BoneIndex;

	UPROPERTY()
	float Radius;

	UPROPERTY()
	FVector LocalPosition;
};

/** Data for a single connected sphere primitive. This should be configured after all spheres have
 *  been processed as they are really just indexing the existing spheres
 */
USTRUCT()
struct FClothCollisionPrim_SphereConnection
{
	GENERATED_BODY()

	FClothCollisionPrim_SphereConnection(int32 SphereIndex0 = INDEX_NONE, int32 SphereIndex1 = INDEX_NONE)
	{
		SphereIndices[0] = SphereIndex0;
		SphereIndices[1] = SphereIndex1;
	}

	UPROPERTY()
	int32 SphereIndices[2];
};

/** Data for a convex face. */
USTRUCT()
struct FClothCollisionPrim_ConvexFace
{
	GENERATED_BODY()

	FClothCollisionPrim_ConvexFace(): Plane(ForceInit) {}

	UPROPERTY()
	FPlane Plane;

	UPROPERTY()
	TArray<int32> Indices;
};

/**
 *	Data for a single convex element
 *	A convex is a collection of planes, in which the clothing will attempt to stay outside of the
 *	shape created by the planes combined.
 */
USTRUCT()
struct FClothCollisionPrim_Convex
{
	GENERATED_BODY()

	FClothCollisionPrim_Convex()
		: BoneIndex(INDEX_NONE)
	{}

	FClothCollisionPrim_Convex(
		TArray<FClothCollisionPrim_ConvexFace>&& InFaces,
		TArray<FVector>&& InSurfacePoints,
		int32 InBoneIndex = INDEX_NONE)
		: Faces(InFaces)
		, SurfacePoints(InSurfacePoints)
		, BoneIndex(InBoneIndex)
	{}

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FPlane> Planes_DEPRECATED;
#endif

	UPROPERTY()
	TArray<FClothCollisionPrim_ConvexFace> Faces;

	UPROPERTY()
	TArray<FVector> SurfacePoints;  // Surface points, used by Chaos and also for visualization

	UPROPERTY()
	int32 BoneIndex;
};

/** Data for a single box primitive. */
USTRUCT()
struct FClothCollisionPrim_Box
{
	GENERATED_BODY()

	FClothCollisionPrim_Box(
		const FVector& InLocalPosition = FVector::ZeroVector,
		const FQuat& InLocalRotation = FQuat::Identity,
		const FVector& InHalfExtents = FVector::ZeroVector,
		int32 InBoneIndex = INDEX_NONE)
		: LocalPosition(InLocalPosition)
		, LocalRotation(InLocalRotation)
		, HalfExtents(InHalfExtents)
		, BoneIndex(InBoneIndex)
	{}

	UPROPERTY()
	FVector LocalPosition;

	UPROPERTY()
	FQuat LocalRotation;

	UPROPERTY()
	FVector HalfExtents;

	UPROPERTY()
	int32 BoneIndex;
};

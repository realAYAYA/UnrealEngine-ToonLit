// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimToTextureSkeletalMesh.h"
#include "CoreMinimal.h"

namespace AnimToTexture_Private
{

// Static to Skeletal Mapping
struct FVertexToMeshMapping
{
	// Closest SkeletalMesh Triangle
	FIntVector3 Triangle;
	// SkeletalMesh Triangle Barycentric Coordinates
	FVector3f  BarycentricCoords;
	// SkeletalMesh Triangle Inverted Matrix
	FMatrix44f InvMatrix;
	
	/* Transforms Point with given Triangle */
	FVector3f TransformPosition(const FVector3f& Point, const FVector3f& PointA, const FVector3f& PointB, const FVector3f& PointC) const;
	
	/* Transforms Vector with given Triangle */
	FVector3f TransformVector(const FVector3f& Vector, const FVector3f& PointA, const FVector3f& PointB, const FVector3f& PointC) const;
	
	/* Interpolate VertexSkinWeights with Barycentric Coordinates */
	void InterpolateVertexSkinWeights(const VertexSkinWeightMax& A, const VertexSkinWeightMax& B, const VertexSkinWeightMax& C,
		VertexSkinWeightMax& OutWeights) const;
	
	/* Computes closest point to triangle*/
	static FVector3f FindClosestPointToTriangle(const FVector3f& Point, const FVector3f& PointA, const FVector3f& PointB, const FVector3f& PointC);

	/* Computes Barycentric coordinates from point to triangle */
	static FVector3f BarycentricCoordinates(const FVector3f& Point, const FVector3f& PointA, const FVector3f& PointB, const FVector3f& PointC);

	/* Computes Triangle Normal */
	static FVector3f GetTriangleNormal(const FVector3f& PointA, const FVector3f& PointB, const FVector3f& PointC);

	/* Computes Triangle Matrix */
	static FMatrix44f GetTriangleMatrix(const FVector3f& PointA, const FVector3f& PointB, const FVector3f& PointC);

	/* Finds closest triangle, point on triangle and barycentric coordinates */
	static int32 FindClosestTriangle(const FVector3f& Point, const TArray<FVector3f>& Vertices, const TArray<FIntVector3>& Triangles,
		FVector3f& OutClosestPoint, FVector3f& OutBarycentricCoords);

	// Creates Mapping between StaticMesh and SkeletalMesh
	static void Create(const UStaticMesh* StaticMesh, const int32 StaticMeshLODIndex,
		const USkeletalMesh* SkeletalMesh, const int32 SkeletalMeshLODIndex,
		TArray<FVertexToMeshMapping>& OutMapping);

	/* Interpolate SkinWeights with VertexToMeshMapping */
	static void InterpolateSkinWeights(const TArray<FVertexToMeshMapping>& Mapping, const TArray<VertexSkinWeightMax>& SkinWeights,
		TArray<VertexSkinWeightMax>& OutSkinWeights);
};

} // end namespace AnimToTexture_Private

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "StaticMeshResources.h"


class FDisplayClusterWarpBlendMath_WarpMesh
{
public:
	FDisplayClusterWarpBlendMath_WarpMesh(const FStaticMeshLODResources& InMeshLODResources)
		: MeshLODResources(InMeshLODResources)
	{ }

public:
	FBox CalcAABBox()
	{
		FBox AABBox = FBox(FVector(FLT_MAX, FLT_MAX, FLT_MAX), FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX));

		const FPositionVertexBuffer& PositionBuffer = MeshLODResources.VertexBuffers.PositionVertexBuffer;
		for (uint32 VertIdx = 0; VertIdx < PositionBuffer.GetNumVertices(); ++VertIdx)
		{
			const FVector& Pts = (FVector)PositionBuffer.VertexPosition(VertIdx);

			AABBox.Min.X = FMath::Min(AABBox.Min.X, Pts.X);
			AABBox.Min.Y = FMath::Min(AABBox.Min.Y, Pts.Y);
			AABBox.Min.Z = FMath::Min(AABBox.Min.Z, Pts.Z);

			AABBox.Max.X = FMath::Max(AABBox.Max.X, Pts.X);
			AABBox.Max.Y = FMath::Max(AABBox.Max.Y, Pts.Y);
			AABBox.Max.Z = FMath::Max(AABBox.Max.Z, Pts.Z);
		}

		return AABBox;
	}

	void CalcSurfaceVectors(FVector& OutSurfaceViewNormal, FVector& OutSurfaceViewPlane)
	{
		// Calc static normal and plane
		const int32 IdxNum = MeshLODResources.IndexBuffer.GetNumIndices();
		const int32 TriNum = IdxNum / 3;

		if (TriNum <= 0)
		{
			return;
		}

		FVector SurfaceNormalViewSum = FVector::ZeroVector;
		const FPositionVertexBuffer& PositionBuffer = MeshLODResources.VertexBuffers.PositionVertexBuffer;

		for (int32 TriIdx = 0; TriIdx < TriNum; ++TriIdx)
		{
			const int32 Index0 = MeshLODResources.IndexBuffer.GetIndex(TriIdx * 3 + 0);
			const int32 Index1 = MeshLODResources.IndexBuffer.GetIndex(TriIdx * 3 + 1);
			const int32 Index2 = MeshLODResources.IndexBuffer.GetIndex(TriIdx * 3 + 2);

			const FVector& Pts1 = (FVector)PositionBuffer.VertexPosition(Index0);
			const FVector& Pts0 = (FVector)PositionBuffer.VertexPosition(Index1);
			const FVector& Pts2 = (FVector)PositionBuffer.VertexPosition(Index2);

			const FVector N1 = Pts1 - Pts0;
			const FVector N2 = Pts2 - Pts0;

			const FVector Normal = FVector::CrossProduct(N2, N1);
			const double Magnitude = Normal.Length();

			SurfaceNormalViewSum += (Normal * Magnitude);
		}

		OutSurfaceViewNormal = SurfaceNormalViewSum.GetSafeNormal();

		//@todo: MeshSurfaceViewPlane not implemented, use MeshSurfaceViewNormal
		// this is for the case when normals are set in the side direction to have same sampling quality over the multiple heavily curved surfaces
		OutSurfaceViewPlane = OutSurfaceViewNormal;
	}

private:
	const FStaticMeshLODResources& MeshLODResources;
};

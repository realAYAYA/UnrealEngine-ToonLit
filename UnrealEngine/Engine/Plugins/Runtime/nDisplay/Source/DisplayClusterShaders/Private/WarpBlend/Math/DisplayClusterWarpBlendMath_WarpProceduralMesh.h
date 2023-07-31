// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ProceduralMeshComponent.h"

class FDisplayClusterWarpBlendMath_WarpProceduralMesh
{
public:
	FDisplayClusterWarpBlendMath_WarpProceduralMesh(const FProcMeshSection& InProcMeshSection)
		: ProcMeshSection(InProcMeshSection)
	{ }

public:
	FBox CalcAABBox()
	{
		FBox AABBox = FBox(FVector(FLT_MAX, FLT_MAX, FLT_MAX), FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX));

		for (const FProcMeshVertex& VertexIt : ProcMeshSection.ProcVertexBuffer)
		{
			const FVector& Pts = VertexIt.Position;

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
		const int32 IdxNum = ProcMeshSection.ProcIndexBuffer.Num();
		const int32 TriNum = IdxNum / 3;

		if (TriNum <= 0)
		{
			return;
		}

		// Calc static normal and plane
		bool bAverageNormalValid = false;
		FVector AverageNormal;

		for (int32 TriIdx = 0; TriIdx < TriNum; ++TriIdx)
		{
			const uint32& Index0 = ProcMeshSection.ProcIndexBuffer[TriIdx * 3 + 0];
			const uint32& Index1 = ProcMeshSection.ProcIndexBuffer[TriIdx * 3 + 1];
			const uint32& Index2 = ProcMeshSection.ProcIndexBuffer[TriIdx * 3 + 2];

			const FVector& Pts0 = ProcMeshSection.ProcVertexBuffer[Index0].Position;
			const FVector& Pts1 = ProcMeshSection.ProcVertexBuffer[Index1].Position;
			const FVector& Pts2 = ProcMeshSection.ProcVertexBuffer[Index2].Position;

			const FVector N1 = Pts0 - Pts1;
			const FVector N2 = Pts2 - Pts1;

			const FVector N = FVector::CrossProduct(N2, N1).GetSafeNormal();

			AverageNormal = bAverageNormalValid ? (N + AverageNormal).GetSafeNormal() : N;

			bAverageNormalValid = true;
		}

		OutSurfaceViewNormal = AverageNormal;

		//@todo: MeshSurfaceViewPlane not implemented, use MeshSurfaceViewNormal
		OutSurfaceViewPlane = OutSurfaceViewNormal;
	}

private:
	const FProcMeshSection& ProcMeshSection;
};

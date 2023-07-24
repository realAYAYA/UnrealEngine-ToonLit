// Copyright Epic Games, Inc. All Rights Reserved.
#include "AnimToTextureMeshMapping.h"

namespace AnimToTexture_Private
{

FVector3f FVertexToMeshMapping::FindClosestPointToTriangle(const FVector3f& P, const FVector3f& A, const FVector3f& B, const FVector3f& C)
{
	const FVector3f AB = B - A;
	const FVector3f AC = C - A;
	const FVector3f AP = P - A;

	const float D1 = FVector3f::DotProduct(AB, AP);
	const float D2 = FVector3f::DotProduct(AC, AP);
	if (D1 <= 0.f && D2 <= 0.f)
	{
		return A;
	}

	const FVector3f BP = P - B;
	const float D3 = FVector3f::DotProduct(AB, BP);
	const float D4 = FVector3f::DotProduct(AC, BP);
	if (D3 >= 0.f && D4 <= D3)
	{
		return B;
	}

	const FVector3f CP = P - C;
	const float D5 = FVector3f::DotProduct(AB, CP);
	const float D6 = FVector3f::DotProduct(AC, CP);
	if (D6 >= 0.f && D5 <= D6)
	{
		return C;
	}

	const float VC = D1 * D4 - D3 * D2;
	if (VC <= 0.f && D1 >= 0.f && D3 <= 0.f)
	{
		const float V = D1 / (D1 - D3);
		return A + V * AB;
	}

	const float VB = D5 * D2 - D1 * D6;
	if (VB <= 0.f && D2 >= 0.f && D6 <= 0.f)
	{
		const float V = D2 / (D2 - D6);
		return A + V * AC;
	}

	const float VA = D3 * D6 - D5 * D4;
	if (VA <= 0.f && (D4 - D3) >= 0.f && (D5 - D6) >= 0.f)
	{
		const float V = (D4 - D3) / ((D4 - D3) + (D5 - D6));
		return B + V * (C - B);
	}

	const float Denom = 1.0f / (VA + VB + VC);
	const float V = VB * Denom;
	const float W = VC * Denom;

	return A + V * AB + W * AC;
}

FVector3f FVertexToMeshMapping::GetTriangleNormal(const FVector3f& A, const FVector3f& B, const FVector3f& C)
{
	const FVector3f V0 = B - A;
	const FVector3f V1 = C - A;
	return FVector3f::CrossProduct(V0, V1).GetSafeNormal();
}

FMatrix44f FVertexToMeshMapping::GetTriangleMatrix(const FVector3f& A, const FVector3f& B, const FVector3f& C)
{	
	const FVector3f V0 = (B - A).GetSafeNormal();
	const FVector3f V1 = GetTriangleNormal(A, B, C);
	const FVector3f V2 = FVector3f::CrossProduct(V0, V1).GetSafeNormal();
	
	FMatrix44f Matrix = FMatrix44f::Identity; // SetAxes doesn't set [3] elements.
	Matrix.SetAxes(&V0, &V1, &V2, &A);

	return Matrix;
}

FVector3f FVertexToMeshMapping::BarycentricCoordinates(const FVector3f& P, const FVector3f& A, const FVector3f& B, const FVector3f& C)
{
	const FVector3f V0 = B - A;
	const FVector3f V1 = C - A;
	const FVector3f V2 = P - A;
	const float D00 = FVector3f::DotProduct(V0, V0);
	const float D01 = FVector3f::DotProduct(V0, V1);
	const float D11 = FVector3f::DotProduct(V1, V1);
	const float D20 = FVector3f::DotProduct(V2, V0);
	const float D21 = FVector3f::DotProduct(V2, V1);
	const float Denom = 1.0f / (D00 * D11 - D01 * D01);
	
	const float V = (D11 * D20 - D01 * D21) * Denom;
	const float W = (D00 * D21 - D01 * D20) * Denom;
	const float U = 1.0f - V - W;

	return FVector3f(U, V, W);
}

int32 FVertexToMeshMapping::FindClosestTriangle(const FVector3f& Point, const TArray<FVector3f>& Vertices, const TArray<FIntVector3>& Triangles,
	FVector3f& OutClosestPoint, FVector3f& OutBarycentricCoords)
{
	int32 ClosestTriangle = INDEX_NONE;
	float ClosestDistance = TNumericLimits<float>::Max();

	for (int32 TriangleIndex = 0; TriangleIndex < Triangles.Num(); TriangleIndex++)
	{
		const FIntVector3& Triangle = Triangles[TriangleIndex];

		// Check if valid indices
		if (Vertices.IsValidIndex(Triangle.X) && 
			Vertices.IsValidIndex(Triangle.Y) && 
			Vertices.IsValidIndex(Triangle.Z))
		{
			const FVector3f& A = Vertices[Triangle.X];
			const FVector3f& B = Vertices[Triangle.Y];
			const FVector3f& C = Vertices[Triangle.Z];

			const FVector3f ClosestPoint = FindClosestPointToTriangle(Point, A, B, C);
			const float Distance = FVector3f::Distance(Point, ClosestPoint);
			if (Distance < ClosestDistance)
			{
				OutClosestPoint = ClosestPoint;
				ClosestTriangle = TriangleIndex;
				ClosestDistance = Distance;
			}
		}
	}
	check(ClosestTriangle != INDEX_NONE);

	// Compute Barycentric Coordinates
	const FIntVector3& Triangle = Triangles[ClosestTriangle];
	const FVector3f& A = Vertices[Triangle.X];
	const FVector3f& B = Vertices[Triangle.Y];
	const FVector3f& C = Vertices[Triangle.Z];
	OutBarycentricCoords = BarycentricCoordinates(OutClosestPoint, A, B, C);

	return ClosestTriangle;
}

//int32 FindClosestVertex(const FVector3f& Point, const TArray<FVector3f>& Vertices)
//{
//	float MinDistance = TNumericLimits<float>::Max();
//	int32 ClosestIndex = INDEX_NONE;
//
//	// Find Closest SkeletalMesh Vertex.
//	for (int32 VertexIndex = 0; VertexIndex < Vertices.Num(); ++VertexIndex)
//	{
//		const float Distance = FVector3f::Dist(Point, Vertices[VertexIndex]);
//		if (Distance < MinDistance)
//		{
//			MinDistance = Distance;
//			ClosestIndex = VertexIndex;
//		}
//	}
//
//	return ClosestIndex;
//}


void FVertexToMeshMapping::Create(
	const UStaticMesh* StaticMesh, const int32 StaticMeshLODIndex,	
	const USkeletalMesh* SkeletalMesh, const int32 SkeletalMeshLODIndex,
	TArray<FVertexToMeshMapping>& OutMapping)
{
	check(StaticMesh);
	check(SkeletalMesh);
	OutMapping.Reset();

	// Get StaticMesh Vertices
	TArray<FVector3f> StaticMeshVertices;
	TArray<FVector3f> StaticMeshNormals;
	const int32 NumStaticMeshVertices = GetVertices(StaticMesh, StaticMeshLODIndex, StaticMeshVertices, StaticMeshNormals);

	// Get SkeletalMesh Vertices
	TArray<FVector3f> SkeletalMeshVertices;
	const int32 NumSkeletalMeshVertices = GetVertices(SkeletalMesh, SkeletalMeshLODIndex, SkeletalMeshVertices);

	// Get SkeletalMesh Triangles
	TArray<FIntVector3> SkeletalMeshTriangles;
	GetTriangles(SkeletalMesh, SkeletalMeshLODIndex, SkeletalMeshTriangles);

	// Allocate
	OutMapping.SetNumUninitialized(NumStaticMeshVertices);

	// Fill Out Mapping
	for (int32 VertexIndex = 0; VertexIndex < NumStaticMeshVertices; VertexIndex++)
	{
		const FVector3f& StaticMeshVertex = StaticMeshVertices[VertexIndex];

		FVector3f ClosestPoint;
		FVector3f BarycentricCoords;

		// Find Closest Triangle (on SkeletalMesh)
		const int32 ClosestTriangle = FindClosestTriangle(StaticMeshVertex, SkeletalMeshVertices, SkeletalMeshTriangles, ClosestPoint, BarycentricCoords);
		check(SkeletalMeshTriangles.IsValidIndex(ClosestTriangle));
		
		// Closest Triangle Vertices
		const FIntVector3& Triangle = SkeletalMeshTriangles[ClosestTriangle];
		const FVector3f& A = SkeletalMeshVertices[Triangle.X];
		const FVector3f& B = SkeletalMeshVertices[Triangle.Y];
		const FVector3f& C = SkeletalMeshVertices[Triangle.Z];
		
		// Compute Triangle Matrix
		const FMatrix44f Matrix = GetTriangleMatrix(A, B, C);
		
		// Store Mapping 
		OutMapping[VertexIndex].Triangle = Triangle;
		OutMapping[VertexIndex].BarycentricCoords = BarycentricCoords;
		OutMapping[VertexIndex].InvMatrix = Matrix.Inverse();
	}
}


void FVertexToMeshMapping::InterpolateVertexSkinWeights(const VertexSkinWeightMax& A, const VertexSkinWeightMax& B, const VertexSkinWeightMax& C,
	VertexSkinWeightMax& OutWeights) const
{	
	// NOTE: this could use NumBones and remove the 256 limit.
	TStaticArray<float, 256> DenseA(InPlace, 0.f);
	TStaticArray<float, 256> DenseB(InPlace, 0.f);
	TStaticArray<float, 256> DenseC(InPlace, 0.f);

	// Convert to Dense Arrays	
	for (int32 Index = 0; Index < MAX_TOTAL_INFLUENCES; Index++)
	{
		if (A.BoneWeights[Index])
		{
			DenseA[A.MeshBoneIndices[Index]] = (float)A.BoneWeights[Index] / 255.f;
		}
		if (B.BoneWeights[Index])
		{
			DenseB[B.MeshBoneIndices[Index]] = (float)B.BoneWeights[Index] / 255.f;
		}
		if (C.BoneWeights[Index])
		{
			DenseC[C.MeshBoneIndices[Index]] = (float)C.BoneWeights[Index] / 255.f;
		}
	}

	// Blend Dense Arrays
	TArray<TPair<uint8, uint16>> SortedVertexSkinWeights;
	SortedVertexSkinWeights.SetNumUninitialized(256);

	for (int32 Index = 0; Index < 256; Index++)
	{
		const float BoneWeightA = DenseA[Index] * BarycentricCoords.X;
		const float BoneWeightB = DenseB[Index] * BarycentricCoords.Y;
		const float BoneWeightC = DenseC[Index] * BarycentricCoords.Z;

		const uint8 BoneWeight = (uint8)FMath::RoundToInt((BoneWeightA + BoneWeightB + BoneWeightC) * 255.f);
		SortedVertexSkinWeights[Index] = TPair<uint8, uint16>(BoneWeight, Index);
	}

	// Sort Weights and return Sparse
	SortedVertexSkinWeights.Sort();
	for (int32 Index = 0; Index < MAX_TOTAL_INFLUENCES; Index++)
	{
		OutWeights.BoneWeights[Index]     = SortedVertexSkinWeights[255 - Index].Key;
		OutWeights.MeshBoneIndices[Index] = SortedVertexSkinWeights[255 - Index].Value;
	}
}

void FVertexToMeshMapping::InterpolateSkinWeights(const TArray<FVertexToMeshMapping>& Mapping, const TArray<VertexSkinWeightMax>& SkinWeights,
	TArray<VertexSkinWeightMax>& OutSkinWeights)
{
	// Allocate StaticMesh Weights
	OutSkinWeights.SetNumUninitialized(Mapping.Num());

	for (int32 VertexIndex = 0; VertexIndex < Mapping.Num(); VertexIndex++)
	{
		const FVertexToMeshMapping& VertexMapping = Mapping[VertexIndex];
		const FIntVector3& Triangle = VertexMapping.Triangle;

		if (SkinWeights.IsValidIndex(Triangle.X) &&
			SkinWeights.IsValidIndex(Triangle.Y) &&
			SkinWeights.IsValidIndex(Triangle.Z))
		{
			const VertexSkinWeightMax& A = SkinWeights[Triangle.X];
			const VertexSkinWeightMax& B = SkinWeights[Triangle.Y];
			const VertexSkinWeightMax& C = SkinWeights[Triangle.Z];

			Mapping[VertexIndex].InterpolateVertexSkinWeights(A, B, C, OutSkinWeights[VertexIndex]);
		}
	}
}

FVector3f FVertexToMeshMapping::TransformPosition(const FVector3f& P, const FVector3f& A, const FVector3f& B, const FVector3f& C) const
{	
	const FMatrix44f Matrix = GetTriangleMatrix(A, B, C);	
	return Matrix.TransformPosition(InvMatrix.TransformPosition(P));
}

FVector3f FVertexToMeshMapping::TransformVector(const FVector3f& V, const FVector3f& A, const FVector3f& B, const FVector3f& C) const
{
	const FMatrix44f Matrix = GetTriangleMatrix(A, B, C);
	return Matrix.TransformVector(InvMatrix.TransformVector(V));
}

} // end namespace AnimToTexture_Private

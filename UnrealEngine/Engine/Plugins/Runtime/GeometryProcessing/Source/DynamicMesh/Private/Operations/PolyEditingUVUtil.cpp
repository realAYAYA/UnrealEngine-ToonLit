// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operations/PolyEditingUVUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Parameterization/DynamicMeshUVEditor.h"


using namespace UE::Geometry;


void UE::Geometry::ComputeArbitraryTrianglePatchUVs(
	FDynamicMesh3& Mesh, 
	FDynamicMeshUVOverlay& UVOverlay,
	const TArray<int32>& TriangleSet)
{
	TArray<int32> NbrTriSet;
	double NbrUVAreaSum = 0.0;
	double Nbr3DAreaSum = 0.0;
	for (int32 tid : TriangleSet)
	{
		FIndex3i NbrTris = Mesh.GetTriNeighbourTris(tid);
		for (int32 j = 0; j < 3; ++j)
		{
			if (NbrTris[j] != IndexConstants::InvalidID && NbrTriSet.Contains(NbrTris[j]) == false)
			{
				NbrTriSet.Add(NbrTris[j]);
				if (UVOverlay.IsSetTriangle(NbrTris[j]))
				{
					FVector3d A, B, C;
					Mesh.GetTriVertices(NbrTris[j], A, B, C);
					Nbr3DAreaSum += VectorUtil::Area(A,B,C);
					FVector2f U, V, W;
					UVOverlay.GetTriElements(NbrTris[j], U, V, W);
					NbrUVAreaSum += (double)VectorUtil::Area(U, V, W);
				}
			}
		}
	}

	double UseUVScale = FMathd::Max(FMathd::Sqrt(NbrUVAreaSum),0.0001) / FMathd::Max(FMathd::Sqrt(Nbr3DAreaSum),0.0001);

	FDynamicMeshUVEditor UVEditor(&Mesh, &UVOverlay);
	FUVEditResult UVEditResult;
	UVEditor.SetTriangleUVsFromExpMap(TriangleSet, FDynamicMeshUVEditor::FExpMapOptions(), &UVEditResult);

	UVEditor.TransformUVElements(UVEditResult.NewUVElements, [&](const FVector2f& UV)
	{
		return (float)UseUVScale * UV;
	});
}
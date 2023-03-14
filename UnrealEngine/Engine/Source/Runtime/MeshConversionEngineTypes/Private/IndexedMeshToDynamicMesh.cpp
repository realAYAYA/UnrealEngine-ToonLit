// Copyright Epic Games, Inc. All Rights Reserved. 

#include "IndexedMeshToDynamicMesh.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"

using namespace UE::Geometry;

void UE::Conversion::RenderBuffersToDynamicMesh(
	const FStaticMeshVertexBuffers& VertexData,
	const FRawStaticIndexBuffer& IndexData,
	const FStaticMeshSectionArray& SectionData,
	FDynamicMesh3& MeshOut,
	bool bAttemptToWeldSeams)
{
	const FStaticMeshVertexBuffer& VertexBuffer = VertexData.StaticMeshVertexBuffer;
	const FPositionVertexBuffer& PositionBuffer = VertexData.PositionVertexBuffer;
	const FColorVertexBuffer& ColorBuffer = VertexData.ColorVertexBuffer;

	int32 NumUVChannels = VertexBuffer.GetNumTexCoords();
	bool bHasUVs = (NumUVChannels > 0);
	NumUVChannels = FMath::Max(1, NumUVChannels);

	MeshOut.Clear();

	MeshOut.EnableAttributes();
	FDynamicMeshNormalOverlay* Normals = MeshOut.Attributes()->PrimaryNormals();
	MeshOut.Attributes()->EnableTangents();
	FDynamicMeshNormalOverlay* Tangents = MeshOut.Attributes()->PrimaryTangents();
	FDynamicMeshNormalOverlay* BiTangents = MeshOut.Attributes()->PrimaryBiTangents();

	MeshOut.Attributes()->SetNumUVLayers(NumUVChannels);
	TArray<FDynamicMeshUVOverlay*> UVs;
	for (int32 ui = 0; ui < NumUVChannels; ++ui)
	{
		UVs.Add(MeshOut.Attributes()->GetUVLayer(ui));
	}
	MeshOut.Attributes()->EnableMaterialID();
	FDynamicMeshMaterialAttribute* MaterialID = MeshOut.Attributes()->GetMaterialID();

	int32 NumVertices = PositionBuffer.GetNumVertices();
	for (int32 vi = 0; vi < NumVertices; ++vi)
	{
		FVector3f Position = PositionBuffer.VertexPosition(vi);
		int32 vid = MeshOut.AppendVertex(FVector3d(Position));
		ensure(vid == vi);

		FVector4f Normal = VertexBuffer.VertexTangentZ(vi);
		int32 nid = Normals->AppendElement(&Normal.X);
		ensure(nid == vi);

		FVector3f Tangent = VertexBuffer.VertexTangentX(vi);
		int32 tid = Tangents->AppendElement(&Tangent.X);
		ensure(tid == vi);

		FVector3f BiTangent = VertexBuffer.VertexTangentY(vi);
		int32 bid = BiTangents->AppendElement(&BiTangent.X);
		ensure(bid == vi);

		for (int32 ui = 0; ui < NumUVChannels; ++ui)
		{
			FVector2f UV = (bHasUVs) ? VertexBuffer.GetVertexUV(vi, ui) : FVector2f::ZeroVector;
			int32 uvid = UVs[ui]->AppendElement(UV);
			ensure(uvid == vi);
		}
	}


	int32 NumTriangles = IndexData.GetNumIndices() / 3;
	for (int32 ti = 0; ti < NumTriangles; ++ti)
	{
		int32 a = IndexData.GetIndex(3 * ti);
		int32 b = IndexData.GetIndex(3 * ti + 1);
		int32 c = IndexData.GetIndex(3 * ti + 2);
		int32 tid = MeshOut.AppendTriangle(a, b, c);
		ensure(tid == ti);
		Normals->SetTriangle(tid, FIndex3i(a, b, c));
		Tangents->SetTriangle(tid, FIndex3i(a, b, c));
		BiTangents->SetTriangle(tid, FIndex3i(a, b, c));
		for (int32 ui = 0; ui < NumUVChannels; ++ui)
		{
			UVs[ui]->SetTriangle(tid, FIndex3i(a, b, c));
		}
	}

	for (const FStaticMeshSection& Section : SectionData)
	{
		int32 MatIndex = Section.MaterialIndex;
		for (uint32 k = 0; k < Section.NumTriangles; ++k)
		{
			uint32 TriIdx = (Section.FirstIndex/3) + k;
			MaterialID->SetValue((int32)TriIdx, MatIndex);
		}
	}


	if (bAttemptToWeldSeams)
	{
		FMergeCoincidentMeshEdges Merge(&MeshOut);
		Merge.Apply();
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "StaticMeshLODResourcesToDynamicMesh.h"
#include "StaticMeshLODResourcesAdapter.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"


using namespace UE::Geometry;

bool FStaticMeshLODResourcesToDynamicMesh::Convert(
	const FStaticMeshLODResources* StaticMeshResources,
	const ConversionOptions& Options,
	FDynamicMesh3& OutputMesh)
{
	FStaticMeshLODResourcesMeshAdapter Adapter(StaticMeshResources);

	Adapter.SetBuildScale(Options.BuildScale, false);

	OutputMesh = FDynamicMesh3();
	OutputMesh.EnableTriangleGroups();
	if (Options.bWantNormals || Options.bWantTangents || Options.bWantUVs || Options.bWantVertexColors || Options.bWantMaterialIDs)
	{
		OutputMesh.EnableAttributes();
	}

	// Copy vertices. LODMesh is dense so this should be 1-1
	int32 VertexCount = Adapter.VertexCount();
	for ( int32 VertID = 0; VertID < VertexCount; ++VertID )
	{
		FVector3d Position = Adapter.GetVertex(VertID);
		int NewVertID = OutputMesh.AppendVertex(Position);
		if (NewVertID != VertID)
		{
			OutputMesh.Clear();
			ensure(false);
			return false;
		}
	}

	// Copy triangles. LODMesh is dense so this should be 1-1 unless there is a duplicate tri or non-manifold edge (currently aborting in that case)
	int32 TriangleCount = Adapter.TriangleCount();
	for (int32 TriID = 0; TriID < TriangleCount; ++TriID)
	{
		FIndex3i Tri = Adapter.GetTriangle(TriID);
		int32 NewTriID = OutputMesh.AppendTriangle(Tri.A, Tri.B, Tri.C);
		if (NewTriID != TriID)
		{
			OutputMesh.Clear();
			ensure(false);
			return false;
		}

	}

	// transfer sections to PolyGroups and MaterialIDs
	FDynamicMeshMaterialAttribute* MaterialIDs = nullptr;
	if (Options.bWantMaterialIDs)
	{
		OutputMesh.Attributes()->EnableMaterialID();
		MaterialIDs = OutputMesh.Attributes()->GetMaterialID();
	}
	for ( int32 SectionIdx = 0; SectionIdx < StaticMeshResources->Sections.Num(); ++SectionIdx)
	{
		const FStaticMeshSection& Section = StaticMeshResources->Sections[SectionIdx];
		for (uint32 TriIdx = 0; TriIdx < Section.NumTriangles; ++TriIdx)
		{
			int32 TriangleID = (int32)(Section.FirstIndex/3 + TriIdx);
			OutputMesh.SetTriangleGroup(TriangleID, SectionIdx);
			if (MaterialIDs != nullptr)
			{
				MaterialIDs->SetValue(TriangleID, Section.MaterialIndex);
			}
		}
	}

	// copy overlay normals
	if (Adapter.HasNormals() && Options.bWantNormals)
	{
		FDynamicMeshNormalOverlay* Normals = OutputMesh.Attributes()->PrimaryNormals();
		if (Normals != nullptr)
		{
			for (int32 VertID = 0; VertID < VertexCount; ++VertID)
			{
				FVector3f N = Adapter.GetNormal(VertID);
				int32 ElemID = Normals->AppendElement(N);
				check(ElemID == VertID);
			}

			for (int32 TriID = 0; TriID < TriangleCount; ++TriID)
			{
				FIndex3i Tri = Adapter.GetTriangle(TriID);
				Normals->SetTriangle(TriID, FIndex3i(Tri.A, Tri.B, Tri.C));
			}
		}
	}

	// copy overlay tangents
	if ( Adapter.HasNormals() && Options.bWantTangents )
	{
		OutputMesh.Attributes()->EnableTangents();
		FDynamicMeshNormalOverlay* TangentsX = OutputMesh.Attributes()->PrimaryTangents();
		if (TangentsX != nullptr)
		{
			for (int32 TriID = 0; TriID < TriangleCount; ++TriID)
			{
				FVector3f T1, T2, T3;
				Adapter.GetTriTangentsX<FVector3f>(TriID, T1, T2, T3);
				int32 a = TangentsX->AppendElement(T1);
				int32 b = TangentsX->AppendElement(T2);
				int32 c = TangentsX->AppendElement(T3);
				TangentsX->SetTriangle(TriID, FIndex3i(a, b, c));
			}
		}

		FDynamicMeshNormalOverlay* TangentsY = OutputMesh.Attributes()->PrimaryBiTangents();
		if (TangentsY != nullptr)
		{
			for (int32 TriID = 0; TriID < TriangleCount; ++TriID)
			{
				FVector3f T1, T2, T3;
				Adapter.GetTriTangentsY<FVector3f>(TriID, T1, T2, T3);
				int32 a = TangentsY->AppendElement(T1);
				int32 b = TangentsY->AppendElement(T2);
				int32 c = TangentsY->AppendElement(T3);
				TangentsY->SetTriangle(TriID, FIndex3i(a, b, c));
			}
		}
	}

	// copy UV layers
	if (Adapter.HasUVs() && Options.bWantUVs)
	{
		int32 NumUVLayers = Adapter.NumUVLayers();
		if (NumUVLayers > 0)
		{
			OutputMesh.Attributes()->SetNumUVLayers(NumUVLayers);
			for (int32 UVLayerIndex = 0; UVLayerIndex < NumUVLayers; ++UVLayerIndex)
			{
				FDynamicMeshUVOverlay* UVOverlay = OutputMesh.Attributes()->GetUVLayer(UVLayerIndex);
				for (int32 TriID = 0; TriID < TriangleCount; ++TriID)
				{
					FVector2f UV1, UV2, UV3;
					Adapter.GetTriUVs<FVector2f>(TriID, UVLayerIndex, UV1, UV2, UV3);
					int32 a = UVOverlay->AppendElement(UV1);
					int32 b = UVOverlay->AppendElement(UV2);
					int32 c = UVOverlay->AppendElement(UV3);
					UVOverlay->SetTriangle(TriID, FIndex3i(a, b, c));
				}
			}
		}
	}

	// copy overlay colors
	if ( Adapter.HasColors() && Options.bWantVertexColors )
	{
		OutputMesh.Attributes()->EnablePrimaryColors();
		FDynamicMeshColorOverlay* Colors = OutputMesh.Attributes()->PrimaryColors();
		for (int32 TriID = 0; TriID < TriangleCount; ++TriID)
		{
			FColor C1, C2, C3;
			Adapter.GetTriColors(TriID, C1, C2, C3);
			// [TODO] should we be doing RGBA conversion here?
			int32 a = Colors->AppendElement( C1.ReinterpretAsLinear()  );
			int32 b = Colors->AppendElement( C2.ReinterpretAsLinear() );
			int32 c = Colors->AppendElement( C3.ReinterpretAsLinear() );
			Colors->SetTriangle(TriID, FIndex3i(a, b, c));
		}
	}

	return true;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshQueryFunctions.h"
#include "UDynamicMesh.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "MeshQueries.h"
#include "MeshBoundaryLoops.h"
#include "Selections/MeshConnectedComponents.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshQueryFunctions)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshQueryFunctions"

template<typename ReturnType> 
ReturnType SimpleMeshQuery(UDynamicMesh* Mesh, ReturnType DefaultValue, 
	TFunctionRef<ReturnType(const FDynamicMesh3& Mesh)> QueryFunc)
{
	if (Mesh)
	{
		ReturnType RetVal;
		Mesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			RetVal = QueryFunc(ReadMesh);
		});
		return RetVal;
	}
	return DefaultValue;
}


template<typename ReturnType> 
ReturnType SimpleMeshUVSetQuery(UDynamicMesh* Mesh, int32 UVSetIndex, bool& bIsValidUVSet, ReturnType DefaultValue, 
	TFunctionRef<ReturnType(const FDynamicMesh3& Mesh, const FDynamicMeshUVOverlay& UVOverlay)> QueryFunc)
{
	bIsValidUVSet = false;
	ReturnType RetVal = DefaultValue;
	if (Mesh)
	{
		Mesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			if (ReadMesh.HasAttributes() && UVSetIndex < ReadMesh.Attributes()->NumUVLayers() )
			{
				const FDynamicMeshUVOverlay* Overlay = ReadMesh.Attributes()->GetUVLayer(UVSetIndex);
				if (Overlay != nullptr)
				{
					bIsValidUVSet = true;
					RetVal = QueryFunc(ReadMesh, *ReadMesh.Attributes()->GetUVLayer(UVSetIndex));
				}
			}
		});
	}
	return RetVal;
}




FString UGeometryScriptLibrary_MeshQueryFunctions::GetMeshInfoString(UDynamicMesh* TargetMesh)
{
	return SimpleMeshQuery<FString>(TargetMesh, FString(TEXT("Mesh is Null")), [&](const FDynamicMesh3& Mesh) { return Mesh.MeshInfoString(); });
}


bool UGeometryScriptLibrary_MeshQueryFunctions::GetIsDenseMesh( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<bool>(TargetMesh, true, [&](const FDynamicMesh3& Mesh) { return Mesh.IsCompact(); });
}

bool UGeometryScriptLibrary_MeshQueryFunctions::GetMeshHasAttributeSet( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<bool>(TargetMesh, true, [&](const FDynamicMesh3& Mesh) { return Mesh.HasAttributes(); });
}

FBox UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<FBox>(TargetMesh, FBox(EForceInit::ForceInit), [&](const FDynamicMesh3& Mesh) { return (FBox)Mesh.GetBounds(true); });
}

void UGeometryScriptLibrary_MeshQueryFunctions::GetMeshVolumeArea( UDynamicMesh* TargetMesh, float& SurfaceArea, float& Volume )
{
	SurfaceArea = Volume = 0;
	SimpleMeshQuery<bool>(TargetMesh, false, [&](const FDynamicMesh3& Mesh) {
		FVector2d VolArea = TMeshQueries<FDynamicMesh3>::GetVolumeArea(Mesh);
		Volume = VolArea.X;
		SurfaceArea = VolArea.Y;
		return true;
	});
}

bool UGeometryScriptLibrary_MeshQueryFunctions::GetIsClosedMesh( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<bool>(TargetMesh, true, [&](const FDynamicMesh3& Mesh) { return Mesh.IsClosed(); });
}

int32 UGeometryScriptLibrary_MeshQueryFunctions::GetNumOpenBorderEdges( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<int32>(TargetMesh, 0, [&](const FDynamicMesh3& Mesh) { 
		int32 Count = 0;
		for (int32 eid : Mesh.EdgeIndicesItr())
		{
			if (Mesh.IsBoundaryEdge(eid))
			{
				Count++;
			}
		}
		return Count;
	});
}

int32 UGeometryScriptLibrary_MeshQueryFunctions::GetNumOpenBorderLoops( UDynamicMesh* TargetMesh, bool& bAmbiguousTopologyFound )
{
	return SimpleMeshQuery<int32>(TargetMesh, 0, [&](const FDynamicMesh3& Mesh) { 
		FMeshBoundaryLoops BoundaryLoops(&Mesh, false);
		bAmbiguousTopologyFound = (BoundaryLoops.Compute() == false);
		return BoundaryLoops.GetLoopCount();
	});
}


int32 UGeometryScriptLibrary_MeshQueryFunctions::GetNumConnectedComponents( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<int32>(TargetMesh, 0, [&](const FDynamicMesh3& Mesh) { 
		FMeshConnectedComponents Components(&Mesh);
		Components.FindConnectedTriangles();
		return Components.Num();
	});
}



int32 UGeometryScriptLibrary_MeshQueryFunctions::GetNumTriangleIDs( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<int32>(TargetMesh, 0, [&](const FDynamicMesh3& Mesh) { return Mesh.MaxTriangleID(); });
}

bool UGeometryScriptLibrary_MeshQueryFunctions::GetHasTriangleIDGaps( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<bool>(TargetMesh, false, [&](const FDynamicMesh3& Mesh) { return !Mesh.IsCompactT(); });
}

bool UGeometryScriptLibrary_MeshQueryFunctions::IsValidTriangleID( UDynamicMesh* TargetMesh, int32 TriangleID )
{
	return SimpleMeshQuery<bool>(TargetMesh, false, [&](const FDynamicMesh3& Mesh) { return Mesh.IsTriangle(TriangleID); });
}

UDynamicMesh* UGeometryScriptLibrary_MeshQueryFunctions::GetAllTriangleIDs(UDynamicMesh* TargetMesh, FGeometryScriptIndexList& TriangleIDList, bool& bHasTriangleIDGaps)
{
	TriangleIDList.Reset(EGeometryScriptIndexType::Triangle);
	bHasTriangleIDGaps = false;
	if (TargetMesh)
	{
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			TriangleIDList.List->Reserve(ReadMesh.TriangleCount());
			for (int32 tid : ReadMesh.TriangleIndicesItr())
			{
				TriangleIDList.List->Add(tid);
			}
			bHasTriangleIDGaps = ! ReadMesh.IsCompactT();
		});
	}
	return TargetMesh;
}

FIntVector UGeometryScriptLibrary_MeshQueryFunctions::GetTriangleIndices(UDynamicMesh* TargetMesh, int32 TriangleID, bool& bIsValidTriangle)
{
	return SimpleMeshQuery<FIntVector>(TargetMesh, FIntVector::NoneValue, [&](const FDynamicMesh3& Mesh) {
		bIsValidTriangle = Mesh.IsTriangle(TriangleID);
		return (bIsValidTriangle) ? (FIntVector)Mesh.GetTriangle(TriangleID) : FIntVector::NoneValue;
	});
}

UDynamicMesh* UGeometryScriptLibrary_MeshQueryFunctions::GetAllTriangleIndices(UDynamicMesh* TargetMesh, FGeometryScriptTriangleList& TriangleList, bool bSkipGaps, bool& bHasTriangleIDGaps)
{
	TriangleList.Reset();
	TArray<FIntVector>& Triangles = *TriangleList.List;
	bHasTriangleIDGaps = false;
	if (TargetMesh)
	{
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			if (bSkipGaps)
			{
				Triangles.Reserve(ReadMesh.TriangleCount());
				for (int32 tid : ReadMesh.TriangleIndicesItr())
				{
					Triangles.Add( (FIntVector)ReadMesh.GetTriangle(tid) );
				}
				bHasTriangleIDGaps = false;
			}
			else
			{
				Triangles.Init(FIntVector::NoneValue, ReadMesh.MaxTriangleID());
				for (int32 tid : ReadMesh.TriangleIndicesItr())
				{
					Triangles[tid] = (FIntVector)ReadMesh.GetTriangle(tid);
				}
				bHasTriangleIDGaps = ! ReadMesh.IsCompactT();
			}
		});
	}

	return TargetMesh;
}


void UGeometryScriptLibrary_MeshQueryFunctions::GetTrianglePositions(UDynamicMesh* TargetMesh, int32 TriangleID, bool& bIsValidTriangle, FVector& Vertex1, FVector& Vertex2, FVector& Vertex3)
{
	bIsValidTriangle = SimpleMeshQuery<bool>(TargetMesh, false, [&](const FDynamicMesh3& Mesh) 
	{
		bIsValidTriangle = Mesh.IsTriangle(TriangleID);
		if (bIsValidTriangle)
		{
			Mesh.GetTriVertices<FVector>(TriangleID, Vertex1, Vertex2, Vertex3);
		}
		else
		{
			Vertex1 = Vertex2 = Vertex3 = FVector::ZeroVector;
		}
		return bIsValidTriangle;
	});
}

FVector UGeometryScriptLibrary_MeshQueryFunctions::GetTriangleFaceNormal(UDynamicMesh* TargetMesh, int32 TriangleID, bool& bIsValidTriangle)
{
	return SimpleMeshQuery<FVector>(TargetMesh, FVector::ZeroVector, [&](const FDynamicMesh3& Mesh) {
		bIsValidTriangle = Mesh.IsTriangle(TriangleID);
		return (bIsValidTriangle) ? (FVector)Mesh.GetTriNormal(TriangleID) : FVector::ZeroVector;
	});
}



int32 UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<int32>(TargetMesh, 0, [&](const FDynamicMesh3& Mesh) { return Mesh.VertexCount(); });
}

int32 UGeometryScriptLibrary_MeshQueryFunctions::GetNumVertexIDs( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<int32>(TargetMesh, 0, [&](const FDynamicMesh3& Mesh) { return Mesh.MaxVertexID(); });
}

bool UGeometryScriptLibrary_MeshQueryFunctions::GetHasVertexIDGaps( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<bool>(TargetMesh, false, [&](const FDynamicMesh3& Mesh) { return !Mesh.IsCompactV(); });
}

bool UGeometryScriptLibrary_MeshQueryFunctions::IsValidVertexID( UDynamicMesh* TargetMesh, int32 VertexID )
{
	return SimpleMeshQuery<bool>(TargetMesh, false, [&](const FDynamicMesh3& Mesh) { return Mesh.IsVertex(VertexID); });
}

UDynamicMesh* UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(UDynamicMesh* TargetMesh, FGeometryScriptIndexList& VertexIDList, bool& bHasVertexIDGaps)
{
	VertexIDList.Reset(EGeometryScriptIndexType::Vertex);
	bHasVertexIDGaps = false;
	if (TargetMesh)
	{
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			VertexIDList.List->Reserve(ReadMesh.VertexCount());
			for (int32 vid : ReadMesh.VertexIndicesItr())
			{
				VertexIDList.List->Add(vid);
			}
			bHasVertexIDGaps = ! ReadMesh.IsCompactV();
		});
	}
	return TargetMesh;
}

FVector UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(UDynamicMesh* TargetMesh, int32 VertexID, bool& bIsValidVertex)
{
	return SimpleMeshQuery<FVector>(TargetMesh, FVector::ZeroVector, [&](const FDynamicMesh3& Mesh) {
		bIsValidVertex = Mesh.IsVertex(VertexID);
		return (bIsValidVertex) ? (FVector)Mesh.GetVertex(VertexID) : FVector::ZeroVector;
	});
}

UDynamicMesh* UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexPositions(UDynamicMesh* TargetMesh, FGeometryScriptVectorList& PositionList, bool bSkipGaps, bool& bHasVertexIDGaps)
{
	PositionList.Reset();
	TArray<FVector>& VertexPositions = *PositionList.List;
	bHasVertexIDGaps = false;
	if (TargetMesh)
	{
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			if (bSkipGaps)
			{
				VertexPositions.Reserve(ReadMesh.VertexCount());
				for (int32 vid : ReadMesh.VertexIndicesItr())
				{
					VertexPositions.Add( (FVector)ReadMesh.GetVertex(vid) );
				}
				bHasVertexIDGaps = false;
			}
			else
			{
				VertexPositions.Init(FVector::ZeroVector, ReadMesh.MaxVertexID());
				for (int32 vid : ReadMesh.VertexIndicesItr())
				{
					VertexPositions[vid] = (FVector)ReadMesh.GetVertex(vid);
				}
				bHasVertexIDGaps = ! ReadMesh.IsCompactV();
			}
		});
	}
	
	return TargetMesh;
}





int32 UGeometryScriptLibrary_MeshQueryFunctions::GetNumUVSets( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<int32>(TargetMesh, 0, [&](const FDynamicMesh3& Mesh) { 
		return Mesh.HasAttributes() ? Mesh.Attributes()->NumUVLayers() : 0; });
}

FBox2D UGeometryScriptLibrary_MeshQueryFunctions::GetUVSetBoundingBox(UDynamicMesh* TargetMesh, int UVSetIndex, bool& bIsValidUVSet, bool& bUVSetIsEmpty)
{
	return SimpleMeshUVSetQuery<FBox2D>(TargetMesh, UVSetIndex, bIsValidUVSet, FBox2D(EForceInit::ForceInit), 
		[&](const FDynamicMesh3& Mesh, const FDynamicMeshUVOverlay& UVSet)
	{
		FAxisAlignedBox2d Bounds = FAxisAlignedBox2d::Empty();
		int32 ValidTriCount = 0;
		for (int32 TriangleID : Mesh.TriangleIndicesItr())
		{
			FVector2f A, B, C;
			if (UVSet.IsSetTriangle(TriangleID))
			{
				UVSet.GetTriElements(TriangleID, A, B, C);
				Bounds.Contain((FVector2d)A); Bounds.Contain((FVector2d)B); Bounds.Contain((FVector2d)C);
				ValidTriCount++;
			}
		}
		bUVSetIsEmpty = (ValidTriCount == 0);
		return (ValidTriCount > 0) ? (FBox2D)Bounds : FBox2D(EForceInit::ForceInit);
	});
}

void UGeometryScriptLibrary_MeshQueryFunctions::GetTriangleUVs(UDynamicMesh* TargetMesh, int32 UVSetIndex, int32 TriangleID, 
	FVector2D& UV1, FVector2D& UV2, FVector2D& UV3, bool& bHaveValidUVs)
{
	bHaveValidUVs = SimpleMeshUVSetQuery<bool>(TargetMesh, UVSetIndex, bHaveValidUVs, false, 
		[&](const FDynamicMesh3& Mesh, const FDynamicMeshUVOverlay& UVSet) 
	{
		if (Mesh.IsTriangle(TriangleID) && UVSet.IsSetTriangle(TriangleID))
		{
			FVector2f A, B, C;
			UVSet.GetTriElements(TriangleID, A, B, C);
			UV1 = (FVector2D)A; UV2 = (FVector2D)B; UV3 = (FVector2D)C;
			return true;
		}
		else
		{
			UV1 = UV2 = UV3 = FVector2D::ZeroVector;
			return false;
		}
	});
}





bool UGeometryScriptLibrary_MeshQueryFunctions::GetHasMaterialIDs( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<bool>(TargetMesh, false, [&](const FDynamicMesh3& Mesh) {
		return (Mesh.HasAttributes() && Mesh.Attributes()->HasMaterialID() && Mesh.Attributes()->GetMaterialID() != nullptr);
	});
}

bool UGeometryScriptLibrary_MeshQueryFunctions::GetHasPolygroups( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<bool>(TargetMesh, false, [&](const FDynamicMesh3& Mesh) {
		return Mesh.HasTriangleGroups();
	});
}

int UGeometryScriptLibrary_MeshQueryFunctions::GetNumExtendedPolygroupLayers( UDynamicMesh* TargetMesh )
{
	return SimpleMeshQuery<int>(TargetMesh, 0, [&](const FDynamicMesh3& Mesh) {
		return Mesh.HasAttributes() ? Mesh.Attributes()->NumPolygroupLayers() : 0;
	});
}



#undef LOCTEXT_NAMESPACE


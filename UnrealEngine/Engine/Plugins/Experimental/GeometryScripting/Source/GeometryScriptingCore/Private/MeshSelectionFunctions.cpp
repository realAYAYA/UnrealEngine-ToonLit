// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshSelectionFunctions.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "Selections/GeometrySelection.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "SphereTypes.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Spatial/FastWinding.h"
#include "Selections/MeshConnectedComponents.h"
#include "Selections/MeshFaceSelection.h"
#include "Selections/MeshVertexSelection.h"

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshSelectionFunctions"





void UGeometryScriptLibrary_MeshSelectionFunctions::GetMeshSelectionInfo(
	FGeometryScriptMeshSelection Selection,
	EGeometryScriptMeshSelectionType& SelectionType,
	int& NumSelected)
{
	SelectionType = Selection.GetSelectionType();
	NumSelected = Selection.GetNumSelected();
}


void UGeometryScriptLibrary_MeshSelectionFunctions::DebugPrintMeshSelection(
	FGeometryScriptMeshSelection Selection,
	bool bDisable)
{
	if (!bDisable)
	{
		Selection.DebugPrint();
	}
}


UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection& Selection,
	EGeometryScriptMeshSelectionType SelectionType)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("CreateSelectAllMeshSelection: TargetMesh is Null"));
		return TargetMesh;
	}
	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		FGeometrySelection NewSelection;
		if (SelectionType == EGeometryScriptMeshSelectionType::Vertices)
		{
			NewSelection.InitializeTypes(EGeometryElementType::Vertex, EGeometryTopologyType::Triangle);
			for (int32 vid : ReadMesh.VertexIndicesItr())
			{
				NewSelection.Selection.Add(FGeoSelectionID::MeshVertex(vid).Encoded());
			}
		}
		else if (SelectionType == EGeometryScriptMeshSelectionType::Triangles)
		{
			NewSelection.InitializeTypes(EGeometryElementType::Face, EGeometryTopologyType::Triangle);
			for (int32 tid : ReadMesh.TriangleIndicesItr())
			{
				NewSelection.Selection.Add(FGeoSelectionID::MeshTriangle(tid).Encoded());
			}
		}
		else
		{
			NewSelection.InitializeTypes(EGeometryElementType::Face, EGeometryTopologyType::Polygroup);
			TSet<int32> UniqueGroupIDs;
			for (int32 tid : ReadMesh.TriangleIndicesItr())
			{
				int32 GroupID = ReadMesh.GetTriangleGroup(tid);
				if (UniqueGroupIDs.Contains(GroupID) == false)
				{
					NewSelection.Selection.Add(FGeoSelectionID::GroupFace(tid, GroupID).Encoded());
					UniqueGroupIDs.Add(GroupID);
				}
			}
		}
		Selection.SetSelection(MoveTemp(NewSelection));
	});
	return TargetMesh;
}

void UGeometryScriptLibrary_MeshSelectionFunctions::CombineMeshSelections(
	FGeometryScriptMeshSelection SelectionA,
	FGeometryScriptMeshSelection SelectionB,
	FGeometryScriptMeshSelection& ResultSelectionOut,
	EGeometryScriptCombineSelectionMode CombineMode)
{
	if (SelectionA.GetSelectionType() != SelectionB.GetSelectionType())
	{
		UE_LOG(LogGeometry, Warning, TEXT("CombineMeshSelections: Selections have different types, cannot combine"));
	}
	ResultSelectionOut.SetSelection(SelectionA);
	ResultSelectionOut.CombineSelectionInPlace(SelectionB, CombineMode);
}



UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::ConvertMeshSelection(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection FromSelection,
	FGeometryScriptMeshSelection& ToSelection,
	EGeometryScriptMeshSelectionType NewType,
	bool bAllowPartialInclusion)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ConvertMeshSelection: TargetMesh is Null"));
		return TargetMesh;
	}
	if (FromSelection.GetSelectionType() == NewType)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ConvertMeshSelection: Trying to convert to same type"));
		ToSelection.SetSelection(FromSelection);
		return TargetMesh;
	}

	if (NewType == EGeometryScriptMeshSelectionType::Vertices)
	{
		TSet<int32> CurTriangles, CurVertices;
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			if (bAllowPartialInclusion)
			{
				FromSelection.ProcessByTriangleID(ReadMesh, [&](int32 TriangleID) { 
					FIndex3i Vertices = ReadMesh.GetTriangle(TriangleID);
					CurVertices.Add(Vertices.A); CurVertices.Add(Vertices.B); CurVertices.Add(Vertices.C);
				});
				ConvertIndexSetToMeshSelection(TargetMesh, CurVertices, EGeometryScriptMeshSelectionType::Vertices, ToSelection);
			}
			else
			{
				FromSelection.ProcessByTriangleID(ReadMesh, [&](int32 TriangleID) { 
					CurTriangles.Add(TriangleID);
					FIndex3i Vertices = ReadMesh.GetTriangle(TriangleID);
					CurVertices.Add(Vertices.A); CurVertices.Add(Vertices.B); CurVertices.Add(Vertices.C);
				});

				FGeometrySelection NewSelection;
				NewSelection.InitializeTypes(EGeometryElementType::Vertex, EGeometryTopologyType::Triangle);
				for (int32 vid : CurVertices)
				{
					bool bAllInSet = true;
					ReadMesh.EnumerateVertexTriangles(vid, [&](int32 tid) { bAllInSet = bAllInSet && CurTriangles.Contains(tid); });
					if (bAllInSet)
					{
						NewSelection.Selection.Add(FGeoSelectionID::MeshVertex(vid).Encoded());
					}
				}
				ToSelection.SetSelection(MoveTemp(NewSelection));
			}
		});

	}
	else if (NewType == EGeometryScriptMeshSelectionType::Triangles)
	{
		FGeometrySelection NewSelection;
		NewSelection.InitializeTypes(EGeometryElementType::Face, EGeometryTopologyType::Triangle);

		if (FromSelection.GetSelectionType() == EGeometryScriptMeshSelectionType::Polygroups || bAllowPartialInclusion)
		{
			TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
			{
				FromSelection.ProcessByTriangleID(ReadMesh, [&](int32 TriangleID) {
					NewSelection.Selection.Add(FGeoSelectionID::MeshTriangle(TriangleID).Encoded());
				});
			});
		}
		else   // vertex selection w/ no partial inclusion, ie only "full" triangles
		{
			TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
			{
				// in this case FromSelection already has this set! but we do not have access to it...
				TSet<int32> CurVertices;
				FromSelection.ProcessByVertexID(ReadMesh, [&](int32 VertexID) {
					CurVertices.Add(VertexID);
				});
				FromSelection.ProcessByTriangleID(ReadMesh, [&](int32 TriangleID) {
					FIndex3i Triangle = ReadMesh.GetTriangle(TriangleID);
					if (CurVertices.Contains(Triangle.A) && CurVertices.Contains(Triangle.B) && CurVertices.Contains(Triangle.C))
					{
						NewSelection.Selection.Add(FGeoSelectionID::MeshTriangle(TriangleID).Encoded());
					}
				});
			});
		}

		ToSelection.SetSelection(MoveTemp(NewSelection));
	}
	else   // polygroups
	{
		if (bAllowPartialInclusion)
		{
			TSet<int32> UniqueGroupIDs;
			TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
			{
				FromSelection.ProcessByTriangleID(ReadMesh, [&](int32 TriangleID) {
					UniqueGroupIDs.Add(ReadMesh.GetTriangleGroup(TriangleID));
				});
			});
			ConvertIndexSetToMeshSelection(TargetMesh, UniqueGroupIDs, EGeometryScriptMeshSelectionType::Polygroups, ToSelection);
		}
		else
		{
			TSet<int32> UniqueGroupIDs;
			TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
			{
				TSet<int32> AllTriangles;
				FromSelection.ProcessByTriangleID(ReadMesh, [&](int32 TriangleID) {
					AllTriangles.Add(TriangleID);
					UniqueGroupIDs.Add(ReadMesh.GetTriangleGroup(TriangleID));
				});

				// if we have a non-selected triangle whose group is in our group set, that group is not fully selected
				TSet<int32> FailGroups;
				for (int32 TriangleID : ReadMesh.TriangleIndicesItr())
				{
					int32 GroupID = ReadMesh.GetTriangleGroup(TriangleID);
					if (UniqueGroupIDs.Contains(GroupID) && AllTriangles.Contains(TriangleID) == false)
					{
						FailGroups.Add(GroupID);
					}
				}
				for (int32 GroupID : FailGroups)
				{
					UniqueGroupIDs.Remove(GroupID);
				}
			});
			ConvertIndexSetToMeshSelection(TargetMesh, UniqueGroupIDs, EGeometryScriptMeshSelectionType::Polygroups, ToSelection);
		}
	}


	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexArrayToMeshSelection(
	UDynamicMesh* TargetMesh,
	const TArray<int32>& IndexArray,
	EGeometryScriptMeshSelectionType SelectionType,
	FGeometryScriptMeshSelection& SelectionOut)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ConvertIndexArrayToMeshSelection: TargetMesh is Null"));
		return TargetMesh;
	}

	FGeometrySelection GeoSelection;

	switch (SelectionType)
	{
	case EGeometryScriptMeshSelectionType::Triangles:
		for (int32 tid : IndexArray)
		{
			GeoSelection.Selection.Add( FGeoSelectionID::MeshTriangle(tid).Encoded() );
		}
		GeoSelection.InitializeTypes(EGeometryElementType::Face, EGeometryTopologyType::Triangle);
		break;

	case EGeometryScriptMeshSelectionType::Vertices:
		for (int32 vid : IndexArray)
		{
			GeoSelection.Selection.Add( FGeoSelectionID::MeshVertex(vid).Encoded() );
		}
		GeoSelection.InitializeTypes(EGeometryElementType::Vertex, EGeometryTopologyType::Triangle);
		break;

	case EGeometryScriptMeshSelectionType::Polygroups:
		for (int32 gid : IndexArray)
		{
			FGeoSelectionID HackGroupFace;
			HackGroupFace.TopologyID = (uint32)gid;
			HackGroupFace.GeometryID = 0xFFFFFFFFu;
			GeoSelection.Selection.Add( HackGroupFace.Encoded() );
		}
		GeoSelection.InitializeTypes(EGeometryElementType::Face, EGeometryTopologyType::Polygroup);
		break;

	}

	SelectionOut.SetSelection(MoveTemp(GeoSelection));
	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexSetToMeshSelection(
	UDynamicMesh* TargetMesh,
	const TSet<int32>& IndexSet,
	EGeometryScriptMeshSelectionType SelectionType,
	FGeometryScriptMeshSelection& SelectionOut)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ConvertIndexSetToMeshSelection: TargetMesh is Null"));
		return TargetMesh;
	}

	FGeometrySelection GeoSelection;

	switch (SelectionType)
	{
	case EGeometryScriptMeshSelectionType::Triangles:
		for (int32 tid : IndexSet)
		{
			GeoSelection.Selection.Add( FGeoSelectionID::MeshTriangle(tid).Encoded() );
		}
		GeoSelection.InitializeTypes(EGeometryElementType::Face, EGeometryTopologyType::Triangle);
		break;

	case EGeometryScriptMeshSelectionType::Vertices:
		for (int32 vid : IndexSet)
		{
			GeoSelection.Selection.Add( FGeoSelectionID::MeshVertex(vid).Encoded() );
		}
		GeoSelection.InitializeTypes(EGeometryElementType::Vertex, EGeometryTopologyType::Triangle);
		break;

	case EGeometryScriptMeshSelectionType::Polygroups:
		for (int32 gid : IndexSet)
		{
			FGeoSelectionID HackGroupFace;
			HackGroupFace.TopologyID = (uint32)gid;
			HackGroupFace.GeometryID = 0xFFFFFFFFu;
			GeoSelection.Selection.Add( HackGroupFace.Encoded() );
		}
		GeoSelection.InitializeTypes(EGeometryElementType::Face, EGeometryTopologyType::Polygroup);

		break;
	}

	SelectionOut.SetSelection(MoveTemp(GeoSelection));
	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::ConvertMeshSelectionToIndexArray(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection Selection,
	TArray<int32>& IndexArray,
	EGeometryScriptMeshSelectionType& SelectionType)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ConvertMeshSelectionToIndexArray: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& Mesh)
	{
		Selection.ConvertToMeshIndexArray(Mesh, IndexArray);
		SelectionType = Selection.GetSelectionType();
	});

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexListToMeshSelection(
	UDynamicMesh* TargetMesh,
	FGeometryScriptIndexList IndexList,
	EGeometryScriptMeshSelectionType SelectionType,
	FGeometryScriptMeshSelection& SelectionOut)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ConvertIndexListToMeshSelection: TargetMesh is Null"));
		return TargetMesh;
	}
	if (IndexList.IndexType == EGeometryScriptIndexType::Any)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ConvertIndexListToMeshSelection: IndexList has type Any, cannot convert"));
		return TargetMesh;
	}

	if (IndexList.IndexType == EGeometryScriptIndexType::Vertex
		|| IndexList.IndexType == EGeometryScriptIndexType::Triangle
		|| IndexList.IndexType == EGeometryScriptIndexType::PolygroupID )
	{
		EGeometryScriptMeshSelectionType InitialType =
			(IndexList.IndexType == EGeometryScriptIndexType::Vertex) ? EGeometryScriptMeshSelectionType::Vertices :
				((IndexList.IndexType == EGeometryScriptIndexType::Triangle) ? EGeometryScriptMeshSelectionType::Triangles : EGeometryScriptMeshSelectionType::Polygroups);
		if (SelectionType == InitialType)
		{
			ConvertIndexArrayToMeshSelection(TargetMesh, *IndexList.List, InitialType, SelectionOut);
		}
		else
		{
			FGeometryScriptMeshSelection TempSelection;
			ConvertIndexArrayToMeshSelection(TargetMesh, *IndexList.List, InitialType, TempSelection);
			ConvertMeshSelection(TargetMesh, TempSelection, SelectionOut, SelectionType, true);
		}
	}
	else if (IndexList.IndexType == EGeometryScriptIndexType::MaterialID)
	{
		TArray<int32> Triangles;
		const TArray<int32>& MaterialIDSelection = *IndexList.List;
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			if (ReadMesh.HasAttributes() && ReadMesh.Attributes()->HasMaterialID())
			{
				const FDynamicMeshMaterialAttribute* MaterialIDs = ReadMesh.Attributes()->GetMaterialID();
				for (int32 tid : ReadMesh.TriangleIndicesItr())
				{
					if (MaterialIDSelection.Contains(MaterialIDs->GetValue(tid)))
					{
						Triangles.Add(tid);
					}
				}
			}
		});

		if (SelectionType == EGeometryScriptMeshSelectionType::Triangles)
		{
			ConvertIndexArrayToMeshSelection(TargetMesh, Triangles, EGeometryScriptMeshSelectionType::Triangles, SelectionOut);
		}
		else
		{
			FGeometryScriptMeshSelection TempSelection;
			ConvertIndexArrayToMeshSelection(TargetMesh, Triangles, EGeometryScriptMeshSelectionType::Triangles, TempSelection);
			ConvertMeshSelection(TargetMesh, TempSelection, SelectionOut, SelectionType, true);
		}
	}

	return TargetMesh;
}




UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::ConvertMeshSelectionToIndexList(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection Selection,
	FGeometryScriptIndexList& IndexList,
	EGeometryScriptIndexType& ResultType,
	EGeometryScriptIndexType ConvertToType)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ConvertMeshSelectionToIndexList: TargetMesh is Null"));
		return TargetMesh;
	}

	ResultType = EGeometryScriptIndexType::Any;
	TArray<int32> TempArray;
	TargetMesh->ProcessMesh([&](const FDynamicMesh3& Mesh)
	{
		ResultType = Selection.ConvertToMeshIndexArray(Mesh, TempArray, ConvertToType);
	});

	IndexList.Reset(ResultType);
	*IndexList.List = MoveTemp(TempArray);

	if (ResultType == EGeometryScriptIndexType::Any)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ConvertMeshSelectionToIndexList: Conversion is not currently supported"));
	}

	return TargetMesh;
}




namespace UELocal
{

static void SelectMeshElementsWithContainmentTest(
	UDynamicMesh* TargetMesh,
	TFunctionRef<bool(const FVector3d& Position, const FVector3d& Normal)> ContainmentFunc,
	FGeometryScriptMeshSelection& SelectionOut,
	EGeometryScriptMeshSelectionType SelectionType,
	int NumTrianglePoints,
	bool bNeedsNormals)
{
	NumTrianglePoints = FMath::Clamp(NumTrianglePoints, 1, 3);
	FVector3d UnitNormal = FVector3d::UnitZ();

	FGeometrySelection GeoSelection;
	if (SelectionType == EGeometryScriptMeshSelectionType::Vertices)
	{
		GeoSelection.InitializeTypes(EGeometryElementType::Vertex, EGeometryTopologyType::Triangle);

		TargetMesh->ProcessMesh([&](const FDynamicMesh3& Mesh) 
		{
			for (int32 vid : Mesh.VertexIndicesItr())
			{
				FVector3d UseNormal = (bNeedsNormals) ? FMeshNormals::ComputeVertexNormal(Mesh,vid) : UnitNormal;
				if ( ContainmentFunc(Mesh.GetVertex(vid), UseNormal) )
				{
					GeoSelection.Selection.Add( FGeoSelectionID::MeshVertex(vid).Encoded() );
				}
			}
		});
	}
	else
	{
		GeoSelection.InitializeTypes(EGeometryElementType::Face, 
			(SelectionType == EGeometryScriptMeshSelectionType::Triangles) ? EGeometryTopologyType::Triangle : EGeometryTopologyType::Polygroup);

		TargetMesh->ProcessMesh([&](const FDynamicMesh3& Mesh) 
		{
			for (int32 tid : Mesh.TriangleIndicesItr())
			{
				FVector3d UseNormal = (bNeedsNormals) ? Mesh.GetTriNormal(tid) : UnitNormal;
				FIndex3i Tri = Mesh.GetTriangle(tid);
				// may be wasteful to test each vertex multiple times...could accumulate a cache at cost of some memory allocation...
				int NumContained =
					(ContainmentFunc(Mesh.GetVertex(Tri.A), UseNormal) ? 1 : 0) +
					(ContainmentFunc(Mesh.GetVertex(Tri.B), UseNormal) ? 1 : 0) +
					(ContainmentFunc(Mesh.GetVertex(Tri.C), UseNormal) ? 1 : 0);

				if ( NumContained >= NumTrianglePoints )
				{
					if (SelectionType == EGeometryScriptMeshSelectionType::Triangles)
					{
						GeoSelection.Selection.Add( FGeoSelectionID::MeshTriangle(tid).Encoded() );
					}
					else
					{
						int32 gid = Mesh.GetTriangleGroup(tid);
						GeoSelection.Selection.Add( FGeoSelectionID::GroupFace(tid, gid).Encoded() );
					}
				}
			}
		});

	}

	SelectionOut.SetSelection(MoveTemp(GeoSelection));
}

} // end namespace UELocal


UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsInBox(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection& SelectionOut,
	FBox Box,
	EGeometryScriptMeshSelectionType SelectionType,
	bool bInvert,
	int MinNumTrianglePoints)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("SelectMeshElementsInBox: TargetMesh is Null"));
		return TargetMesh;
	}

	FAxisAlignedBox3d Container(Box);
	auto ContainsFunc = [&Container, bInvert](const FVector3d& Point, const FVector3d& Normal) 
	{ 
		return Container.Contains(Point) != bInvert; 
	};
	UELocal::SelectMeshElementsWithContainmentTest(TargetMesh, ContainsFunc,
		SelectionOut, SelectionType, MinNumTrianglePoints, false);
	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsInSphere(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection& SelectionOut,
	FVector SphereOrigin,
	double SphereRadius,
	EGeometryScriptMeshSelectionType SelectionType,
	bool bInvert,
	int MinNumTrianglePoints)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("SelectMeshElementsInSphere: TargetMesh is Null"));
		return TargetMesh;
	}

	SphereRadius = FMathd::Clamp(SphereRadius, FMathd::Epsilon, FMathd::Sqrt(TNumericLimits<double>::Max()));
	UE::Geometry::FSphere3d Container(SphereOrigin, SphereRadius);

	auto ContainsFunc = [&Container, bInvert](const FVector3d& Point, const FVector3d& Normal) 
	{ 
		return Container.Contains(Point) != bInvert; 
	};
	UELocal::SelectMeshElementsWithContainmentTest(TargetMesh, ContainsFunc,
		SelectionOut, SelectionType, MinNumTrianglePoints, false);
	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsWithPlane(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection& SelectionOut,
	FVector PlaneOrigin,
	FVector PlaneNormal,
	EGeometryScriptMeshSelectionType SelectionType,
	bool bInvert,
	int MinNumTrianglePoints)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("SelectMeshElementsWithPlane: TargetMesh is Null"));
		return TargetMesh;
	}

	PlaneNormal = Normalized(PlaneNormal);
	auto ContainsFunc = [&PlaneOrigin, &PlaneNormal, bInvert](const FVector3d& Point, const FVector3d& Normal) 
	{ 
		bool bContains = (Point - PlaneOrigin).Dot(PlaneNormal) >= 0;
		return bContains != bInvert;
	};
	UELocal::SelectMeshElementsWithContainmentTest(TargetMesh, ContainsFunc,
		SelectionOut, SelectionType, MinNumTrianglePoints, false);
	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection& SelectionOut,
	FVector PlaneNormal,
	double MaxAngleDeg,
	EGeometryScriptMeshSelectionType SelectionType,
	bool bInvert,
	int MinNumTrianglePoints)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("SelectMeshElementsByNormalAngle: TargetMesh is Null"));
		return TargetMesh;
	}

	PlaneNormal = Normalized(PlaneNormal);
	double CosMaxAngle = FMathd::Cos(MaxAngleDeg * FMathd::DegToRad);
	auto ContainsFunc = [&PlaneNormal, &CosMaxAngle, bInvert](const FVector3d& Point, const FVector3d& Normal) 
	{
		bool bContains = PlaneNormal.Dot(Normal) >= CosMaxAngle;
		return bContains != bInvert;
	};
	UELocal::SelectMeshElementsWithContainmentTest(TargetMesh, ContainsFunc,
		SelectionOut, SelectionType, MinNumTrianglePoints, true);
	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsInsideMesh(
	UDynamicMesh* TargetMesh,
	UDynamicMesh* SelectionMesh,
	FGeometryScriptMeshSelection& SelectionOut,
	FTransform SelectionMeshTransform,
	EGeometryScriptMeshSelectionType SelectionType,
	bool bInvert,
	double ShellDistance,
	double WindingThreshold,
	/** Only include triangles with this many vertices inside the Selection Mesh */
	int MinNumTrianglePoints)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("SelectMeshElementsInsideMesh: TargetMesh is Null"));
		return TargetMesh;
	}
	if (SelectionMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("SelectMeshElementsInsideMesh: SelectionMesh is Null"));
		return TargetMesh;
	}
	if (SelectionMesh == TargetMesh)
	{
		UE_LOG(LogGeometry, Warning, TEXT("SelectMeshElementsInsideMesh: SelectionMesh == TargetMesh, this is not supported"));
		// TODO: could select-all here?
		return TargetMesh;
	}

	// todo: for small meshes it is possibly cheaper to make a copy?
	FTransform InvTransform = SelectionMeshTransform.Inverse();

	SelectionMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		FDynamicMeshAABBTree3 Spatial(&ReadMesh, true);
		TFastWindingTree<FDynamicMesh3> FastWinding(&Spatial, true);

		auto ContainsFunc = [&Spatial, &FastWinding, InvTransform, WindingThreshold, ShellDistance, bInvert](const FVector3d& Point, const FVector3d& Normal)
		{
			FVector3d LocalPoint = InvTransform.TransformPosition(Point);
			bool bContains = FastWinding.IsInside(LocalPoint, WindingThreshold);
			if (!bContains && ShellDistance > 0)
			{
				double NearestDistSqr;
				int32 NearestTID = Spatial.FindNearestTriangle(LocalPoint, NearestDistSqr, IMeshSpatial::FQueryOptions(ShellDistance));
				if (NearestTID >= 0 && NearestDistSqr < ShellDistance * ShellDistance)
				{
					bContains = true;
				}
			}
			return bContains != bInvert;
		};
		UELocal::SelectMeshElementsWithContainmentTest(TargetMesh, ContainsFunc,
			SelectionOut, SelectionType, MinNumTrianglePoints, true);
	});

	return TargetMesh;
}




UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::InvertMeshSelection(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection Selection,
	FGeometryScriptMeshSelection& NewSelection,
	bool bOnlyToConnected)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("InvertMeshSelection: TargetMesh is Null"));
		return TargetMesh;
	}

	if (bOnlyToConnected)
	{
		ExpandMeshSelectionToConnected(TargetMesh, Selection, NewSelection, EGeometryScriptTopologyConnectionType::Geometric);
	}
	else
	{
		CreateSelectAllMeshSelection(TargetMesh, NewSelection, Selection.GetSelectionType());
	}
	NewSelection.CombineSelectionInPlace(Selection, EGeometryScriptCombineSelectionMode::Subtract);

	return TargetMesh;
}




UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::ExpandMeshSelectionToConnected(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection Selection,
	FGeometryScriptMeshSelection& NewSelection,
	EGeometryScriptTopologyConnectionType ConnectionType)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ExpandMeshSelectionToConnected: TargetMesh is Null"));
		return TargetMesh;
	}
	if (Selection.IsEmpty())
	{
		UE_LOG(LogGeometry, Warning, TEXT("ExpandMeshSelectionToConnected: Initial Selection is Empty"));
		return TargetMesh;
	}
	if (Selection.GetSelectionType() == EGeometryScriptMeshSelectionType::Vertices)
	{
		// todo: this...
		UE_LOG(LogGeometry, Warning, TEXT("ExpandMeshSelectionToConnected: Vertex Selection currently not supported"));
		NewSelection.SetSelection(Selection);
		return TargetMesh;
	}

	if (Selection.GetSelectionType() == EGeometryScriptMeshSelectionType::Polygroups &&
		ConnectionType == EGeometryScriptTopologyConnectionType::Polygroup)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ExpandMeshSelectionToConnected: Expanding Polygroup Selection to Connected Polygroups will not change selection"));
		NewSelection.SetSelection(Selection);	// this setup makes no sense
		return TargetMesh;
	}

	// collect up existing triangles and (optionally) polygroups
	TArray<int32> CurTriangles;
	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		Selection.ProcessByTriangleID(ReadMesh, [&](int32 TriangleID) { CurTriangles.Add(TriangleID); });
	});

	TSet<int32> ResultTriangles;
	if (ConnectionType == EGeometryScriptTopologyConnectionType::Geometric)
	{
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			FMeshConnectedComponents::GrowToConnectedTriangles(&ReadMesh, CurTriangles, ResultTriangles, nullptr, [](int32, int32) { return true; });
		});
	}
	else if (ConnectionType == EGeometryScriptTopologyConnectionType::Polygroup)
	{
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			FMeshConnectedComponents::GrowToConnectedTriangles(&ReadMesh, CurTriangles, ResultTriangles, nullptr,
				[&](int32 FromTriID, int32 ToTriID) {
					return ReadMesh.GetTriangleGroup(FromTriID) == ReadMesh.GetTriangleGroup(ToTriID);
			});
		});
	}
	else if (ConnectionType == EGeometryScriptTopologyConnectionType::MaterialID)
	{
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			if (ReadMesh.HasAttributes() && ReadMesh.Attributes()->GetMaterialID() != nullptr)
			{
				const FDynamicMeshMaterialAttribute* MaterialID = ReadMesh.Attributes()->GetMaterialID();
				FMeshConnectedComponents::GrowToConnectedTriangles(&ReadMesh, CurTriangles, ResultTriangles, nullptr,
					[&](int32 FromTriID, int32 ToTriID) {
					return MaterialID->GetValue(FromTriID) == MaterialID->GetValue(ToTriID);
				});
			}
		});
	}

	if (Selection.GetSelectionType() == EGeometryScriptMeshSelectionType::Triangles)
	{
		ConvertIndexSetToMeshSelection(TargetMesh, ResultTriangles, EGeometryScriptMeshSelectionType::Triangles, NewSelection);
	}
	else
	{
		TSet<int32> ResultGroupIDs;
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			for (int32 tid : ResultTriangles)
			{
				ResultGroupIDs.Add(ReadMesh.GetTriangleGroup(tid));
			}
		});
		ConvertIndexSetToMeshSelection(TargetMesh, ResultGroupIDs, EGeometryScriptMeshSelectionType::Polygroups, NewSelection);
	}

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshSelectionFunctions::ExpandContractMeshSelection(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection Selection,
	FGeometryScriptMeshSelection& NewSelection,
	int32 Iterations,
	bool bContract,
	bool bOnlyExpandToFaceNeighbours)
{
	if (TargetMesh == nullptr)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ExpandContractMeshSelection: TargetMesh is Null"));
		return TargetMesh;
	}
	if (Selection.IsEmpty())
	{
		UE_LOG(LogGeometry, Warning, TEXT("ExpandContractMeshSelection: Initial Selection is Empty"));
		return TargetMesh;
	}
	if (Iterations <= 0)
	{
		NewSelection.SetSelection(Selection);
		return TargetMesh;
	}
	Iterations = FMath::Clamp(Iterations, 1, 100);

	// TODO: when doing multiple iterations w/ polygroups, we cannot rely on the code below because it is only
	// expanding/contracting by triangle rings. Need to expand to polygroups at each step which is currently not easy
	// to do with a FMeshFaceSelection, need to convert to FMeshConnectedComponents/etc. So for now we will just
	// recursively do it this way which is expensive...
	if (Selection.GetSelectionType() == EGeometryScriptMeshSelectionType::Polygroups && Iterations > 1)
	{
		FGeometryScriptMeshSelection CurSelection = Selection;
		FGeometryScriptMeshSelection NextSelection;
		for (int32 k = 0; k < Iterations; ++k)
		{
			ExpandContractMeshSelection(TargetMesh, CurSelection, NextSelection, 1, bContract, bOnlyExpandToFaceNeighbours);
			CurSelection = NextSelection;
		}
		NewSelection = CurSelection;
		return TargetMesh;
	}


	FGeometrySelection NewGeoSelection;
	if (Selection.GetSelectionType() == EGeometryScriptMeshSelectionType::Vertices)
	{
		NewGeoSelection.InitializeTypes(EGeometryElementType::Vertex, EGeometryTopologyType::Triangle);
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			FMeshVertexSelection VtxSelection(&ReadMesh);
			Selection.ProcessByVertexID(ReadMesh, [&](int32 VertexID) { VtxSelection.Select(VertexID); } );
			if (bContract)
			{
				VtxSelection.ContractByBorderVertices(Iterations);
			}
			else
			{
				VtxSelection.ExpandToOneRingNeighbours(Iterations);
			}
			for (int32 VertexID : VtxSelection)
			{
				NewGeoSelection.Selection.Add(FGeoSelectionID::MeshVertex(VertexID).Encoded());
			}
		});
	}
	else 
	{
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			FMeshFaceSelection TriSelection(&ReadMesh);
			Selection.ProcessByTriangleID(ReadMesh, [&](int32 TriangleID) { TriSelection.Select(TriangleID); });
			if (bContract)
			{
				TriSelection.ContractBorderByOneRingNeighbours(Iterations, true);
			}
			else
			{
				if (bOnlyExpandToFaceNeighbours)
				{
					TriSelection.ExpandToFaceNeighbours(Iterations);
				}
				else
				{
					TriSelection.ExpandToOneRingNeighbours(Iterations);
				}
			}
			if (Selection.GetSelectionType() == EGeometryScriptMeshSelectionType::Triangles)
			{
				NewGeoSelection.InitializeTypes(EGeometryElementType::Face, EGeometryTopologyType::Triangle);
				for (int32 TriangleID : TriSelection)
				{
					NewGeoSelection.Selection.Add(FGeoSelectionID::MeshTriangle(TriangleID).Encoded());
				}
			}
			else
			{
				NewGeoSelection.InitializeTypes(EGeometryElementType::Face, EGeometryTopologyType::Polygroup);
				TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
				{
					TSet<int32> UniqueGroupIDs;
					for (int32 tid : TriSelection)
					{
						int32 GroupID = ReadMesh.GetTriangleGroup(tid);
						if (UniqueGroupIDs.Contains(GroupID) == false)
						{
							NewGeoSelection.Selection.Add(FGeoSelectionID::GroupFace(tid, GroupID).Encoded());
						}
					}
				});

			}
		});
	}

	NewSelection.SetSelection(MoveTemp(NewGeoSelection));

	return TargetMesh;
}





#undef LOCTEXT_NAMESPACE
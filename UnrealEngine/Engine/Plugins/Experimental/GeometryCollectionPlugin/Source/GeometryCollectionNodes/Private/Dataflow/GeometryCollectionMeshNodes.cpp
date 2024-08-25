// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionMeshNodes.h"
#include "Dataflow/DataflowCore.h"

#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionEngineUtility.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "UObject/UnrealTypePrivate.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "StaticMeshAttributes.h"
#include "DynamicMeshEditor.h"
#include "Operations/MeshBoolean.h"

#include "EngineGlobals.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionConvexUtility.h"
#include "Voronoi/Voronoi.h"
#include "PlanarCut.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"
#include "FractureEngineClustering.h"
#include "FractureEngineSelection.h"
#include "FractureEngineUtility.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "UDynamicMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionMeshNodes)

namespace Dataflow
{

	void GeometryCollectionMeshNodes()
	{
		static const FLinearColor CDefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FPointsToMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBoxToMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMeshInfoDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMeshToCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionToMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FStaticMeshToMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMeshAppendDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMeshBooleanDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMeshCopyToPointsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetMeshDataDataflowNode);

		// Mesh
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Mesh", FLinearColor(1.f, 0.16f, 0.05f), CDefaultNodeBodyTintColor);
	}
}


void FPointsToMeshDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TObjectPtr<UDynamicMesh>>(&Mesh) || Out->IsA<int32>(&TriangleCount))
	{
		const TArray<FVector>& PointsArr = GetValue<TArray<FVector>>(Context, &Points);

		if (PointsArr.Num() > 0)
		{
			TObjectPtr<UDynamicMesh> DynamicMesh = NewObject<UDynamicMesh>();
			DynamicMesh->Reset();

			UE::Geometry::FDynamicMesh3& DynMesh = DynamicMesh->GetMeshRef();

			for (auto& Point : PointsArr)
			{
				DynMesh.AppendVertex(Point);
			}

			SetValue(Context, DynamicMesh, &Mesh);
			SetValue(Context, DynamicMesh->GetTriangleCount(), &TriangleCount);
		}
		else
		{
			SetValue(Context, TObjectPtr<UDynamicMesh>(NewObject<UDynamicMesh>()), &Mesh);
			SetValue(Context, 0, &TriangleCount);
		}
	}
}


void FBoxToMeshDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TObjectPtr<UDynamicMesh>>(&Mesh) || Out->IsA<int32>(&TriangleCount))
	{
		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->Reset();

		UE::Geometry::FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();

		FBox InBox = GetValue<FBox>(Context, &Box);

		TArray<FVector3f> Vertices;
		TArray<FIntVector> Triangles;

		FFractureEngineUtility::ConvertBoxToVertexAndTriangleData(InBox, Vertices, Triangles);
		FFractureEngineUtility::ConstructMesh(DynMesh, Vertices, Triangles);

		SetValue(Context, NewMesh, &Mesh);
		SetValue(Context, NewMesh->GetTriangleCount(), &TriangleCount);
	}
}


void FMeshInfoDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&InfoString))
	{
		if (const TObjectPtr<const UDynamicMesh> InMesh = GetValue(Context, &Mesh))
		{
			const UE::Geometry::FDynamicMesh3& DynMesh = InMesh->GetMeshRef();

			SetValue(Context, DynMesh.MeshInfoString(), &InfoString);
		}
		else
		{
			SetValue(Context, FString(""), &InfoString);
		}
	}
}


void FMeshToCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		if (const TObjectPtr<const UDynamicMesh> InMesh = GetValue(Context, &Mesh))
		{
			const UE::Geometry::FDynamicMesh3& DynMesh = InMesh->GetMeshRef();

			if (DynMesh.VertexCount() > 0)
			{
				FMeshDescription MeshDescription;
				FStaticMeshAttributes Attributes(MeshDescription);
				Attributes.Register();

				FDynamicMeshToMeshDescription Converter;
				Converter.Convert(&DynMesh, MeshDescription, true);

				FGeometryCollection NewGeometryCollection = FGeometryCollection();
				FGeometryCollectionEngineConversion::AppendMeshDescription(&MeshDescription, FString(TEXT("TEST")), 0, FTransform().Identity, &NewGeometryCollection);

				FManagedArrayCollection NewCollection = FManagedArrayCollection();
				NewGeometryCollection.CopyTo(&NewCollection);

				SetValue(Context, MoveTemp(NewCollection), &Collection);

				return;
			}
		}

		SetValue(Context, FManagedArrayCollection(), &Collection);
	}
}


void FCollectionToMeshDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
#if WITH_EDITORONLY_DATA
	if (Out->IsA<TObjectPtr<UDynamicMesh>>(&Mesh))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		
		if (InCollection.NumElements(FGeometryCollection::TransformGroup) > 0)
		{
			if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
			{
				const TManagedArray<FTransform3f>& BoneTransforms = InCollection.GetAttribute<FTransform3f>("Transform", FGeometryCollection::TransformGroup);

				TArray<int32> TransformIndices;
				TransformIndices.AddUninitialized(BoneTransforms.Num());

				int32 Idx = 0;
				for (int32& TransformIdx : TransformIndices)
				{
					TransformIdx = Idx++;
				}

				FMeshDescription MeshDescription;
				FStaticMeshAttributes Attributes(MeshDescription);
				Attributes.Register();

				FTransform TransformOut;

				ConvertToMeshDescription(MeshDescription, TransformOut, bCenterPivot, *GeomCollection, BoneTransforms, TransformIndices);

				TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
				NewMesh->Reset();

				UE::Geometry::FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();
				{
					FMeshDescriptionToDynamicMesh ConverterToDynamicMesh;
					ConverterToDynamicMesh.Convert(&MeshDescription, DynMesh);
				}

				SetValue(Context, NewMesh, &Mesh);

				return;
			}
		}

		SetValue(Context, TObjectPtr<UDynamicMesh>(NewObject<UDynamicMesh>()), &Mesh);
	}
#endif
}


void FStaticMeshToMeshDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
#if WITH_EDITORONLY_DATA
	if (Out->IsA<TObjectPtr<UDynamicMesh>>(&Mesh))
	{
		if (FMeshDescription* MeshDescription = bUseHiRes ? StaticMesh->GetHiResMeshDescription() : StaticMesh->GetMeshDescription(LODLevel))
		{
			TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
			NewMesh->Reset();

			UE::Geometry::FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();
			{
				FMeshDescriptionToDynamicMesh ConverterToDynamicMesh;
				ConverterToDynamicMesh.Convert(MeshDescription, DynMesh);
			}

			SetValue(Context, NewMesh, &Mesh);
		}
		else
		{
			SetValue(Context, TObjectPtr<UDynamicMesh>(NewObject<UDynamicMesh>()), &Mesh);
		}
	}
#endif
}


void FMeshAppendDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TObjectPtr<UDynamicMesh>>(&Mesh))
	{
		if (TObjectPtr<UDynamicMesh> InMesh1 = GetValue<TObjectPtr<UDynamicMesh>>(Context, &Mesh1))
		{
			if (TObjectPtr<UDynamicMesh> InMesh2 = GetValue<TObjectPtr<UDynamicMesh>>(Context, &Mesh2))
			{
				const UE::Geometry::FDynamicMesh3& DynMesh1 = InMesh1->GetMeshRef();
				const UE::Geometry::FDynamicMesh3& DynMesh2 = InMesh2->GetMeshRef();

				if (DynMesh1.VertexCount() > 0 || DynMesh2.VertexCount() > 0)
				{
					TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
					NewMesh->Reset();

					UE::Geometry::FDynamicMesh3& ResultDynMesh = NewMesh->GetMeshRef();

					UE::Geometry::FDynamicMeshEditor MeshEditor(&ResultDynMesh);

					UE::Geometry::FMeshIndexMappings IndexMaps1;
					MeshEditor.AppendMesh(&DynMesh1, IndexMaps1);

					UE::Geometry::FMeshIndexMappings IndexMaps2;
					MeshEditor.AppendMesh(&DynMesh2, IndexMaps2);

					SetValue(Context, NewMesh, &Mesh);

					return;
				}
			}
		}

		SetValue(Context, TObjectPtr<UDynamicMesh>(NewObject<UDynamicMesh>()), &Mesh);
	}
}


void FMeshBooleanDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TObjectPtr<UDynamicMesh>>(&Mesh))
	{
		if (TObjectPtr<UDynamicMesh> InMesh1 = GetValue<TObjectPtr<UDynamicMesh>>(Context, &Mesh1))
		{
			if (TObjectPtr<UDynamicMesh> InMesh2 = GetValue<TObjectPtr<UDynamicMesh>>(Context, &Mesh2))
			{
				const UE::Geometry::FDynamicMesh3& DynMesh1 = InMesh1->GetMeshRef();
				const UE::Geometry::FDynamicMesh3& DynMesh2 = InMesh2->GetMeshRef();

				if (DynMesh1.VertexCount() > 0 && DynMesh2.VertexCount() > 0)
				{
					// Get output
					TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
					NewMesh->Reset();
					UE::Geometry::FDynamicMesh3& ResultDynMesh = NewMesh->GetMeshRef();

					UE::Geometry::FMeshBoolean::EBooleanOp BoolOp = UE::Geometry::FMeshBoolean::EBooleanOp::Intersect;
					if (Operation == EMeshBooleanOperationEnum::Dataflow_MeshBoolean_Intersect)
					{
						BoolOp = UE::Geometry::FMeshBoolean::EBooleanOp::Intersect;
					}
					else if (Operation == EMeshBooleanOperationEnum::Dataflow_MeshBoolean_Union)
					{
						BoolOp = UE::Geometry::FMeshBoolean::EBooleanOp::Union;
					}
					else if (Operation == EMeshBooleanOperationEnum::Dataflow_MeshBoolean_Difference)
					{
						BoolOp = UE::Geometry::FMeshBoolean::EBooleanOp::Difference;
					}

					UE::Geometry::FMeshBoolean Boolean(&DynMesh1, &DynMesh2, &ResultDynMesh, BoolOp);
					Boolean.bSimplifyAlongNewEdges = true;
					Boolean.PreserveUVsOnlyForMesh = 0; // slight warping of the autogenerated cell UVs generally doesn't matter
					Boolean.bWeldSharedEdges = false;
					Boolean.bTrackAllNewEdges = true;

					if (Boolean.Compute())
					{
						SetValue(Context, NewMesh, &Mesh);

						return;
					}
				}
			}
		}

		SetValue(Context, TObjectPtr<UDynamicMesh>(NewObject<UDynamicMesh>()), &Mesh);
	}
}

void FMeshCopyToPointsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TObjectPtr<UDynamicMesh>>(&Mesh))
	{
		if (TObjectPtr<UDynamicMesh> InMeshToCopy = GetValue<TObjectPtr<UDynamicMesh>>(Context, &MeshToCopy))
		{
			const UE::Geometry::FDynamicMesh3& InDynMeshToCopy = InMeshToCopy->GetMeshRef();

			const TArray<FVector>& InPoints = GetValue<TArray<FVector>>(Context, &Points);

			if (InPoints.Num() > 0 && InDynMeshToCopy.VertexCount() > 0)
			{
				TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
				NewMesh->Reset();

				UE::Geometry::FDynamicMesh3& ResultDynMesh = NewMesh->GetMeshRef();
				UE::Geometry::FDynamicMeshEditor MeshEditor(&ResultDynMesh);

				for (auto& Point : InPoints)
				{
					UE::Geometry::FDynamicMesh3 DynMeshTemp(InDynMeshToCopy);
					UE::Geometry::FRefCountVector VertexRefCounts = DynMeshTemp.GetVerticesRefCounts();

					UE::Geometry::FRefCountVector::IndexIterator ItVertexID = VertexRefCounts.BeginIndices();
					const UE::Geometry::FRefCountVector::IndexIterator ItEndVertexID = VertexRefCounts.EndIndices();

					while (ItVertexID != ItEndVertexID)
					{
						DynMeshTemp.SetVertex(*ItVertexID, Scale * DynMeshTemp.GetVertex(*ItVertexID) + Point);
						++ItVertexID;
					}

					UE::Geometry::FMeshIndexMappings IndexMaps;
					MeshEditor.AppendMesh(&DynMeshTemp, IndexMaps);
				}

				SetValue(Context, NewMesh, &Mesh);

				return;
			}
		}

		SetValue(Context, TObjectPtr<UDynamicMesh>(NewObject<UDynamicMesh>()), &Mesh);
	}
}


void FGetMeshDataDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&VertexCount))
	{
		if (const TObjectPtr<const UDynamicMesh> InMesh = GetValue<TObjectPtr<UDynamicMesh>>(Context, &Mesh))
		{
			SetValue(Context, InMesh->GetMeshRef().VertexCount(), &VertexCount);
		}
		else
		{
			SetValue(Context, 0, &VertexCount);
		}
	}
	else if (Out->IsA<int32>(&EdgeCount))
	{
		if (const TObjectPtr<const UDynamicMesh> InMesh = GetValue<TObjectPtr<UDynamicMesh>>(Context, &Mesh))
		{
			SetValue(Context, InMesh->GetMeshRef().EdgeCount(), &EdgeCount);
		}
		else
		{
			SetValue(Context, 0, &EdgeCount);
		}
	}
	else if (Out->IsA<int32>(&TriangleCount))
	{
		if (const TObjectPtr<const UDynamicMesh> InMesh = GetValue<TObjectPtr<UDynamicMesh>>(Context, &Mesh))
		{
			SetValue(Context, InMesh->GetMeshRef().TriangleCount(), &TriangleCount);
		}
		else
		{
			SetValue(Context, 0, &TriangleCount);
		}
	}
}


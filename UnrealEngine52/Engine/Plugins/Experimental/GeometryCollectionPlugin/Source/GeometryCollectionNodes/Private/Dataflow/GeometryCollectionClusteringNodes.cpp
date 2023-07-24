// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionClusteringNodes.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionClusteringNodes)

namespace Dataflow
{

	void GeometryCollectionClusteringNodes()
	{
		static const FLinearColor CDefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAutoClusterDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FClusterFlattenDataflowNode);

		// GeometryCollection|Cluster
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("GeometryCollection|Cluster", FLinearColor(.25f, 0.45f, 0.8f), CDefaultNodeBodyTintColor);
	}
}


void FAutoClusterDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			EClusterSizeMethodEnum InClusterSizeMethod = ClusterSizeMethod;
			int32 InClusterSites = GetValue<int32>(Context, &ClusterSites);
			float InClusterFraction = GetValue<float>(Context, &ClusterFraction);
			float InSiteSize = GetValue<float>(Context, &SiteSize);
			bool InAutoCluster = AutoCluster;
			bool InAvoidIsolated = AvoidIsolated;

			TArray<int32> SelectedBones;
			InTransformSelection.AsArray(SelectedBones);

			FFractureEngineClustering::AutoCluster(*GeomCollection,
				SelectedBones,
				(EFractureEngineClusterSizeMethod)InClusterSizeMethod,
				InClusterSites,
				InClusterFraction,
				InSiteSize,
				InAutoCluster,
				InAvoidIsolated);

			SetValue<FManagedArrayCollection>(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
		}
	}
}


void FClusterFlattenDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(GeomCollection.Get(), -1);

			const TManagedArray<int32>& Levels = GeomCollection->GetAttribute<int32>("Level", FGeometryCollection::TransformGroup);

			// Populate Selected Bones in an Array
			// @todo(harsha) Implement with Selection
			// For every bone in selected array: [ClusterIndex]
			int32 ClusterIndex = 0;
			TArray<int32> LeafBones;
			FGeometryCollectionClusteringUtility::GetLeafBones(GeomCollection.Get(), ClusterIndex, true, LeafBones);
			FGeometryCollectionClusteringUtility::ClusterBonesUnderExistingNode(GeomCollection.Get(), ClusterIndex, LeafBones);
			FGeometryCollectionClusteringUtility::RemoveDanglingClusters(GeomCollection.Get());
			// End for

			FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(GeomCollection.Get(), -1);
			SetValue<FManagedArrayCollection>(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
		}
	}
}

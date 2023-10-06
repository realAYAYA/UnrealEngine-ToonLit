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
#include "GeometryCollection/Facades/CollectionTransformSelectionFacade.h"
#include "GeometryCollection/Facades/CollectionHierarchyFacade.h"
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
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FClusterUnclusterDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FClusterDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FClusterMergeDataflowNode);

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
			bool InEnforceSiteParameters = EnforceSiteParameters;
			bool InAvoidIsolated = AvoidIsolated;
			int32 InGridX = GetValue<int32>(Context, &ClusterGridWidth);
			int32 InGridY = GetValue<int32>(Context, &ClusterGridDepth);
			int32 InGridZ = GetValue<int32>(Context, &ClusterGridHeight);
			float InMinimumClusterSize = GetValue<float>(Context, &MinimumSize);
			int32 InKMeansIterations = ClusterSizeMethod == EClusterSizeMethodEnum::Dataflow_ClusterSizeMethod_ByGrid ? DriftIterations : 500;

			TArray<int32> SelectedBones;
			InTransformSelection.AsArray(SelectedBones);

			FFractureEngineClustering::AutoCluster(*GeomCollection,
				SelectedBones,
				(EFractureEngineClusterSizeMethod)InClusterSizeMethod,
				InClusterSites,
				InClusterFraction,
				InSiteSize,
				InAutoCluster,
				InAvoidIsolated, 
				InEnforceSiteParameters,
				InGridX, InGridY, InGridZ, InMinimumClusterSize, InKMeansIterations);

			SetValue<const FManagedArrayCollection&>(Context, *GeomCollection, &Collection);
		}
	}
}


void FClusterFlattenDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection) && IsConnected(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
		if (InCollection.NumElements(FGeometryCollection::TransformAttribute) > 0)
		{
			if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
			{
				Chaos::Facades::FCollectionHierarchyFacade HierarchyFacade(*GeomCollection);
				HierarchyFacade.GenerateLevelAttribute();

				// Populate Selected Bones in an Array
				// @todo(harsha) Implement with Selection
				// For every bone in selected array: [RootClusterIndex]
				const int32 RootClusterIndex = HierarchyFacade.GetRootIndex();
				TArray<int32> LeafBones;
				FGeometryCollectionClusteringUtility::GetLeafBones(GeomCollection.Get(), RootClusterIndex, true, LeafBones);
				FGeometryCollectionClusteringUtility::ClusterBonesUnderExistingNode(GeomCollection.Get(), RootClusterIndex, LeafBones);
				FGeometryCollectionClusteringUtility::RemoveDanglingClusters(GeomCollection.Get());
				// End for

				HierarchyFacade.GenerateLevelAttribute();
				SetValue(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
			}
		}
	}
}


void FClusterUnclusterDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);
		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			Chaos::Facades::FCollectionHierarchyFacade HierarchyFacade(*GeomCollection);
			HierarchyFacade.GenerateLevelAttribute();

			TArray<int32> Selection = InTransformSelection.AsArray();
			GeometryCollection::Facades::FCollectionTransformSelectionFacade SelectionFacade(*GeomCollection);
			SelectionFacade.ConvertSelectionToClusterNodes(Selection, false);
			SelectionFacade.RemoveRootNodes(Selection);
			if (!Selection.IsEmpty())
			{
				FGeometryCollectionClusteringUtility::CollapseHierarchyOneLevel(GeomCollection.Get(), Selection);
				FGeometryCollectionClusteringUtility::RemoveDanglingClusters(GeomCollection.Get());

				HierarchyFacade.GenerateLevelAttribute();
			}
			SetValue<const FManagedArrayCollection&>(Context, *GeomCollection, &Collection);
		}
	}
}


void FClusterDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
		const FDataflowTransformSelection& InTransformSelection = GetValue(Context, &TransformSelection);
		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			TArray<int32> Selection = InTransformSelection.AsArray();
			FFractureEngineClustering::ClusterSelected(*GeomCollection, Selection);
			SetValue(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
		}
	}
}

void FClusterMergeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
		const FDataflowTransformSelection& InTransformSelection = GetValue(Context, &TransformSelection);
		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			TArray<int32> Selection = InTransformSelection.AsArray();
			FFractureEngineClustering::MergeSelectedClusters(*GeomCollection, Selection);
			SetValue(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
		}
	}
}


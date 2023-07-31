// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureToolClusterMagnet.h"

#include "FractureTool.h"
#include "FractureEditorStyle.h"
#include "FractureEditorCommands.h"
#include "FractureToolContext.h"

#include "Chaos/TriangleMesh.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "Chaos/Particles.h"
#include "Chaos/Vector.h"

#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"

#include "GeometryCollection/GeometryCollectionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FractureToolClusterMagnet)


#define LOCTEXT_NAMESPACE "FractureClusterMagnet"



UFractureToolClusterMagnet::UFractureToolClusterMagnet(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	ClusterMagnetSettings = NewObject<UFractureClusterMagnetSettings>(GetTransientPackage(), UFractureClusterMagnetSettings::StaticClass());
	ClusterMagnetSettings->OwnerTool = this;
}


FText UFractureToolClusterMagnet::GetDisplayText() const
{
	return FText(NSLOCTEXT("Fracture", "FractureToolClusterMagnet", "Magnet"));
}


FText UFractureToolClusterMagnet::GetTooltipText() const
{
	return FText(NSLOCTEXT("Fracture", "FractureToolClusterMagnetToolTip", "Builds clusters by grouping the selected bones with their adjacent, neighboring bones. Can iteratively expand to a larger set of neighbors-of-neighbors."));
}

FText UFractureToolClusterMagnet::GetApplyText() const
{
	return FText(NSLOCTEXT("Fracture", "ExecuteClusterMagnet", "Cluster Magnet"));
}

FSlateIcon UFractureToolClusterMagnet::GetToolIcon() const
{
	return FSlateIcon("FractureEditorStyle", "FractureEditor.ClusterMagnet");
}


TArray<UObject*> UFractureToolClusterMagnet::GetSettingsObjects() const
{
	TArray<UObject*> Settings;
	Settings.Add(ClusterMagnetSettings);
	return Settings;
}

void UFractureToolClusterMagnet::RegisterUICommand(FFractureEditorCommands* BindingContext)
{
	UI_COMMAND_EXT(BindingContext, UICommandInfo, "ClusterMagnet", "Magnet", "Builds clusters by grouping the selected bones with their adjacent, neighboring bones. Can iteratively expand to a larger set of neighbors-of-neighbors.", EUserInterfaceActionType::ToggleButton, FInputChord());
	BindingContext->ClusterMagnet = UICommandInfo;
}


void UFractureToolClusterMagnet::Execute(TWeakPtr<FFractureEditorModeToolkit> InToolkit)
{
	if (InToolkit.IsValid())
	{
		FFractureEditorModeToolkit* Toolkit = InToolkit.Pin().Get();

		TArray<FFractureToolContext> Contexts = GetFractureToolContexts();

		for (FFractureToolContext& Context : Contexts)
		{
			// We require certain attributes present to proceed.
			if (!CheckPresenceOfNecessaryAttributes(Context.GetGeometryCollection()))
			{
				continue;
			}

			FGeometryCollectionEdit Edit(Context.GetGeometryCollectionComponent(), GeometryCollection::EEditUpdate::RestPhysicsDynamic);

			const TManagedArray<TSet<int32>>& Children = Context.GetGeometryCollection()->Children;
			if (!Context.GetGeometryCollection()->HasAttribute("Level", FTransformCollection::TransformGroup))
			{
				FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(Edit.GetRestCollection()->GetGeometryCollection().Get(), -1);
			}
			const TManagedArray<int32>& Levels = Context.GetGeometryCollection()->GetAttribute<int32>("Level", FGeometryCollection::TransformGroup);
			int32 StartTransformCount = Children.Num();
			
			Context.Sanitize();
			TMap<int32, TArray<int32>> ClusteredSelection = Context.GetClusteredSelections();

			for (TPair<int32, TArray<int32>>& Group : ClusteredSelection)
			{
				if (Group.Key == INDEX_NONE) // Group is top level
				{
					continue;
				}

				// We have the connections for the leaf nodes of our geometry collection. We want to percolate those up to the top nodes.
				TMap<int32, TSet<int32>> TopNodeConnectivity = InitializeConnectivity(Children[Group.Key], Context.GetGeometryCollection(), Levels[Group.Key]+1);

				// Separate the top nodes into cluster magnets and a pool of available nodes.
				TArray<FClusterMagnet> ClusterMagnets;
				TSet<int32> RemainingPool;
				SeparateClusterMagnets(Children[Group.Key], Group.Value, TopNodeConnectivity, ClusterMagnets, RemainingPool);

				for (uint32 Iteration = 0; Iteration < ClusterMagnetSettings->Iterations; ++Iteration)
				{
					bool bNeighborsAbsorbed = false;

					// each cluster gathers adjacent nodes from the pool
					for (FClusterMagnet& ClusterMagnet : ClusterMagnets)
					{
						bNeighborsAbsorbed |= AbsorbClusterNeighbors(TopNodeConnectivity, ClusterMagnet, RemainingPool);
					}

					// early termination
					if (!bNeighborsAbsorbed)
					{
						break;
					}
				}

				// Create new clusters from the cluster magnets
				for (const FClusterMagnet& ClusterMagnet : ClusterMagnets)
				{
					if (ClusterMagnet.ClusteredNodes.Num() > 1)
					{
						TArray<int32> NewChildren = ClusterMagnet.ClusteredNodes.Array();
						NewChildren.Sort();
						FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(Context.GetGeometryCollection().Get(), NewChildren[0], NewChildren, false, false);

					}
				}
			}

			Context.GenerateGuids(StartTransformCount);

			Refresh(Context, Toolkit);
		}

		SetOutlinerComponents(Contexts, Toolkit);
	}
}

bool UFractureToolClusterMagnet::CheckPresenceOfNecessaryAttributes(const FGeometryCollectionPtr GeometryCollection) const
{
	if (!GeometryCollection->HasAttribute("Level", FTransformCollection::TransformGroup))
	{
		UE_LOG(LogFractureTool, Error, TEXT("Cannot execute Cluster Magnet tool: missing Level attribute."));
		return false;
	}

	if (!GeometryCollection->HasAttribute("Proximity", FGeometryCollection::GeometryGroup))
	{
		UE_LOG(LogFractureTool, Error, TEXT("Cannot execute Cluster Magnet tool: missing Proximity attribute."));
		return false;
	}

	return true;
}

TMap<int32, TSet<int32>> UFractureToolClusterMagnet::InitializeConnectivity(const TSet<int32>& TopNodes, FGeometryCollectionPtr GeometryCollection, int32 OperatingLevel) const
{
	FGeometryCollectionProximityUtility ProximityUtility(GeometryCollection.Get());
	ProximityUtility.UpdateProximity();

	TMap<int32, TSet<int32>> ConnectivityMap;
	for (int32 Index : TopNodes)
	{
		// Collect the proximity indices of all the leaf nodes under this top node,
		// traced back up to its parent top node, so that all connectivity describes
		// relationships only between top nodes.
		TSet<int32> Connections;
		CollectTopNodeConnections(GeometryCollection, Index, OperatingLevel, Connections);
		Connections.Remove(Index);

		// Remove any connections outside the current operating branch.
		ConnectivityMap.Add(Index, Connections.Intersect(TopNodes));
	}

	return ConnectivityMap;
}

void UFractureToolClusterMagnet::CollectTopNodeConnections(FGeometryCollectionPtr GeometryCollection, int32 Index, int32 OperatingLevel, TSet<int32>& OutConnections) const
{
	const TManagedArray<int32>& TransformToGeometryIndex = GeometryCollection->TransformToGeometryIndex;
	if (GeometryCollection->SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid
		&& TransformToGeometryIndex[Index] != INDEX_NONE) // rigid node with geometry, leaf of the simulated part
	{
		const TManagedArray<TSet<int32>>& Proximity = GeometryCollection->GetAttribute<TSet<int32>>("Proximity", FGeometryCollection::GeometryGroup);
		const TManagedArray<int32>& GeometryToTransformIndex = GeometryCollection->TransformIndex;


		for (int32 Neighbor : Proximity[TransformToGeometryIndex[Index]])
		{
			int32 NeighborTransformIndex = GeometryToTransformIndex[Neighbor];
			OutConnections.Add(FGeometryCollectionClusteringUtility::GetParentOfBoneAtSpecifiedLevel(GeometryCollection.Get(), NeighborTransformIndex, OperatingLevel));
		}		
	}
	else
	{
		const TManagedArray<TSet<int32>>& Children = GeometryCollection->Children;
		for (int32 ChildIndex : Children[Index])
		{
			CollectTopNodeConnections(GeometryCollection, ChildIndex, OperatingLevel, OutConnections);
		}
	}
}

void UFractureToolClusterMagnet::SeparateClusterMagnets(
	const TSet<int32>& TopNodes,
	const TArray<int32>& Selection,
	const TMap<int32, TSet<int32>>& TopNodeConnectivity,
	TArray<FClusterMagnet>& OutClusterMagnets,
	TSet<int32>& OutRemainingPool) const
{
	OutClusterMagnets.Reserve(TopNodes.Num());
	OutRemainingPool.Reserve(TopNodes.Num());
	
	for (int32 Index : TopNodes)
	{
		if (Selection.Contains(Index))
		{
			OutClusterMagnets.AddDefaulted();
			FClusterMagnet& NewMagnet = OutClusterMagnets.Last();
			NewMagnet.ClusteredNodes.Add(Index);
			NewMagnet.Connections = TopNodeConnectivity[Index];
		}
		else
		{
			OutRemainingPool.Add(Index);
		}
	}
}

bool UFractureToolClusterMagnet::AbsorbClusterNeighbors(const TMap<int32, TSet<int32>> TopNodeConnectivity, FClusterMagnet& OutClusterMagnet, TSet<int32>& OutRemainingPool) const
{
	// Return true if neighbors were absorbed.
	bool bNeighborsAbsorbed = false;

	TSet<int32> NewConnections;
	for (int32 NeighborIndex : OutClusterMagnet.Connections)
	{
		// If the neighbor is still in the pool, absorb it and its connections.
		if (OutRemainingPool.Contains(NeighborIndex))
		{
			OutClusterMagnet.ClusteredNodes.Add(NeighborIndex);
			NewConnections.Append(TopNodeConnectivity[NeighborIndex]);
			OutRemainingPool.Remove(NeighborIndex);
			bNeighborsAbsorbed = true;
		}
	}
	OutClusterMagnet.Connections.Append(NewConnections);

	return bNeighborsAbsorbed;
}

#undef LOCTEXT_NAMESPACE

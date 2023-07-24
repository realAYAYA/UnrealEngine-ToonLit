// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureEngineClustering.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "Math/BoxSphereBounds.h"

FVoronoiPartitioner::FVoronoiPartitioner(const FGeometryCollection* GeometryCollection, int32 ClusterIndex)
{
	// Collect children of cluster
	const TManagedArray<TSet<int32>>& Children = GeometryCollection->Children;
	TransformIndices = Children[ClusterIndex].Array();

	GenerateCentroids(GeometryCollection);
}

void FVoronoiPartitioner::KMeansPartition(int32 InPartitionCount)
{
	PartitionCount = InPartitionCount;

	InitializePartitions();
	for (int32 Iteration = 0; Iteration < MaxKMeansIterations; Iteration++)
	{
		// Refinement is complete when no nodes change partition.
		if (!Refine())
		{
			break;
		}
	}
}

void FVoronoiPartitioner::MergeSingleElementPartitions(FGeometryCollection* GeometryCollection)
{
	if (Connectivity.IsEmpty())
	{
		GenerateConnectivity(GeometryCollection);
	}
	for (int32 ElIdx = 0; ElIdx < TransformIndices.Num(); ElIdx++)
	{
		int32 Partition = Partitions[ElIdx];
		if (PartitionSize[Partition] == 1)
		{
			// Find the smallest neighboring partition to merge to
			// (to help keep partition sizes balanced)
			int32 SmallestNbrPartition = -1;
			int32 SmallestNbrSize = TransformIndices.Num()+1;
			for (int32 NbrEl : Connectivity[ElIdx])
			{
				int32 NbrPartition = Partitions[NbrEl];
				if (NbrPartition == Partition)
				{
					continue;
				}
				int32 NbrSize = PartitionSize[NbrPartition];
				if (NbrSize > 0 && (SmallestNbrPartition == -1 || NbrSize < SmallestNbrSize))
				{
					SmallestNbrPartition = NbrPartition;
					SmallestNbrSize = NbrSize;
				}
			}
			if (SmallestNbrPartition != -1)
			{
				Partitions[ElIdx] = SmallestNbrPartition;
				PartitionSize[Partition]--;
				PartitionSize[SmallestNbrPartition]++;
			}
		}
	}
}

void FVoronoiPartitioner::SplitDisconnectedPartitions(FGeometryCollection* GeometryCollection)
{
	Visited.Init(false, TransformIndices.Num());
	if (Connectivity.IsEmpty())
	{
		GenerateConnectivity(GeometryCollection);
	}
	
	for (int32 PartitionIndex = 0; PartitionIndex < PartitionCount; ++PartitionIndex)
	{
		int32 FirstElementInPartitionIndex = Partitions.Find(PartitionIndex);
		if (FirstElementInPartitionIndex > INDEX_NONE)
		{
			MarkVisited(FirstElementInPartitionIndex, PartitionIndex);

			// Place unvisited partition members in a new partition.
			bool bFoundUnattached = false;
			for (int32 Index = 0; Index < TransformIndices.Num(); ++Index)
			{
				if (Partitions[Index] == PartitionIndex)
				{
					if (!Visited[Index])
					{
						if (!bFoundUnattached)
						{
							bFoundUnattached = true;
							PartitionCount++;
							PartitionSize.Add(0);
						}
						Partitions[Index] = PartitionCount - 1;
						PartitionSize[PartitionCount - 1]++;
						PartitionSize[PartitionIndex]--;
					}
				}
			}
		}
	}
}

void FVoronoiPartitioner::MarkVisited(int32 Index, int32 PartitionIndex)
{
	Visited[Index] = true;
	for (int32 Adjacent : Connectivity[Index])
	{
		if (!Visited[Adjacent] && Partitions[Adjacent] == PartitionIndex)
		{
			MarkVisited(Adjacent, PartitionIndex);
		}
	}
}

TArray<int32> FVoronoiPartitioner::GetPartition(int32 PartitionIndex) const
{
	TArray<int32> PartitionContents;
	PartitionContents.Reserve(TransformIndices.Num());
	for (int32 Index = 0; Index < TransformIndices.Num(); ++Index)
	{
		if (Partitions[Index] == PartitionIndex)
		{
			PartitionContents.Add(TransformIndices[Index]);
		}
	}

	return PartitionContents;
}

void FVoronoiPartitioner::GenerateConnectivity(const FGeometryCollection* GeometryCollection)
{
	
	Connectivity.SetNum(TransformIndices.Num());

	if (!ensure(GeometryCollection->HasAttribute("Level", FGeometryCollection::TransformGroup)))
	{
		return;
	}
	const TManagedArray<int32>& Levels = GeometryCollection->GetAttribute<int32>("Level", FGeometryCollection::TransformGroup);

	for (int32 Index = 0; Index < TransformIndices.Num(); ++Index)
	{
		CollectConnections(GeometryCollection, TransformIndices[Index], Levels[TransformIndices[Index]], Connectivity[Index]);
	}
}

void FVoronoiPartitioner::CollectConnections(const FGeometryCollection* GeometryCollection, int32 Index, int32 OperatingLevel, TSet<int32>& OutConnections) const
{
	const TManagedArray<TSet<int32>>& Children = GeometryCollection->Children;
	if (GeometryCollection->IsRigid(Index))
	{
		const TManagedArray<TSet<int32>>& Proximity = GeometryCollection->GetAttribute<TSet<int32>>("Proximity", FGeometryCollection::GeometryGroup);
		const TManagedArray<int32>& GeometryToTransformIndex = GeometryCollection->TransformIndex;
		const TManagedArray<int32>& TransformToGeometryIndex = GeometryCollection->TransformToGeometryIndex;

		for (int32 Neighbor : Proximity[TransformToGeometryIndex[Index]])
		{
			int32 NeighborTransformIndex = FGeometryCollectionClusteringUtility::GetParentOfBoneAtSpecifiedLevel(GeometryCollection, GeometryToTransformIndex[Neighbor], OperatingLevel);
			int32 ContextIndex = TransformIndices.Find(NeighborTransformIndex);
			if (ContextIndex > INDEX_NONE)
			{
				OutConnections.Add(ContextIndex);
			}
		}
	}
	else
	{
		for (int32 ChildIndex : Children[Index])
		{
			CollectConnections(GeometryCollection, ChildIndex, OperatingLevel, OutConnections);
		}
	}
}

void FVoronoiPartitioner::GenerateCentroids(const FGeometryCollection* GeometryCollection)
{
	Centroids.SetNum(TransformIndices.Num());
	ParallelFor(TransformIndices.Num(), [this, GeometryCollection](int32 Index)
		{
			Centroids[Index] = GenerateCentroid(GeometryCollection, TransformIndices[Index]);
		});
}

FVector FVoronoiPartitioner::GenerateCentroid(const FGeometryCollection* GeometryCollection, int32 TransformIndex) const
{
	FBox Bounds = GenerateBounds(GeometryCollection, TransformIndex);
	return Bounds.GetCenter();
}

FBox FVoronoiPartitioner::GenerateBounds(const FGeometryCollection* GeometryCollection, int32 TransformIndex) const
{
	check(GeometryCollection->IsRigid(TransformIndex) || GeometryCollection->IsClustered(TransformIndex));

	// Return the bounds of all the vertices contained by this branch.

	if (GeometryCollection->IsRigid(TransformIndex))
	{
		const TManagedArray<FTransform>& Transforms = GeometryCollection->Transform;
		const TManagedArray<int32>& Parents = GeometryCollection->Parent;
		FTransform GlobalTransform = GeometryCollectionAlgo::GlobalMatrix(Transforms, Parents, TransformIndex);
		
		int32 GeometryIndex = GeometryCollection->TransformToGeometryIndex[TransformIndex];
		int32 VertexStart = GeometryCollection->VertexStart[GeometryIndex];
		int32 VertexCount = GeometryCollection->VertexCount[GeometryIndex];
		const TManagedArray<FVector3f>& GCVertices = GeometryCollection->Vertex;

		TArray<FVector> Vertices;
		Vertices.SetNum(VertexCount);
		for (int32 VertexOffset = 0; VertexOffset < VertexCount; ++VertexOffset)
		{
			Vertices[VertexOffset] = GlobalTransform.TransformPosition((FVector)GCVertices[VertexStart + VertexOffset]);
		}

		return FBox(Vertices);
	}
	else // recurse the cluster
	{
		const TArray<int32> Children = GeometryCollection->Children[TransformIndex].Array();

		// Empty clusters are problematic
		if (Children.Num() == 0)
		{
			return FBox();
		}

		FBox Bounds = GenerateBounds(GeometryCollection, Children[0]);
		for (int32 ChildIndex = 1; ChildIndex < Children.Num(); ++ChildIndex)
		{
			Bounds += GenerateBounds(GeometryCollection, Children[ChildIndex]);
		}

		return Bounds;
	}
}

void FVoronoiPartitioner::InitializePartitions()
{
	check(PartitionCount > 0);

	PartitionCount = FMath::Min(PartitionCount, TransformIndices.Num());

	// Set initial partition centers as selects from the vertex set
	PartitionCenters.SetNum(PartitionCount);
	int32 TransformStride = FMath::Max(1,FMath::FloorToInt(float(TransformIndices.Num()) / float(PartitionCount)));
	for (int32 PartitionIndex = 0; PartitionIndex < PartitionCount; ++PartitionIndex)
	{
		PartitionCenters[PartitionIndex] = Centroids[TransformStride * PartitionIndex];
	}

	// At beginning, all nodes belong to first partition.
	Partitions.Init(0, TransformIndices.Num());
	PartitionSize.Init(0, PartitionCount);

	if (PartitionCount > 0)
	{
		PartitionSize[0] = TransformIndices.Num();
	}
}

bool FVoronoiPartitioner::Refine()
{
	// Assign each transform to its closest partition center.
	bool bUnchanged = true;
	for (int32 Index = 0; Index < Centroids.Num(); ++Index)
	{
		int32 ClosestPartition = FindClosestPartitionCenter(Centroids[Index]);
		if (ClosestPartition != Partitions[Index])
		{
			bUnchanged = false;
			PartitionSize[Partitions[Index]]--;
			PartitionSize[ClosestPartition]++;
			Partitions[Index] = ClosestPartition;
		}
	}

	if (bUnchanged)
	{
		return false;
	}

	// Recalculate partition centers
	for (int32 PartitionIndex = 0; PartitionIndex < PartitionCount; ++PartitionIndex)
	{
		if (PartitionSize[PartitionIndex] > 0)
		{
			PartitionCenters[PartitionIndex] = FVector(0.0f, 0.0f, 0.0f);
		}
	}

	for (int32 Index = 0; Index < Partitions.Num(); ++Index)
	{
		PartitionCenters[Partitions[Index]] += Centroids[Index];
	}

	for (int32 PartitionIndex = 0; PartitionIndex < PartitionCount; ++PartitionIndex)
	{
		if (PartitionSize[PartitionIndex] > 0)
		{
			PartitionCenters[PartitionIndex] /= float(PartitionSize[PartitionIndex]);
		}
	}

	return true;
}

int32 FVoronoiPartitioner::FindClosestPartitionCenter(const FVector& Location) const
{
	int32 ClosestPartition = INDEX_NONE;
	float SmallestDistSquared = TNumericLimits<float>::Max();

	for (int32 PartitionIndex = 0; PartitionIndex < PartitionCount; ++PartitionIndex)
	{
		float DistSquared = FVector::DistSquared(Location, PartitionCenters[PartitionIndex]);
		if (DistSquared < SmallestDistSquared)
		{
			SmallestDistSquared = DistSquared;
			ClosestPartition = PartitionIndex;
		}
	}

	check(ClosestPartition > INDEX_NONE);
	return ClosestPartition;
}

class FFractureEngineBoneSelection
{
public:
	FFractureEngineBoneSelection(const FGeometryCollection& InGeometryCollection, const TArray<int32>& InSelectedBones)
		: GeometryCollection(InGeometryCollection)
		, SelectedBones(InSelectedBones)
	{}

	const TArray<int32>& GetSelectedBones() const { return SelectedBones;  }
	void ConvertSelectionToClusterNodes();

private:
	bool IsValidBone(int32 Index) const;
	bool HasSelectedAncestor(int32 Index) const;
	void Sanitize(bool bFavorParents = true);

private:
	const FGeometryCollection& GeometryCollection;
	TArray<int32> SelectedBones;
};

bool FFractureEngineBoneSelection::IsValidBone(int32 Index) const
{
	return Index >= 0 && Index < GeometryCollection.Parent.Num();
}

bool FFractureEngineBoneSelection::HasSelectedAncestor(int32 Index) const
{
	const TManagedArray<int32>& Parents = GeometryCollection.GetAttribute<int32>("Parent", FGeometryCollection::TransformGroup);

	if (!ensureMsgf(Index >= 0 && Index < Parents.Num(), TEXT("Invalid index in selection: %d"), Index))
	{
		return false;
	}

	int32 CurrIndex = Index;
	while (Parents[CurrIndex] != INDEX_NONE)
	{
		CurrIndex = Parents[CurrIndex];
		if (SelectedBones.Contains(CurrIndex))
		{
			return true;
		}
	}

	// We've arrived at the top of the hierarchy with no selected ancestors
	return false;
}

void FFractureEngineBoneSelection::Sanitize(bool bFavorParents)
{
	// Ensure that selected indices are valid
	int NumTransforms = GeometryCollection.NumElements(FGeometryCollection::TransformGroup);
	SelectedBones.RemoveAll([NumTransforms](int32 Index) {
		return Index == INDEX_NONE || !ensure(Index < NumTransforms);
	});

	// Ensure that children of a selected node are not also selected.
	if (bFavorParents)
	{
		SelectedBones.RemoveAll([this](int32 Index) {
			return !IsValidBone(Index) || HasSelectedAncestor(Index);
		});
	}

	SelectedBones.Sort();
}

void FFractureEngineBoneSelection::ConvertSelectionToClusterNodes()
{
	// If this is a non-cluster node, select the cluster containing it instead.
	const TManagedArray<TSet<int32>>& Children = GeometryCollection.Children;
	const TManagedArray<int32>& Parents = GeometryCollection.Parent;
	const TManagedArray<int32>& SimulationType = GeometryCollection.SimulationType;

	Sanitize();

	TArray<int32> AddedClusterSelections;
	for (int32 Index : SelectedBones)
	{
		int32 AddParent = FGeometryCollection::Invalid;
		if (SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid)
		{
			AddParent = Parents[Index];
		}
		else if (SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_None)
		{
			AddParent = Parents[Parents[Index]];
		}
		if (AddParent != FGeometryCollection::Invalid)
		{
			AddedClusterSelections.AddUnique(AddParent);
		}
	}
	SelectedBones.Append(AddedClusterSelections);

	Sanitize();
}

void FFractureEngineClustering::AutoCluster(FGeometryCollection& GeometryCollection,
	const TArray<int32>& BoneIndices,
	const EFractureEngineClusterSizeMethod ClusterSizeMethod,
	const uint32 SiteCount,
	const float SiteCountFraction,
	const float SiteSize,
	const bool bEnforceConnectivity,
	const bool bAvoidIsolated)
{
	FFractureEngineBoneSelection Selection(GeometryCollection, BoneIndices);

	Selection.ConvertSelectionToClusterNodes();

	for (const int32 ClusterIndex : Selection.GetSelectedBones())
	{
		AutoCluster(GeometryCollection, ClusterIndex, ClusterSizeMethod, SiteCount, SiteCountFraction, SiteSize, bEnforceConnectivity, bAvoidIsolated);
	}
}

void FFractureEngineClustering::AutoCluster(FGeometryCollection& GeometryCollection,
	const int32 ClusterIndex,
	const EFractureEngineClusterSizeMethod ClusterSizeMethod,
	const uint32 SiteCount,
	const float SiteCountFraction,
	const float SiteSize,
	const bool bEnforceConnectivity,
	const bool bAvoidIsolated)
{
	FVoronoiPartitioner VoronoiPartition(&GeometryCollection, ClusterIndex);
	int32 NumChildren = GeometryCollection.Children[ClusterIndex].Num();

	int32 SiteCountToUse = 1;
	if (ClusterSizeMethod == EFractureEngineClusterSizeMethod::ByNumber)
	{
		SiteCountToUse = SiteCount;
	}
	else if (ClusterSizeMethod == EFractureEngineClusterSizeMethod::ByFractionOfInput)
	{
		SiteCountToUse = FMath::Max(2, NumChildren * SiteCountFraction);
	}
	else if (ClusterSizeMethod == EFractureEngineClusterSizeMethod::BySize)
	{
		float TotalVolume = GeometryCollection.GetBoundingBox().GetBox().GetVolume();
		float DesiredVolume = FMath::Pow(SiteSize, 3);
		SiteCountToUse = FMath::Max(1, TotalVolume / DesiredVolume);
	}

	VoronoiPartition.KMeansPartition(SiteCountToUse);
	if (VoronoiPartition.GetPartitionCount() == 0)
	{
		return;
	}

	bool bNeedProximity = bEnforceConnectivity || bAvoidIsolated;
	if (bNeedProximity)
	{
		FGeometryCollectionProximityUtility ProximityUtility(&GeometryCollection);
		ProximityUtility.UpdateProximity();
	}

	if (bEnforceConnectivity)
	{
		VoronoiPartition.SplitDisconnectedPartitions(&GeometryCollection);
	}

	if (bAvoidIsolated)
	{
		// attempt to remove isolated via merging (may not succeed, as it only merges if there is a cluster in proximity)
		VoronoiPartition.MergeSingleElementPartitions(&GeometryCollection);
	}

	int32 NonEmptyPartitionCount = VoronoiPartition.GetNonEmptyPartitionCount();
	bool bHasEmptyClusters = NonEmptyPartitionCount > 0;

	if (bAvoidIsolated && NonEmptyPartitionCount == 1)
	{
		return;
	}

	int32 PartitionCount = VoronoiPartition.GetPartitionCount();
	int32 NewClusterIndexStart = GeometryCollection.AddElements(PartitionCount, FGeometryCollection::TransformGroup);

	for (int32 Index = 0; Index < PartitionCount; ++Index)
	{
		TArray<int32> NewCluster = VoronoiPartition.GetPartition(Index);
		if (bAvoidIsolated && NewCluster.Num() == 1)
		{
			bHasEmptyClusters = true;
			NewCluster.Reset();
		}

		int32 NewClusterIndex = NewClusterIndexStart + Index;
		GeometryCollection.Parent[NewClusterIndex] = ClusterIndex;
		GeometryCollection.Children[ClusterIndex].Add(NewClusterIndex);
		GeometryCollection.BoneName[NewClusterIndex] = "ClusterBone";
		GeometryCollection.Children[NewClusterIndex] = TSet<int32>(NewCluster);
		GeometryCollection.SimulationType[NewClusterIndex] = FGeometryCollection::ESimulationTypes::FST_Clustered;
		GeometryCollection.Transform[NewClusterIndex] = FTransform::Identity;
		GeometryCollectionAlgo::ParentTransforms(&GeometryCollection, NewClusterIndex, NewCluster);
	}
	if (bHasEmptyClusters)
	{
		FGeometryCollectionClusteringUtility::RemoveDanglingClusters(&GeometryCollection);
	}
	FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(&GeometryCollection, ClusterIndex);
	FGeometryCollectionClusteringUtility::RecursivelyUpdateChildBoneNames(ClusterIndex, GeometryCollection.Children, GeometryCollection.BoneName);
	FGeometryCollectionClusteringUtility::ValidateResults(&GeometryCollection);
}




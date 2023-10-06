// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureEngineClustering.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/Facades/CollectionTransformSelectionFacade.h"
#include "GeometryCollection/Facades/CollectionTransformFacade.h"
#include "GeometryCollection/Facades/CollectionHierarchyFacade.h"
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

void FVoronoiPartitioner::KMeansPartition(int32 InPartitionCount, int32 MaxIterations, TArrayView<const FVector> InitialCenters)
{
	PartitionCount = InPartitionCount;
	if (InitialCenters.Num())
	{
		PartitionCount = InitialCenters.Num();
	}

	InitializePartitions(InitialCenters);
	// Note: We run max iterations + 1 here because the first iteration just applies the initial cluster assignment
	for (int32 Iteration = 0; Iteration < MaxIterations + 1; Iteration++)
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

void FVoronoiPartitioner::MergeSmallPartitions(FGeometryCollection* GeometryCollection, float SizeThreshold)
{
	if (Connectivity.IsEmpty())
	{
		GenerateConnectivity(GeometryCollection);
	}

	FGeometryCollectionConvexUtility::SetVolumeAttributes(GeometryCollection);
	const TManagedArray<float>& Volumes = GeometryCollection->GetAttribute<float>("Volume", FTransformCollection::TransformGroup);

	float VolumeThreshold = SizeThreshold * SizeThreshold * SizeThreshold;
	TArray<float> PartitionVolumes;
	PartitionVolumes.Init(0, PartitionSize.Num());
	
	int32 NonEmptyPartitions = GetNonEmptyPartitionCount();

	for (int32 ElIdx = 0; ElIdx < TransformIndices.Num(); ElIdx++)
	{
		int32 Partition = Partitions[ElIdx];
		PartitionVolumes[Partition] += Volumes[TransformIndices[ElIdx]];
	}

	TArray<int32> ToConsider;
	ToConsider.Reserve(Partitions.Num());
	for (int32 Partition = 0; Partition < PartitionVolumes.Num(); ++Partition)
	{
		if (PartitionVolumes[Partition] < VolumeThreshold)
		{
			ToConsider.Emplace(Partition);
		}
	}
	while (!ToConsider.IsEmpty())
	{
		int32 Partition = ToConsider.Pop(false);
		if (NonEmptyPartitions < 3)
		{
			break; // if we only have two partitions, stop merging
		}
		if (PartitionVolumes[Partition] < VolumeThreshold)
		{
			int32 SmallestNbrPartition = INDEX_NONE;
			float SmallestNbrVolume = FMathf::MaxReal;
			for (int32 ElIdx = 0; ElIdx < TransformIndices.Num(); ++ElIdx)
			{
				if (Partitions[ElIdx] == Partition)
				{
					for (int32 NbrEl : Connectivity[ElIdx])
					{
						int32 NbrPartition = Partitions[NbrEl];
						if (NbrPartition != Partition)
						{
							if (SmallestNbrPartition == INDEX_NONE || PartitionVolumes[NbrPartition] < SmallestNbrVolume)
							{
								SmallestNbrVolume = PartitionVolumes[NbrPartition];
								SmallestNbrPartition = NbrPartition;
							}
						}
					}
				}
			}
			if (SmallestNbrPartition != INDEX_NONE)
			{
				PartitionVolumes[SmallestNbrPartition] += PartitionVolumes[Partition];
				PartitionVolumes[Partition] = 0;
				float CombinedSize = PartitionSize[SmallestNbrPartition] + PartitionSize[Partition];
				PartitionSize[SmallestNbrPartition] = CombinedSize;
				if (CombinedSize < VolumeThreshold)
				{
					ToConsider.Push(SmallestNbrPartition);
					Swap(ToConsider.Last(), ToConsider[0]); // make sure we don't always merge to the same cluster
				}
				PartitionSize[Partition] = 0;
				NonEmptyPartitions--;
				for (int32 ElIdx = 0; ElIdx < TransformIndices.Num(); ++ElIdx)
				{
					if (Partitions[ElIdx] == Partition)
					{
						Partitions[ElIdx] = SmallestNbrPartition;
					}
				}
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

FBox FVoronoiPartitioner::GenerateBounds(const FGeometryCollection* GeometryCollection, int32 TransformIndex)
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

void FVoronoiPartitioner::InitializePartitions(TArrayView<const FVector> InitialCenters)
{
	check(PartitionCount > 0);

	if (InitialCenters.Num() > 0)
	{
		PartitionCenters.Reset();
		PartitionCenters.Append(InitialCenters);
	}
	else
	{
		PartitionCount = FMath::Min(PartitionCount, TransformIndices.Num());

		// Set initial partition centers as selects from the vertex set
		PartitionCenters.SetNum(PartitionCount);
		int32 TransformStride = FMath::Max(1, FMath::FloorToInt(float(TransformIndices.Num()) / float(PartitionCount)));
		for (int32 PartitionIndex = 0; PartitionIndex < PartitionCount; ++PartitionIndex)
		{
			PartitionCenters[PartitionIndex] = Centroids[TransformStride * PartitionIndex];
		}
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
	const bool bAvoidIsolated,
	const bool bEnforceSiteParameters,
	const int32 GridX,
	const int32 GridY,
	const int32 GridZ,
	const float MinimumClusterSize,
	const int32 KMeansIterations)
{
	FFractureEngineBoneSelection Selection(GeometryCollection, BoneIndices);

	Selection.ConvertSelectionToClusterNodes();

	for (const int32 ClusterIndex : Selection.GetSelectedBones())
	{
		AutoCluster(GeometryCollection, ClusterIndex, ClusterSizeMethod, SiteCount, SiteCountFraction, SiteSize, 
			bEnforceConnectivity, bAvoidIsolated, bEnforceSiteParameters, GridX, GridY, GridZ, MinimumClusterSize, KMeansIterations);
	}
}

void FFractureEngineClustering::AutoCluster(FGeometryCollection& GeometryCollection,
	const int32 ClusterIndex,
	const EFractureEngineClusterSizeMethod ClusterSizeMethod,
	const uint32 SiteCount,
	const float SiteCountFraction,
	const float SiteSize,
	const bool bEnforceConnectivity,
	const bool bAvoidIsolated,
	const bool bEnforceSiteParameters,
	const int32 InGridX,
	const int32 InGridY,
	const int32 InGridZ,
	const float MinimumClusterSize,
	const int32 KMeansIterations)
{
	FVoronoiPartitioner VoronoiPartition(&GeometryCollection, ClusterIndex);
	int32 NumChildren = GeometryCollection.Children[ClusterIndex].Num();

	// Used by the ByGrid method, which manually distributes initial positions
	TArray<FVector> PartitionPositions;

	int32 DesiredSiteCountToUse = 1;
	int32 IterationsToUse = KMeansIterations;
	if (ClusterSizeMethod == EFractureEngineClusterSizeMethod::ByNumber)
	{
		DesiredSiteCountToUse = SiteCount;
	}
	else if (ClusterSizeMethod == EFractureEngineClusterSizeMethod::ByFractionOfInput)
	{
		DesiredSiteCountToUse = FMath::Max(2, NumChildren * SiteCountFraction);
	}
	else if (ClusterSizeMethod == EFractureEngineClusterSizeMethod::BySize)
	{
		float TotalVolume = GeometryCollection.GetBoundingBox().GetBox().GetVolume();
		float DesiredVolume = FMath::Pow(SiteSize, 3);
		DesiredSiteCountToUse = FMath::Max(1, TotalVolume / DesiredVolume);
	}
	else if (ClusterSizeMethod == EFractureEngineClusterSizeMethod::ByGrid)
	{
		PartitionPositions = GenerateGridSites(GeometryCollection, ClusterIndex, InGridX, InGridY, InGridZ);
	}

	int32 SiteCountToUse = DesiredSiteCountToUse;
	int32 PreviousPartitionCount = TNumericLimits<int32>::Max();
	bool bIterate = false;
	do
	{
		VoronoiPartition.KMeansPartition(SiteCountToUse, IterationsToUse, PartitionPositions);
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

		// Note: Previously if bAvoidIsolated was set, we'd attempt to merge clusters of one here via
		//  VoronoiPartition.MergeSingleElementPartitions(&GeometryCollection);
		// However this results in some overly-large clusters, especially with grid clustering,
		// so we prefer to simply skip clustering in such cases.

		if (MinimumClusterSize > 0)
		{
			// attempt to remove isolated via cluster merging (may not succeed, as it only merges if there is a cluster in proximity)
			VoronoiPartition.MergeSmallPartitions(&GeometryCollection, MinimumClusterSize);
		}
		const int32 IsolatedPartitionToIgnore = bAvoidIsolated ? VoronoiPartition.GetIsolatedPartitionCount() : 0;
		const int32 PartitionCount = VoronoiPartition.GetNonEmptyPartitionCount() - IsolatedPartitionToIgnore;
		bIterate = bEnforceSiteParameters				 // Is the feature enabled?
			&& PartitionPositions.IsEmpty()				 // Is the explicit position array empty? (otherwise, site count is ignored)
			&& (PartitionCount > DesiredSiteCountToUse)  // Have we reached the desired outcome
			&& (SiteCountToUse > 1)						 // Is SiteCount large enough ?
			&& (PartitionCount < PreviousPartitionCount);// Are we progressing ?
		if(bIterate)
		{
			SiteCountToUse--;
		}
	} while (bIterate);

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

TArray<FVector> FFractureEngineClustering::GenerateGridSites(
	const FGeometryCollection& GeometryCollection,
	const TArray<int32>& BoneIndices,
	const int32 GridX,
	const int32 GridY,
	const int32 GridZ)
{
	TArray<FVector> AllPoints;
	FFractureEngineBoneSelection Selection(GeometryCollection, BoneIndices);

	Selection.ConvertSelectionToClusterNodes();

	for (const int32 ClusterIndex : Selection.GetSelectedBones())
	{
		AllPoints.Append(GenerateGridSites(GeometryCollection, ClusterIndex, GridX, GridY, GridZ));
	}
	return AllPoints;
}

TArray<FVector> FFractureEngineClustering::GenerateGridSites(
	const FGeometryCollection& GeometryCollection,
	const int32 ClusterIndex,
	const int32 InGridX,
	const int32 InGridY,
	const int32 InGridZ,
	FBox* OutBounds)
{
	TArray<FVector> PartitionPositions;

	FBox Bounds = FVoronoiPartitioner::GenerateBounds(&GeometryCollection, ClusterIndex);
	if (OutBounds)
	{
		*OutBounds = Bounds;
	}
	int32 GridX = FMath::Max(1, InGridX);
	int32 GridY = FMath::Max(1, InGridY);
	int32 GridZ = FMath::Max(1, InGridZ);
	PartitionPositions.Reserve(GridX * GridY * GridZ);

	auto StepToParam = [](int32 Step, int32 NumSteps) -> double
	{
		return (double(Step) + .5) / double(NumSteps);
	};

	for (int32 StepX = 0; StepX < GridX; ++StepX)
	{
		double Xt = StepToParam(StepX, GridX);
		double X = FMath::Lerp(Bounds.Min.X, Bounds.Max.X, Xt);
		for (int32 StepY = 0; StepY < GridY; ++StepY)
		{
			double Yt = StepToParam(StepY, GridY);
			double Y = FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, Yt);
			for (int32 StepZ = 0; StepZ < GridZ; ++StepZ)
			{
				double Zt = StepToParam(StepZ, GridZ);
				double Z = FMath::Lerp(Bounds.Min.Z, Bounds.Max.Z, Zt);
				PartitionPositions.Emplace(X, Y, Z);
			}
		}
	}

	return PartitionPositions;
}


bool FFractureEngineClustering::ClusterSelected(
	FGeometryCollection& GeometryCollection,
	TArray<int32>& Selection)
{
	GeometryCollection::Facades::FCollectionTransformSelectionFacade SelectionFacade(GeometryCollection);
	SelectionFacade.RemoveRootNodes(Selection);
	SelectionFacade.Sanitize(Selection);

	if (Selection.Num() <= 1)
	{
		// Don't make a cluster of 1
		return false;
	}

	Chaos::Facades::FCollectionHierarchyFacade HierarchyFacade(GeometryCollection);
	int32 StartTransformCount = GeometryCollection.NumElements(FGeometryCollection::TransformGroup);
	int32 LowestCommonAncestor = FGeometryCollectionClusteringUtility::FindLowestCommonAncestor(&GeometryCollection, Selection);
	// Cluster selected bones beneath common parent
	if (LowestCommonAncestor != INDEX_NONE)
	{
		FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNodeWithParent(&GeometryCollection, LowestCommonAncestor, Selection, true);
		GeometryCollection::GenerateTemporaryGuids(&GeometryCollection, StartTransformCount, true);
		FGeometryCollectionClusteringUtility::RemoveDanglingClusters(&GeometryCollection);
		HierarchyFacade.GenerateLevelAttribute();

		return true;
	}

	return false;
}

bool FFractureEngineClustering::MergeSelectedClusters(FGeometryCollection& GeometryCollection, TArray<int32>& Selection)
{
	Chaos::Facades::FCollectionHierarchyFacade HierarchyFacade(GeometryCollection);

	GeometryCollection::Facades::FCollectionTransformSelectionFacade SelectionFacade(GeometryCollection);
	SelectionFacade.ConvertEmbeddedSelectionToParents(Selection); // embedded geo must stay attached to parent

	// Collect children of context clusters
	TArray<int32> ChildBones;
	int32 StartTransformCount = GeometryCollection.NumElements(FGeometryCollection::TransformGroup);
	ChildBones.Reserve(StartTransformCount);
	bool bHasClusters = false;
	for (int32 Select : Selection)
	{
		if (GeometryCollection.SimulationType[Select] == FGeometryCollection::ESimulationTypes::FST_Clustered)
		{
			bHasClusters = true;
			ChildBones.Append(HierarchyFacade.GetChildrenAsArray(Select));
		}
		else
		{
			ChildBones.Add(Select);
		}
	}

	// If there were no clusters in the selection, we create a cluster
	if (!bHasClusters)
	{
		// Cluster selected bones beneath common parent
		int32 LowestCommonAncestor = FGeometryCollectionClusteringUtility::FindLowestCommonAncestor(&GeometryCollection, Selection);
		if (LowestCommonAncestor != INDEX_NONE)
		{
			FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNodeWithParent(&GeometryCollection, LowestCommonAncestor, Selection, true);
			GeometryCollection::GenerateTemporaryGuids(&GeometryCollection, StartTransformCount, true);
			Selection.SetNum(1);
			Selection[0] = LowestCommonAncestor;
			if (FGeometryCollectionClusteringUtility::RemoveDanglingClusters(&GeometryCollection))
			{
				// Bone index is unreliable after removal of some bones
				Selection.Empty();
			}
			HierarchyFacade.GenerateLevelAttribute();
			return true;
		}
	}
	else
	{
		int32 MergeNode = FGeometryCollectionClusteringUtility::PickBestNodeToMergeTo(&GeometryCollection, Selection);
		if (MergeNode >= 0)
		{
			FGeometryCollectionClusteringUtility::ClusterBonesUnderExistingNode(&GeometryCollection, MergeNode, ChildBones);
			Selection.SetNum(1);
			Selection[0] = MergeNode;
			if (FGeometryCollectionClusteringUtility::RemoveDanglingClusters(&GeometryCollection))
			{
				// Bone index is unreliable after removal of some bones
				Selection.Empty();
			}
			HierarchyFacade.GenerateLevelAttribute();
			return true;
		}
	}

	return false;
}

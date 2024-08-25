// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/Count.h"
#include "Math/Box.h"

class FGeometryCollection;


class FRACTUREENGINE_API FVoronoiPartitioner
{
public:
	FVoronoiPartitioner(const FGeometryCollection* GeometryCollection, int32 ClusterIndex);

	/** 
	 * Cluster bodies into k partitions using K-Means. Connectivity is ignored: only spatial proximity is considered. 
	 * @param InPartitionCount	Number of partitions to target, if InitialCenters is not provided
	 * @param MaxIterations		Maximum iterations of refinement of partitions. In many cases, K-Means will converge and stop early if MaxIterations is large.
	 * @param InitialCenters	If non-empty, these positions will be used to initialize the partition locations. The target partition count will then be the length of this array.
	 */
	void KMeansPartition(int32 InPartitionCount, int32 MaxIterations = 500, TArrayView<const FVector> InitialCenters = TArrayView<const FVector>());

	/** Split any partition islands into their own partition. This will possbily increase number of partitions to exceed desired count. */
	void SplitDisconnectedPartitions(FGeometryCollection* GeometryCollection);

	/** Merge any partitions w/ only 1 body into a connected, neighboring partition (if any).  This can decrease the number of partitions below the desired count. */
	void MergeSingleElementPartitions(FGeometryCollection* GeometryCollection);

	/** Merge any too-small partitions into a connected, neighboring partition (if any).  This can decrease the number of partitions below the desired count. */
	void MergeSmallPartitions(FGeometryCollection* GeometryCollection, float PartitionSizeThreshold);

	int32 GetPartitionCount() const { return PartitionCount; }

	int32 GetNonEmptyPartitionCount() const
	{
		return PartitionSize.Num() - Algo::Count(PartitionSize, 0);
	}

	int32 GetIsolatedPartitionCount() const
	{
		return Algo::Count(PartitionSize, 1);
	}

	/** return the GeometryCollection TranformIndices within the partition. */
	TArray<int32> GetPartition(int32 PartitionIndex) const;

	static FBox GenerateBounds(const FGeometryCollection* GeometryCollection, int32 TransformIndex);

private:
	void GenerateConnectivity(const FGeometryCollection* GeometryCollection);
	void CollectConnections(const FGeometryCollection* GeometryCollection, int32 Index, int32 OperatingLevel, TSet<int32>& OutConnections) const;
	void GenerateCentroids(const FGeometryCollection* GeometryCollection);
	FVector GenerateCentroid(const FGeometryCollection* GeometryCollection, int32 TransformIndex) const;
	void InitializePartitions(TArrayView<const FVector> InitialCenters = TArrayView<const FVector>());
	bool Refine();
	int32 FindClosestPartitionCenter(const FVector& Location) const;
	void MarkVisited(int32 Index, int32 PartitionIndex);

private:
	TArray<int32> TransformIndices;
	TArray<FVector> Centroids;
	// mapping from index into TransformIndices to partition number
	TArray<int32> Partitions;
	int32 PartitionCount;
	TArray<int32> PartitionSize;
	TArray<FVector> PartitionCenters;
	// mapping from index into TransformIndices to the set of connected transforms (also via their index in TransformIndices)
	TArray<TSet<int32>> Connectivity;
	TArray<bool> Visited;
};

enum class FRACTUREENGINE_API EFractureEngineClusterSizeMethod : uint8
{
	// Cluster by specifying an absolute number of clusters
	ByNumber,
	// Cluster by specifying a fraction of the number of input bones
	ByFractionOfInput,
	// Cluster by specifying the density of the input bones
	BySize,
	// Cluster by a regular grid distribution
	ByGrid,
};

class FRACTUREENGINE_API FFractureEngineClustering
{
public:

	static void AutoCluster(FGeometryCollection& GeometryCollection,
		const TArray<int32>& BoneIndices,
		const EFractureEngineClusterSizeMethod ClusterSizeMethod,
		const uint32 SiteCount,
		const float SiteCountFraction,
		const float SiteSize,
		const bool bEnforceConnectivity,
		const bool bAvoidIsolated,
		const bool bEnforceSiteParameters,
		const int32 GridX = 2,
		const int32 GridY = 2,
		const int32 GridZ = 2,
		const float MinimumClusterSize = 0,
		const int32 KMeansIterations = 500,
		const bool bPreferConvexity = false,
		const float ConcavityErrorTolerance = 0);

	static void AutoCluster(FGeometryCollection& GeometryCollection,
		const int32 ClusterIndex,
		const EFractureEngineClusterSizeMethod ClusterSizeMethod,
		const uint32 SiteCount,
		const float SiteCountFraction,
		const float SiteSize,
		const bool bEnforceConnectivity,
		const bool bAvoidIsolated,
		const bool bEnforceSiteParameters,
		const int32 GridX = 2,
		const int32 GridY = 2,
		const int32 GridZ = 2,
		const float MinimumClusterSize = 0,
		const int32 KMeansIterations = 500,
		const bool bPreferConvexity = false,
		const float ConcavityErrorTolerance = 0);

	// Autoclustering that favors convex-shaped clusters
	static void ConvexityBasedCluster(FGeometryCollection& GeometryCollection,
		int32 ClusterIndex,
		uint32 SiteCount,
		bool bEnforceConnectivity,
		bool bAvoidIsolated,
		float ConcavityErrorTolerance);

	static TArray<FVector> GenerateGridSites(
		const FGeometryCollection& GeometryCollection,
		const TArray<int32>& BoneIndices,
		const int32 GridX,
		const int32 GridY,
		const int32 GridZ);

	static TArray<FVector> GenerateGridSites(
		const FGeometryCollection& GeometryCollection,
		const int32 ClusterIndex,
		const int32 GridX,
		const int32 GridY,
		const int32 GridZ,
		FBox* OutBounds = nullptr);

	// Cluster the chosen transform indices (and update the selection array to remove any that were not clustered, i.e. invalid or root transforms)
	// @return true if the GeometryCollection was updated
	static bool ClusterSelected(
		FGeometryCollection& GeometryCollection,
		TArray<int32>& InOutSelection
	);

	// Merge selected clusters. Non-clusters in the selection are converted to the closest (parent) clusters.
	// On success, returns true and InOutSelection holds the index of the cluster to which the selection was merged.
	static bool MergeSelectedClusters(
		FGeometryCollection& GeometryCollection,
		TArray<int32>& InOutSelection
	);

	// Merge neighbors, and neighbors of neighbors (out to the Iterations number) to the selected clusters.
	static bool ClusterMagnet(
		FGeometryCollection& GeometryCollection,
		TArray<int32>& InOutSelection,
		int32 Iterations
	);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "GeometryCollection/GeometryCollection.h"
#endif

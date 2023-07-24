// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/Count.h"
#include "Math/Box.h"

class FGeometryCollection;


class FRACTUREENGINE_API FVoronoiPartitioner
{
public:
	FVoronoiPartitioner(const FGeometryCollection* GeometryCollection, int32 ClusterIndex);

	/** Cluster bodies into k partitions using K-Means. Connectivity is ignored: only spatial proximity is considered. */
	void KMeansPartition(int32 InPartitionCount);

	/** Split any partition islands into their own partition. This will possbily increase number of partitions to exceed desired count. */
	void SplitDisconnectedPartitions(FGeometryCollection* GeometryCollection);

	/** Merge any partitions w/ only 1 body into a connected, neighboring partition (if any).  This can decrease the number of partitions below the desired count. */
	void MergeSingleElementPartitions(FGeometryCollection* GeometryCollection);

	int32 GetPartitionCount() const { return PartitionCount; }

	int32 GetNonEmptyPartitionCount() const
	{
		return PartitionSize.Num() - Algo::Count(PartitionSize, 0);
	}

	/** return the GeometryCollection TranformIndices within the partition. */
	TArray<int32> GetPartition(int32 PartitionIndex) const;

private:
	void GenerateConnectivity(const FGeometryCollection* GeometryCollection);
	void CollectConnections(const FGeometryCollection* GeometryCollection, int32 Index, int32 OperatingLevel, TSet<int32>& OutConnections) const;
	void GenerateCentroids(const FGeometryCollection* GeometryCollection);
	FVector GenerateCentroid(const FGeometryCollection* GeometryCollection, int32 TransformIndex) const;
	FBox GenerateBounds(const FGeometryCollection* GeometryCollection, int32 TransformIndex) const;
	void InitializePartitions();
	bool Refine();
	int32 FindClosestPartitionCenter(const FVector& Location) const;
	void MarkVisited(int32 Index, int32 PartitionIndex);

private:
	TArray<int32> TransformIndices;
	TArray<FVector> Centroids;
	TArray<int32> Partitions;
	int32 PartitionCount;
	TArray<int32> PartitionSize;
	TArray<FVector> PartitionCenters;
	TArray<TSet<int32>> Connectivity;
	TArray<bool> Visited;


	// Not generally necessary but this is a safety measure to prevent oscillating solves that never converge.
	const int32 MaxKMeansIterations = 500;
};

enum class FRACTUREENGINE_API EFractureEngineClusterSizeMethod : uint8
{
	// Cluster by specifying an absolute number of clusters
	ByNumber,
	// Cluster by specifying a fraction of the number of input bones
	ByFractionOfInput,
	// Cluster by specifying the density of the input bones
	BySize,
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
		const bool bAvoidIsolated);

	static void AutoCluster(FGeometryCollection& GeometryCollection,
		const int32 ClusterIndex,
		const EFractureEngineClusterSizeMethod ClusterSizeMethod,
		const uint32 SiteCount,
		const float SiteCountFraction,
		const float SiteSize,
		const bool bEnforceConnectivity,
		const bool bAvoidIsolated);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "GeometryCollection/GeometryCollection.h"
#endif

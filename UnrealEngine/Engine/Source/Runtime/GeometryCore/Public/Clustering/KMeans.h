// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/RandomStream.h"
#include "MathUtil.h"
#include "VectorTypes.h"
#include "MatrixTypes.h"
#include "Spatial/PriorityOrderPoints.h"

namespace UE
{
namespace Geometry
{

using namespace UE::Math;


struct FClusterKMeans
{
	/// Parameters

	// Max iterations of K-Means clustering. Will use fewer iterations if the clustering method converges.
	int32 MaxIterations = 500;

	// Random Seed used to initialize clustering (if InitialCenters are not provided)
	int32 RandomSeed = 0;

	/// Outputs

	// Mapping from input points to cluster IDs
	TArray<int32> ClusterIDs;

	// Number of points in each cluster
	TArray<int32> ClusterSizes;

	/**
	 * Compute the K-Means clustering of FVector points
	 * 
	 * @param PointsToCluster	Points to partition into clusters
	 * @param NumClusters		Target number of clusters to create, if InitialCenters is not provided
	 * @param InitialCenters	If non-empty, these positions will be used to initialize the cluster locations
	 * @param OutClusterCenters	If non-null, will be filled with the cluster centers
	 * @return number of clusters found
	 */
	template<typename TVectorType = FVector>
	int32 ComputeClusters(
		TArrayView<const TVectorType> PointsToCluster, int32 NumClusters,
		TArrayView<const TVectorType> InitialCenters = TArrayView<const TVectorType>(), 
		TArray<TVectorType>* OutClusterCenters = nullptr)
	{
		if (PointsToCluster.IsEmpty())
		{
			// nothing to cluster
			return 0;
		}

		// Initialize cluster centers
		TArray<TVectorType> LocalCenters;
		TArray<TVectorType>* UseCenters = &LocalCenters;
		if (OutClusterCenters)
		{
			UseCenters = OutClusterCenters;
			UseCenters->Reset();
		}
		if (InitialCenters.IsEmpty())
		{
			TArray<int32> Ordering;
			const int32 OrderingNum = PointsToCluster.Num();
			Ordering.SetNumUninitialized(OrderingNum);
			for (int32 Idx = 0; Idx < OrderingNum; ++Idx)
			{
				Ordering[Idx] = Idx;
			}
			// Shuffle the first NumClusters indices and use them as the initial centers
			NumClusters = FMath::Min(PointsToCluster.Num(), NumClusters);
			UseCenters->Reserve(NumClusters);
			FRandomStream RandomStream(RandomSeed);
			for (int32 Idx = 0; Idx < NumClusters; ++Idx)
			{
				Swap(Ordering[Idx], Ordering[Idx + RandomStream.RandHelper(OrderingNum - Idx)]);
				UseCenters->Add(PointsToCluster[Ordering[Idx]]);
			}
		}
		else
		{
			// Note: We intentionally do not check if more centers were provided than points here.
			// The excess centers will instead be removed below when no points are assigned to them.
			UseCenters->Append(InitialCenters);
		}

		NumClusters = UseCenters->Num();
		ClusterIDs.Init(-1, PointsToCluster.Num());
		ClusterSizes.SetNumZeroed(NumClusters);

		if (NumClusters == 0)
		{
			return NumClusters;
		}
		else if (NumClusters == 1)
		{
			for (int32 PointIdx = 0; PointIdx < PointsToCluster.Num(); ++PointIdx)
			{
				ClusterIDs[PointIdx] = 0;
			}
			ClusterSizes[0] = PointsToCluster.Num();
			return NumClusters;
		}

		
		TArray<TVectorType> NextCenters;
		NextCenters.SetNumZeroed(NumClusters);

		TArray<int32> ClusterIDRemap; // array to use if we need to remap cluster IDs due to an empty cluster

		int32 UseMaxIterations = FMath::Max(1, MaxIterations); // always use at least one iteration to make sure the initial assignment happens
		for (int32 Iterations = 0; Iterations < UseMaxIterations; ++Iterations)
		{
			bool bClustersChanged = Iterations == 0; // clusters always change on first iteration; otherwise we must check
			for (int32 PointIdx = 0; PointIdx < PointsToCluster.Num(); ++PointIdx)
			{
				const TVectorType& Point = PointsToCluster[PointIdx];
				double ClosestDistSq = (double)DistanceSquared(Point, (*UseCenters)[0]);
				int32 ClosestCenter = 0;
				for (int32 CenterIdx = 1; CenterIdx < UseCenters->Num(); ++CenterIdx)
				{
					double DistSq = DistanceSquared(PointsToCluster[PointIdx], (*UseCenters)[CenterIdx]);
					if (DistSq < ClosestDistSq)
					{
						ClosestDistSq = DistSq;
						ClosestCenter = CenterIdx;
					}
				}

				ClusterSizes[ClosestCenter]++;
				if (Iterations > 0)
				{
					int32 OldClusterID = ClusterIDs[PointIdx];
					ClusterSizes[OldClusterID]--;
					if (OldClusterID != ClosestCenter)
					{
						bClustersChanged = true;
					}
				}
				
				ClusterIDs[PointIdx] = ClosestCenter;
				NextCenters[ClosestCenter] += Point;
			}

			// Stop iterating if clusters are unchanged
			if (!bClustersChanged)
			{
				break;
			}

			// Update cluster centers and detect/delete any empty clusters
			bool bDeletedClusters = false;
			for (int32 ClusterIdx = 0; ClusterIdx < UseCenters->Num(); ++ClusterIdx)
			{
				if (ClusterSizes[ClusterIdx] > 0)
				{
					(*UseCenters)[ClusterIdx] = NextCenters[ClusterIdx] / ClusterSizes[ClusterIdx];
					NextCenters[ClusterIdx] = TVectorType::ZeroVector;
				}
				else
				{
					if (!bDeletedClusters)
					{
						ClusterIDRemap.SetNumUninitialized(UseCenters->Num(), EAllowShrinking::No);
						for (int32 Idx = 0; Idx < UseCenters->Num(); ++Idx)
						{
							ClusterIDRemap[Idx] = Idx;
						}
					}
					bDeletedClusters = true;
					ClusterIDRemap[ClusterSizes.Num() - 1] = ClusterIdx;
					ClusterSizes.RemoveAtSwap(ClusterIdx, 1, EAllowShrinking::No);
					UseCenters->RemoveAtSwap(ClusterIdx, 1, EAllowShrinking::No);
					NextCenters.RemoveAtSwap(ClusterIdx, 1, EAllowShrinking::No);
				}
			}
			if (bDeletedClusters)
			{
				checkSlow(!ClusterSizes.IsEmpty());
				checkSlow(ClusterSizes.Num() == UseCenters->Num());
				for (int32& ClusterID : ClusterIDs)
				{
					ClusterID = ClusterIDRemap[ClusterID];
				}
			}
		}

		return ClusterSizes.Num();
	}

	/**
	 * Helper function to generate (approximately) uniform-spaced initial clusters centers, which can be passed to ComputeClusters.
	 */
	template<typename TVectorType = FVector>
	void GetUniformSpacedInitialCenters(TArrayView<const TVectorType> PointsToCluster, int32 NumClusters, TArray<TVectorType>& OutCenters)
	{
		FPriorityOrderPoints OrderPoints;
		OrderPoints.ComputeUniformSpaced(PointsToCluster, TArrayView<const float>(), NumClusters);
		int32 NumOut = FMath::Min(NumClusters, OrderPoints.Order.Num());
		OutCenters.Reset(NumOut);
		for (int32 OrderIdx = 0; OrderIdx < NumOut; ++OrderIdx)
		{
			OutCenters.Add(PointsToCluster[OrderPoints.Order[OrderIdx]]);
		}
	}


	void GEOMETRYCORE_API GetClusters(TArray<TArray<int32>>& OutClusters);
};

} // end namespace UE::Geometry
} // end namespace UE
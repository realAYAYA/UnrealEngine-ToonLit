// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Containers/ArrayView.h"
#include "Algo/IsSorted.h"

class FArchive;

namespace UE::PoseSearch
{
struct FKDTreeImplementation;

struct POSESEARCH_API FKDTree
{
	struct DataSource
	{
		DataSource(int32 pointCount, int32 pointDim, const float* data)
		: PointCount(pointDim > 0 ? pointCount : 0)
		, PointDim(pointDim)
		, Data(data)
		{
		}

		// Must return the number of data points
		inline size_t kdtree_get_point_count() const { return PointCount; }

		// this method is called by nanoflann::findNeighbors -> nanoflann::searchLevel, so it has to be as fast as possible
		inline float kdtree_get_pt(const size_t idx, const size_t dim) const { return Data[idx * PointDim + dim]; }

		// Optional bounding-box computation: return false to default to a standard
		// bbox computation loop.
		//   Return true if the BBOX was already computed by the class and returned
		//   in "bb" so it can be avoided to redo it again. Look at bb.size() to
		//   find out the expected dimensionality (e.g. 2 or 3 for point clouds)
		template <class BBOX> bool kdtree_get_bbox(BBOX& /* bb */) const { return false; }

		int32 PointCount = 0;
		int32 PointDim = 0;
		const float* Data = nullptr;
	};

	struct KNNResultSet
	{
		inline KNNResultSet(size_t InNumNeighbors, TArrayView<size_t> InIndexes, TArrayView<float> InDistances, TConstArrayView<size_t> InExcludeFromSearchIndexes = TConstArrayView<size_t>())
		: Indexes(InIndexes)
		, Distances(InDistances)
		, NumNeighbors(InNumNeighbors)
		, Count(0)
		, ExcludeFromSearchIndexes(InExcludeFromSearchIndexes)
		{
			// by having IndexesView and DistancesView cardinality bigger than NumNeighbors, we can skip some if statements in the addPoint method
			check(NumNeighbors > 0);
			check(InIndexes.Num() > NumNeighbors);
			check(InDistances.Num() > NumNeighbors);
			check(Algo::IsSorted(InExcludeFromSearchIndexes));

			Distances[NumNeighbors - 1] = UE_BIG_NUMBER;
		}

		inline size_t Num() const
		{
			return Count;
		}

		inline bool full() const
		{
			return Count == NumNeighbors;
		}

		inline bool addPoint(size_t dist, size_t index)
		{
			if (Algo::BinarySearch(ExcludeFromSearchIndexes, index) != INDEX_NONE)
			{
				return true;
			}

			// shifting Distances[i] and Indexes[i] to make space for "dist" and "index" at the right "i"th slot
			size_t i;
			for (i = Count; (i > 0) && (Distances[i - 1] > dist); --i)
			{
				// no need to check "if (i < capacity)" since dists and indices can contains more items than capacity_ 
				Distances[i] = Distances[i - 1];
				Indexes[i] = Indexes[i - 1];
			}
			
			// inserting "dist" and "index" in a sorted manner
			// no need to check "if (i < capacity)" since dists and indices can contains more items than capacity_ 
			Distances[i] = dist;
			Indexes[i] = index;
			
			if (Count < NumNeighbors)
			{
				Count++;
			}

			// tell caller that the search shall continue
			return true;
		}

		inline float worstDist() const { return Distances[NumNeighbors - 1]; }

	private:
		TArrayView<size_t> Indexes;
		TArrayView<float> Distances;
		size_t NumNeighbors;
		size_t Count;
		TConstArrayView<size_t> ExcludeFromSearchIndexes; // sorted array view
	};
	

	FKDTree(int32 Count, int32 Dim, const float* Data, int32 MaxLeafSize);
	FKDTree();
	FKDTree(const FKDTree& r);
	FKDTree(FKDTree&& r) = delete;
	
	~FKDTree();

	FKDTree& operator=(const FKDTree& r);

	bool FindNeighbors(KNNResultSet& Result, const float* Query) const;
	void Construct(int32 Count, int32 Dim, const float* Data, int32 MaxLeafSize);
	SIZE_T GetAllocatedSize() const;

	DataSource DataSrc;
	FKDTreeImplementation* Impl = nullptr;
};

FArchive& Serialize(FArchive& Ar, FKDTree& KDTree, const float* data);
} // namespace UE::PoseSearch
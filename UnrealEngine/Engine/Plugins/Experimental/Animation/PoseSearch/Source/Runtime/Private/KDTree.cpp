// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/KDTree.h"
#include "Stats/Stats.h"

#ifndef UE_POSE_SEARCH_USE_NANOFLANN
	#define UE_POSE_SEARCH_USE_NANOFLANN 1
#endif

// @third party code - BEGIN nanoflann
#if UE_POSE_SEARCH_USE_NANOFLANN
THIRD_PARTY_INCLUDES_START
#include "nanoflann/nanoflann.hpp"
THIRD_PARTY_INCLUDES_END
#endif
// @third party code - END nanoflann

namespace UE::PoseSearch
{

#if UE_POSE_SEARCH_USE_NANOFLANN
using FKDTreeImplementationBase = nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<float, FKDTree::FDataSource>, FKDTree::FDataSource>;
struct FKDTreeImplementation : FKDTreeImplementationBase
{
	using FKDTreeImplementationBase::FKDTreeImplementationBase;
};
#endif

FKDTree::FKDTree(int32 Count, int32 Dim, const float* Data, int32 MaxLeafSize)
: DataSource(Count, Dim, Data)
, Impl(nullptr)
{
#if UE_POSE_SEARCH_USE_NANOFLANN
	if (Count > 0 && Dim > 0 && Data)
	{
		Impl = new FKDTreeImplementation(Dim, DataSource, nanoflann::KDTreeSingleIndexAdaptorParams(MaxLeafSize));
	}
#endif
}

FKDTree::FKDTree()
: DataSource(0, 0, nullptr)
, Impl(nullptr)
{
}

FKDTree::~FKDTree()
{
	Reset();
}

#if UE_POSE_SEARCH_USE_NANOFLANN
void CopySubTree(FKDTree& KDTree, FKDTreeImplementation::NodePtr& ThisNode, const FKDTreeImplementation::NodePtr& OtherNode)
{
	check(KDTree.Impl);

	ThisNode = KDTree.Impl->pool.template allocate<FKDTreeImplementation::Node>();

	ThisNode->node_type = OtherNode->node_type;

	if (OtherNode->child1 != nullptr)
	{
		CopySubTree(KDTree, ThisNode->child1, OtherNode->child1);
	}
	else
	{
		ThisNode->child1 = nullptr;
	}

	if (OtherNode->child2 != nullptr)
	{
		CopySubTree(KDTree, ThisNode->child2, OtherNode->child2);
	}
	else
	{
		ThisNode->child2 = nullptr;
	}
}

#endif

FKDTree::FKDTree(const FKDTree& Other)
{
#if UE_POSE_SEARCH_USE_NANOFLANN
	if (this != &Other && Other.Impl)
	{
		Impl = new FKDTreeImplementation(0, DataSource, nanoflann::KDTreeSingleIndexAdaptorParams(0));

		DataSource = Other.DataSource;

		check(Other.Impl->m_size < UINT_MAX);
		Impl->m_size = Other.Impl->m_size;

		if (Impl->m_size > 0)
		{
			Impl->dim = Other.Impl->dim;

			const uint32 root_bbox_size = Other.Impl->root_bbox.size();
			check(root_bbox_size < UINT_MAX);
			Impl->root_bbox.resize(root_bbox_size);

			for (uint32 i = 0; i < root_bbox_size; ++i)
			{
				Impl->root_bbox[i] = Other.Impl->root_bbox[i];
			}

			check(Other.Impl->m_leaf_max_size < UINT_MAX);
			const uint32 KDTreeLeafMaxSize = Other.Impl->m_leaf_max_size;
			Impl->m_leaf_max_size = KDTreeLeafMaxSize;

			const uint32 VAccSize = Other.Impl->vAcc.size();
			check(VAccSize < UINT_MAX);
			Impl->vAcc.resize(VAccSize);
			
			for (uint32 i = 0; i < VAccSize; ++i)
			{
				Impl->vAcc[i] = Other.Impl->vAcc[i];
			}
			
			CopySubTree(*this, Impl->root_node, Other.Impl->root_node);
		}
	}
#endif
}

FKDTree& FKDTree::operator=(const FKDTree& Other)
{
	if (this != &Other)
	{
		Reset();
		new(this)FKDTree(Other);
	}
	return *this;
}

void FKDTree::Reset()
{
#if UE_POSE_SEARCH_USE_NANOFLANN
	delete Impl;
	Impl = nullptr;
#endif
	DataSource = FDataSource();
}

void FKDTree::Construct(int32 Count, int32 Dim, const float* Data, int32 MaxLeafSize)
{
	Reset();
	new(this)FKDTree(Count, Dim, Data, MaxLeafSize);
}

bool FKDTree::FindNeighbors(KNNResultSet& Result, const float* Query) const
{
#if UE_POSE_SEARCH_USE_NANOFLANN
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FKDTree_FindNeighbors);

	check(Query && Impl->root_node);
	const nanoflann::SearchParams SearchParams(
		32,			// Ignored parameter (Kept for compatibility with the FLANN interface).
		0.f,		// search for eps-approximate neighbours (default: 0)
		false);		// only for radius search, require neighbours sorted by
	return Impl->findNeighbors(Result, Query, SearchParams);
#else
	checkNoEntry(); // unimplemented
	return false;
#endif
}

SIZE_T FKDTree::GetAllocatedSize() const
{
	SIZE_T AllocatedSize = sizeof(FKDTree);

#if UE_POSE_SEARCH_USE_NANOFLANN
	if (Impl)
	{
		AllocatedSize += sizeof(FKDTreeImplementation);
		AllocatedSize += Impl->usedMemory(*Impl);
	}
#endif

	return AllocatedSize;
}


#if UE_POSE_SEARCH_USE_NANOFLANN
FArchive& SerializeSubTree(FArchive& Ar, FKDTree& KDTree, FKDTreeImplementation::NodePtr& KDTreeNode)
{
	check(KDTree.Impl);

	if (Ar.IsLoading())
	{
		KDTreeNode = KDTree.Impl->pool.template allocate<FKDTreeImplementation::Node>();
	}

	Ar.Serialize(&KDTreeNode->node_type, sizeof(KDTreeNode->node_type));

	bool child1 = KDTreeNode->child1 != nullptr;
	bool child2 = KDTreeNode->child2 != nullptr;
	Ar << child1;
	Ar << child2;

	if (child1)
	{
		SerializeSubTree(Ar, KDTree, KDTreeNode->child1);
	}
	else if (Ar.IsLoading())
	{
		KDTreeNode->child1 = nullptr;
	}

	if (child2)
	{
		SerializeSubTree(Ar, KDTree, KDTreeNode->child2);
	}
	else if (Ar.IsLoading())
	{
		KDTreeNode->child2 = nullptr;
	}
	return Ar;
}

#endif

FArchive& Serialize(FArchive& Ar, FKDTree& KDTree, const float* KDTreeData)
{
#if UE_POSE_SEARCH_USE_NANOFLANN
	uint32 KDTreeSize = KDTree.Impl ? KDTree.Impl->m_size : 0;

	Ar << KDTreeSize;

	check(KDTreeSize < UINT_MAX);

	if (KDTreeSize > 0)
	{
		if (Ar.IsLoading() && !KDTree.Impl)
		{
			KDTree.Impl = new FKDTreeImplementation(0, KDTree.DataSource, nanoflann::KDTreeSingleIndexAdaptorParams(0));
		}

		KDTree.Impl->m_size = KDTreeSize;

		Ar << KDTree.Impl->dim;

		uint32 root_bbox_size = KDTree.Impl->root_bbox.size();
		check(KDTree.Impl->root_bbox.size() < UINT_MAX);
		Ar << root_bbox_size;

		if (Ar.IsLoading())
		{
			KDTree.DataSource.Data = KDTreeData;
			KDTree.DataSource.PointDim = KDTree.Impl->dim;
			KDTree.DataSource.PointCount = KDTree.Impl->m_size;

			KDTree.Impl->root_bbox.resize(root_bbox_size);
		}

		for (FKDTreeImplementation::Interval& el : KDTree.Impl->root_bbox)
		{
			Ar.Serialize(&el, sizeof(FKDTreeImplementation::Interval));
		}

		check(KDTree.Impl->m_leaf_max_size < UINT_MAX);
		uint32 KDTreeLeafMaxSize = KDTree.Impl->m_leaf_max_size;
		Ar << KDTreeLeafMaxSize;
		KDTree.Impl->m_leaf_max_size = KDTreeLeafMaxSize;

		check(KDTree.Impl->vAcc.size() < UINT_MAX);
		uint32 VAccSize = KDTree.Impl->vAcc.size();
		Ar << VAccSize;
		if (Ar.IsLoading())
		{
			KDTree.Impl->vAcc.resize(VAccSize);
		}
		for (uint32_t& el : KDTree.Impl->vAcc)
		{
			Ar << el;
		}
		SerializeSubTree(Ar, KDTree, KDTree.Impl->root_node);
	}
	else if (Ar.IsLoading())
	{
		KDTree.Reset();
	}
#endif

	return Ar;
}

} // namespace UE::PoseSearch

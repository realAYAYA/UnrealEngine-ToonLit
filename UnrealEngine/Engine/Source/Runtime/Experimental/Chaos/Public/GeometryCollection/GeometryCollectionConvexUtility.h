// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayCollection.h"

class FGeometryCollection;

namespace Chaos
{
	class FConvex;
}

UENUM()
enum class EConvexOverlapRemoval : int32
{
	// Do not remove overlaps between convex hulls
	None = 0,
	// Remove all overlaps between neighboring convex hulls
	All = 1,
	// Only remove overlaps on convex hulls of clusters, ignoring leaf-leaf overlaps
	OnlyClusters = 2,
	// Only remove overlaps between overlapping clusters, ignoring leaf-leaf and cluster-leaf overlaps
	OnlyClustersVsClusters = 3
};

class CHAOS_API FGeometryCollectionConvexUtility
{
public:

	struct FGeometryCollectionConvexData
	{
		TManagedArray<TSet<int32>>& TransformToConvexIndices;
		TManagedArray<TUniquePtr<Chaos::FConvex>>& ConvexHull;
	};

	/** Ensure that convex hull data exists for the Geometry Collection and construct it if not (or if some data is missing. */
	static FGeometryCollectionConvexData GetValidConvexHullData(FGeometryCollection* GeometryCollection);

	/** Get convex hull data for the Geometry Collection if it is present */
	static TOptional<FGeometryCollectionConvexData> GetConvexHullDataIfPresent(FGeometryCollection* GeometryCollection);

	/** @return true if convex hull data is present */
	static bool HasConvexHullData(FGeometryCollection* GeometryCollection);

	/**
	 Create non-overlapping convex hull data for all transforms in the geometry collection (except transforms where it would be better to just use the hulls of the children) 

	 @param GeometryCollection					The collection to add convex hulls to
	 @param FractionAllowRemove					The fraction of a convex body we can cut away to remove overlaps with neighbors, before we fall back to using the hulls of the children directly.  (Does not affect leaves of hierarchy)
	 @param SimplificationDistanceThreshold		Approximate minimum distance between vertices, below which we remove vertices to generate a simpler convex shape.  If 0.0, no simplification will occur.
	 @param CanExceedFraction					The fraction by which the convex body volume on a cluster can exceed the volume of the geometry under that cluster (a value of 1 == exceed by 100% == convex hull has 2x the volume of the geometry)
	 @param OverlapRemovalMethod				If bRemoveOverlaps, control which overlaps are removed
	 @param OverlapRemovalShrinkPercent			Compute overlaps based on objects shrunk by this percentage, so objects that would not overlap with this value set as their 'Collision Object Reduction Percentage' will not be cut
	 */
	static FGeometryCollectionConvexData CreateNonOverlappingConvexHullData(FGeometryCollection* GeometryCollection, double FractionAllowRemove = .3, double SimplificationDistanceThreshold = 0.0, double CanExceedFraction = .5, 
		EConvexOverlapRemoval OverlapRemovalMethod = EConvexOverlapRemoval::All, double OverlapRemovalShrinkPercent = 0.0);

	/** Returns the convex hull of the vertices contained in the specified geometry. */
	static TUniquePtr<Chaos::FConvex> FindConvexHull(const FGeometryCollection* GeometryCollection, int32 GeometryIndex);

	/** Delete the convex hulls pointed at by the transform indices provided. */
	static void RemoveConvexHulls(FGeometryCollection* GeometryCollection, const TArray<int32>& SortedTransformDeletes);

	/** Set default values for convex hull related managed arrays. */
	static void SetDefaults(FGeometryCollection* GeometryCollection, FName Group, uint32 StartSize, uint32 NumElements);

	/** Get the HasCustomConvex flags.  If they're missing, either add them (if bAddIfMissing) or return nullptr */
	static TManagedArray<int32>* GetCustomConvexFlags(FGeometryCollection* GeometryCollection, bool bAddIfMissing = false);

	/** @return true if the GeometryCollection has convex data with no null pointers and no invalid indices */
	static bool ValidateConvexData(const FGeometryCollection* GeometryCollection);

	/** Set Volume and Size attributes on the Collection (will be called by CreateNonOverlappingConvexHullData -- Volumes must be up to date for convex calc) */
	static void SetVolumeAttributes(FGeometryCollection* Collection);

	/**
	 * Copy convex hulls from *below* FromTransformIdx over to all live at ToTransformIdx.
	 * The two geometry collections can be the same but do not need to be.
	 * Note: This will also set HasCustomConvex flags for the ToTransformIdx.
	 * 
	 * @param FromCollection	The collection to copy from
	 * @param FromTransformIdx	The transform indices whose child nodes should be copied from (or, if it's a leaf, the hull on the leaf will be copied instead)
	 * @param ToCollection		The collection to copy to (can be the same as FromCollection)
	 * @param ToTransformIdx	The transform indices whose convexes will be replaced with *copies* of the child convexes.  Must be same length as FromTransformIdx; can be the same array.
	 * @param bLeafOnly			If true, we will only collect convexes from leaf bones, not from clusters.
	 */
	static void CopyChildConvexes(const FGeometryCollection* FromCollection, const TArrayView<const int32>& FromTransformIdx, FGeometryCollection* ToCollection, const TArrayView<const int32>& ToTransformIdx, bool bLeafOnly);

};


// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoxTypes.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector2D.h"

namespace UE
{
namespace Geometry
{

using namespace UE::Math;

/**
 * Generate a "z-order curve" ordering for a point set by distributing the points to a quad- or octree and then reading the points indices back in tree traversal order
 * This is useful for giving the points spatial locality -- so points that are close together in the ordering tend to be close together spatially, also
 */
struct GEOMETRYCORE_API FZOrderCurvePoints
{
	/// Inputs

	// Maximum depth of quad/octree used to distribute points in space
	int32 MaxTreeDepth = 10;


	/// Outputs

	TArray<int32> Order;

public:

	FZOrderCurvePoints(int32 MaxTreeDepth = 10) : MaxTreeDepth(MaxTreeDepth)
	{
	}

	/**
	 * Compute an ordering for the input points that attempts to order points so adjacent points are close together spatially
	 * Supports both 2D and 3D points.
	 *
	 * @param Points				Points to re-order
	 * @param Bounds				Optional bounding box of the points; if an empty box is passed in, it will be computed for you
	 */
	void Compute(TArrayView<const FVector2d> Points, const FAxisAlignedBox2d& Bounds = FAxisAlignedBox2d::Empty());
	void Compute(TArrayView<const FVector2f> Points, const FAxisAlignedBox2f& Bounds = FAxisAlignedBox2f::Empty());
	void Compute(TArrayView<const FVector3d> Points, const FAxisAlignedBox3d& Bounds = FAxisAlignedBox3d::Empty());
	void Compute(TArrayView<const FVector3f> Points, const FAxisAlignedBox3f& Bounds = FAxisAlignedBox3f::Empty());

};

} // end namespace UE::Geometry
} // end namespace UE

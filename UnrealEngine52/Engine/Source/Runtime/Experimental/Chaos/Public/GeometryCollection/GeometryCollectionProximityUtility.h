// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"

#include "GeometryCollectionProximityUtility.generated.h"

class FGeometryCollection;
namespace UE::GeometryCollectionConvexUtility
{
	struct FConvexHulls;
}

UENUM()
enum class EProximityMethod : int32
{
	// Precise proximity mode looks for geometry with touching vertices or touching, coplanar, opposite-facing triangles. This works well with geometry fractured using our fracture tools.
	Precise,
	// Convex Hull proximity mode looks for geometry with overlapping convex hulls (with an optional offset)
	ConvexHull
};

UENUM()
enum class EProximityContactMethod : uint8
{
	// Rejects proximity if the bounding boxes do not overlap by more than this many centimeters in any major axis direction. This can filter out corner connections of box-like shapes.
	MinOverlapInProjectionToMajorAxes
	//~ TODO: Add other methods for filtering overlaps, e.g. based on approximate surface area of the contact
};

class CHAOS_API FGeometryCollectionProximityUtility
{
public:
	FGeometryCollectionProximityUtility(FGeometryCollection* InCollection);

	void UpdateProximity(UE::GeometryCollectionConvexUtility::FConvexHulls* OptionalComputedHulls = nullptr);

	// Update proximity data if it is not already present
	void RequireProximity(UE::GeometryCollectionConvexUtility::FConvexHulls* OptionalComputedHulls = nullptr);

	void InvalidateProximity();

	void CopyProximityToConnectionGraph();
	void ClearConnectionGraph();

private:
	FGeometryCollection* Collection;
};


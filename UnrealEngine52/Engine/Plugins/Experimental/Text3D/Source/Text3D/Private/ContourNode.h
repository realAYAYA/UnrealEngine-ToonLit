// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Polygon2.h"

using UE::Geometry::FPolygon2f;

struct FContourNode;
using TSharedContourNode = TSharedPtr<FContourNode>;

struct FContourNode final
{
	FContourNode(const TSharedPtr<FPolygon2f> ContourIn, const bool bCanHaveIntersectionsIn, const bool bClockwiseIn) :
		Contour(ContourIn),
		bCanHaveIntersections(bCanHaveIntersectionsIn),
		bClockwise(bClockwiseIn)
	{
	}


	const TSharedPtr<FPolygon2f> Contour;
	// Needed for dividing contours with self-intersections: default value is true, false for parts of divided contours
	const bool bCanHaveIntersections;
	bool bClockwise;

	// Contours that are inside this contour
	TArray<TSharedContourNode> Children;
};

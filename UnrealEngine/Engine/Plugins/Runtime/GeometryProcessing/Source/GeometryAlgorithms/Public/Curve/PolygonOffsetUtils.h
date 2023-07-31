// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curve/GeneralPolygon2.h"

namespace UE::Geometry
{
	enum class EPolygonOffsetJoinType : uint8
	{
		Square,		/* Uniform squaring on all convex edge joins. */
		Round,		/* Arcs on all convex edge joins. */
		Miter,		/* Squaring of convex edge joins with acute angles ("spikes"). Use in combination with MiterLimit. */
	};

	enum class EPolygonOffsetEndType : uint8
	{
		Polygon,	/* Offsets only one side of a closed path */
		Joined,		/* Offsets both sides of a path, with joined ends */
		Butt,		/* Offsets both sides of a path, with square blunt ends */
		Square,		/* Offsets both sides of a path, with square extended ends */
		Round,		/* Offsets both sides of a path, with round extended ends */
	};
	
	/** Offsets a given polygon (a polyline or closed polygon), determined by EndType by a  given amount.  */
	template <typename GeometryType, typename RealType>
	class TOffsetPolygon2
	{
	public:
		// Input
		TArray<GeometryType> Polygons;
		RealType Offset = 1.0;
		RealType MiterLimit = 2.0; // Minimum value is clamped to 2.0
		EPolygonOffsetJoinType JoinType = EPolygonOffsetJoinType::Square;
		EPolygonOffsetEndType EndType = EPolygonOffsetEndType::Polygon;

		// Output
		TArray<UE::Geometry::TGeneralPolygon2<RealType>> Result;

		explicit TOffsetPolygon2(const TArray<GeometryType>& InPolygons);

		bool ComputeResult();
	};

	typedef TOffsetPolygon2<TArrayView<TVector2<float>>, float> FOffsetPolygon2f;
	typedef TOffsetPolygon2<TArrayView<TVector2<double>>, double> FOffsetPolygon2d;
}

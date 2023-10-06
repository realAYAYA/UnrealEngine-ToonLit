// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "IndexTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "MeshShapeGenerator.h"
#include "Polygon2.h"

namespace UE
{
namespace Geometry
{


/**
 * Generate planar triangulation of a Polygon.
 */
class FPlanarPolygonMeshGenerator : public FMeshShapeGenerator
{
public:
	/** Polygon to triangulate. If Polygon has self-intersections or degenerate edges, result is undefined. */
	FPolygon2d Polygon;

	/** Normal vector of all vertices will be set to this value. Default is +Z axis. */
	FVector3f Normal;

	/** How to map 2D indices to 3D. Default is (0,1) = (x,y,0). */
	FIndex2i IndicesMap;

public:
	GEOMETRYCORE_API FPlanarPolygonMeshGenerator();

	/** Initialize the polygon from an array of 2D vertices */
	GEOMETRYCORE_API void SetPolygon(const TArray<FVector2D>& PolygonVerts);

	/** Generate the triangulation */
	GEOMETRYCORE_API virtual FMeshShapeGenerator& Generate() override;


	/** Create vertex at position under IndicesMap, shifted to Origin*/
	inline FVector3d MakeVertex(double x, double y)
	{
		FVector3d v(0, 0, 0);
		v[IndicesMap.A] = x;
		v[IndicesMap.B] = y;
		return v;
	}
};


} // end namespace UE::Geometry
} // end namespace UE

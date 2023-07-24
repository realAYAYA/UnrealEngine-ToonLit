// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MathUtil.h"
#include "VectorTypes.h"
#include "GeometryTypes.h"
#include "Polygon2.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace UE
{
namespace Geometry
{

/**
 * Calculate a Convex Hull for a Mesh by first Projecting all vertices to a plane, computing a
 * 2D convex polygon that contains them, and then sweeping that 2D hull to create an extruded 3D volume.
 */
class DYNAMICMESH_API FMeshProjectionHull
{
public:
	/** Input Mesh */
	const FDynamicMesh3* Mesh;

	/** Input 3D Frame/Plane */
	FFrame3d ProjectionFrame;

	/** If true, 2D convex hull is simplified using MinEdgeLength and DeviationTolerance */
	bool bSimplifyPolygon = false;
	/** Minimum Edge Length of the simplified 2D Convex Hull */
	double MinEdgeLength = 0.01;
	/** Deviation Tolerance of the simplified 2D Convex Hull */
	double DeviationTolerance = 0.1;

	/** Minimum thickness of extrusion. If extrusion length is smaller than this amount, box is expanded symmetrically */
	double MinThickness = 0.0;


	/** Calculated convex hull polygon */
	FPolygon2d ConvexHull2D;

	/** Simplified convex hull polygon. Not initialized if bSimplifyPolygon == false */
	FPolygon2d SimplifiedHull2D;

	/** Output swept-polygon convex hull */
	FDynamicMesh3 ConvexHull3D;

public:
	FMeshProjectionHull(const FDynamicMesh3* MeshIn)
	{
		Mesh = MeshIn;
	}

	/**
	 * Calculate output 2D Convex Polygon and Swept-Polygon 3D Mesh for vertices of input Mesh
	 * @return true on success
	 */
	bool Compute();

};

} // end namespace UE::Geometry
} // end namespace UE
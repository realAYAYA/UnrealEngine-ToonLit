// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MathUtil.h"
#include "VectorTypes.h"
#include "IntVectorTypes.h"
#include "GeometryTypes.h"
#include "DynamicMesh/DynamicMesh3.h"

class FProgressCancel;

namespace UE
{
namespace Geometry
{

/**
 * Calculate Convex Hull of a Mesh
 */
class DYNAMICMESH_API FMeshConvexHull
{
public:
	/** Input Mesh */
	const FDynamicMesh3* Mesh;

	/** If set, hull will be computed on subset of vertices */
	TArray<int32> VertexSet;

	/** If true, output convex hull is simplified down to MaxTargetFaceCount */
	bool bPostSimplify = false;
	/** Target triangle count of the output Convex Hull */
	int32 MaxTargetFaceCount = 0;

	/** Output convex hull */
	FDynamicMesh3 ConvexHull;

	/** 
	 *  Choose a more or less evenly-spaced subset of mesh vertices. Conceptually, this function creates a uniform 
	 *  grid with given cell size. (Cell size is given roughly as a percentage of the total mesh bounding box.) Each 
	 *  grid cell can hold up to one vertex. Return the set of representative vertices, maximum one per cell.
	 * 
	 *  @param Mesh							Surface mesh whose vertices we want to sample
	 *  @param CellSizeAsPercentOfBounds	Grid cell size expressed as a percentage of the mesh bounding box size
	 *  @param OutSamples					Indices of chosen vertices
	 */
	static void GridSample(const FDynamicMesh3& Mesh, 
						   int GridResolutionMaxAxis,
						   TArray<int32>& OutSamples);

	/** Used for testing/debugging */
	static FVector3i DebugGetCellIndex(const FDynamicMesh3& Mesh,
									   int GridResolutionMaxAxis,
									   int VertexIndex);


public:
	FMeshConvexHull(const FDynamicMesh3* MeshIn)
	{
		Mesh = MeshIn;
	}

	/**
	 * Calculate output ConvexHull mesh for vertices of input Mesh
	 * @return true on success
	 */
	bool Compute(FProgressCancel* Progress = nullptr);

	/**
	 * Simplify the output convex hull. Assumes ConvexHull is already computed.
	 * @return true on success
	 */
	static bool SimplifyHull(FDynamicMesh3& HullMesh, int32 MaxTargetFaceCount, FProgressCancel* Progress = nullptr);

protected:
	bool Compute_FullMesh(FProgressCancel* Progress);
	bool Compute_VertexSubset(FProgressCancel* Progress);
};


} // end namespace UE::Geometry
} // end namespace UE
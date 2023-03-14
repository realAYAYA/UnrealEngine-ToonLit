// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MathUtil.h"
#include "VectorTypes.h"
#include "GeometryTypes.h"
#include "MeshRegionBoundaryLoops.h"
#include "DynamicMesh/DynamicMeshChangeTracker.h"


namespace UE
{
namespace Geometry
{

class FDynamicMesh3;
class FMeshNormals;
class FDynamicMeshChangeTracker;

/**
 * FOffsetMeshRegion implements local extrusion/offset of a mesh region. 
 * The selected triangles are separated and then stitched back together, creating
 * an new strip of triangles around their border (s). The offset region is
 * then transformed using the OffsetPositionFunc.
 *
 * Complex input regions are handled, eg it can be multiple disconnected components, donut-shaped, etc
 * 
 * Each quad of the border loop is assigned it's own normal and UVs (ie each is a separate UV-island)
 */
class DYNAMICMESH_API FOffsetMeshRegion
{
public:


	enum class EVertexExtrusionVectorType
	{
		Zero,
		VertexNormal,
		// Angle weighted average of the triangles in the selection that contain this vertex (unit length)
		SelectionTriNormalsAngleWeightedAverage,
		// Like SelectionTriNormalsAngleWeightedAverage, but with the vertex length adjusted to try to keep
		// the selection triangles parallel to their original location.
		SelectionTriNormalsAngleWeightedAdjusted,
	};

	//
	// Inputs
	//

	/** The mesh that we are modifying */
	FDynamicMesh3* Mesh;

	/** The triangle region we are modifying */
	TArray<int32> Triangles;

	/** This function is called to generate the offset vertex position. */
	TFunction<FVector3d(const FVector3d& Position, const FVector3d& VertexVector, int Vid)> OffsetPositionFunc = 
		[this](const FVector3d& Position, const FVector3d& VertexVector, int Vid)
	{
		return Position + this->DefaultOffsetDistance * (FVector3d)VertexVector;
	};

	/** Used in the default OffsetPositionFunc. */
	double DefaultOffsetDistance = 1.0;

	/** Determines the type of vector passed in per-vertex to OffsetPositionFunc */
	EVertexExtrusionVectorType ExtrusionVectorType = EVertexExtrusionVectorType::Zero;

	/** 
	 * When stitching the extruded portion to the source loop, this determines the grouping of the side quads by 
	 * passing in successive Eids in the original loop. For instance, always returning true will result in one 
	 * group for the stitched portion, and always returning true will result in a separate group for every edge
	 * quad. By default, the same group is given to adjacent edges that separate the same two groups, or to border
	 * edges that are colinear.
	 */
	TFunction<bool(int32 Eid1, int32 Eid2)> LoopEdgesShouldHaveSameGroup =
		[this](int32 Eid1, int32 Eid2) { return EdgesSeparateSameGroupsAndAreColinearAtBorder(Mesh, Eid1, Eid2, true); };

	/** quads on the stitch loop are planar-projected and scaled by this amount */
	float UVScaleFactor = 1.0f;

	/** If a sub-region of Triangles is a full connected component, offset into a solid instead of leaving a shell*/
	bool bOffsetFullComponentsAsSolids = true;

	/** 
	 * When bOffsetFullComponentsAsSolids is true and the extrusion applied by OffsetPositionFunc is negative,
	 * then fully-connected components need to be turned inside-out. Thus, in this circumstance we need to
	 * be notified whether the OffsetPositionFunc is applying a positive or negative offset.
	 */
	bool bIsPositiveOffset = true;

	/** When using SelectionTriNormalsAngleWeightedAdjusted to keep triangles parallel, clamp the offset scale to at most this factor */
	double MaxScaleForAdjustingTriNormalsOffset = 4.0;

	//
	// Outputs
	//

	/**
	 * Offset information for a single connected component
	 */
	struct FOffsetInfo
	{
		/**
		 * Set of triangles for this region. These will be the same Tids that were
		 * originally in the region. 
		 */
		TArray<int32> OffsetTids;
		/** 
		 * Groups on offset faces. These IDs will be the same as the original groups 
		 * in the region. 
		 */
		TArray<int32> OffsetGroups;

		/** Initial loops on the mesh */
		TArray<FEdgeLoop> BaseLoops;
		/** 
		 * Offset loops on the mesh. If the region had bowtie vertices (in terms of selection, not
		 * even necessarily in terms of original mesh topology), then the number of loops here may
		 * no longer match number of loops in BaseLoops, StitchTriangles, etc (for example, imagine
		 * a circular hole tangent inside a circular region; when the tangent bowtie is split, this
		 * becomes one C-shaped loop instead of two nested ones)
		 */
		TArray<FEdgeLoop> OffsetLoops;

		/** Lists of triangle-strip "tubes" that connect each loop-pair */
		TArray<TArray<int>> StitchTriangles;
		/** List of group ids / polygon ids on each triangle-strip "tube" */
		TArray<TArray<int>> StitchPolygonIDs;

		/** If true, full region was thickened into a solid */
		bool bIsSolid = false;
	};

	/**
	 * List of offset regions/components
	 */
	TArray<FOffsetInfo> OffsetRegions;

	/**
	 * List of all triangles created/modified by this operation
	 */
	TArray<int32> AllModifiedAndNewTriangles;

public:
	FOffsetMeshRegion(FDynamicMesh3* mesh);

	virtual ~FOffsetMeshRegion() {}


	/**
	 * @return EOperationValidationResult::Ok if we can apply operation, or error code if we cannot
	 */
	virtual EOperationValidationResult Validate()
	{
		// @todo calculate MeshBoundaryLoops and make sure it is valid

		// is there any reason we couldn't do this??

		return EOperationValidationResult::Ok;
	}


	/**
	 * Apply the Offset operation to the input mesh.
	 * @return true if the algorithm succeeds
	 */
	virtual bool Apply();

	// The default function used for LoopEdgesShouldHaveSameGroup. 
	static bool EdgesSeparateSameGroupsAndAreColinearAtBorder(FDynamicMesh3* Mesh,
		int32 Eid1, int32 Eid2, bool bCheckColinearityAtBorder);
protected:

	virtual bool ApplyOffset(FOffsetInfo& Region);
};

} // end namespace UE::Geometry
} // end namespace UE

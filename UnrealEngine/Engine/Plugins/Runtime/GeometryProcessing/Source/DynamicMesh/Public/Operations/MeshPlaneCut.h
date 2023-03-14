// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3Sharp MeshPlaneCut

#pragma once

#include "MathUtil.h"
#include "VectorTypes.h"
#include "GeometryTypes.h"
#include "MeshBoundaryLoops.h"
#include "Curve/GeneralPolygon2.h"



namespace UE
{
namespace Geometry
{

class FDynamicMesh3;
template<typename RealType> class TDynamicMeshScalarTriangleAttribute;

/**
 * Cut the Mesh with the Plane. The *positive* side, ie (p-o).n > 0, is removed.
 * If possible, returns boundary loop(s) along cut
 * (this will fail if cut intersected with holes in mesh).
 * Also FillHoles() for a topological fill. Or use CutLoops and fill yourself.
 * 
 * Algorithm is:
 *    1) find all edge crossings
 *	  2) optionally discard any triangles with all vertex distances < epsilon.
 *    3) Do edge splits at crossings
 *    4 option a) (optionally) delete all vertices on positive side
 *	  4 option b) (OR optionally) disconnect all triangles w/ vertices on positive side (if keeping both sides)
 *	  4 option c) do nothing (if keeping both sides and not disconnecting them)
 *    5) (optionally) collapse any degenerate boundary edges 
 *	  6) (optionally) change an attribute tag for all triangles on positive side
 *    7) find loops through valid boundary edges (ie connected to splits, or on-plane edges) (if second half was kept, do this separately for each separate mesh ID label)
 */
class DYNAMICMESH_API FMeshPlaneCut
{
public:

	//
	// Inputs
	//
	FDynamicMesh3 *Mesh;
	FVector3d PlaneOrigin, PlaneNormal;
	
	/**
	 * If set, only edges that pass this filter will be split
	 */
	TUniqueFunction<bool(int32)> EdgeFilterFunc = nullptr;

	bool bCollapseDegenerateEdgesOnCut = true;
	double DegenerateEdgeTol = FMathd::ZeroTolerance;

	/** UVs on any hole fill surfaces are scaled by this amount */
	float UVScaleFactor = 1.0f;

	/** Tolerance distance for considering a vertex to be 'on plane' */
	double PlaneTolerance = FMathf::ZeroTolerance * 10.0;


	// TODO support optionally restricting plane cut to a mesh selection
	//MeshFaceSelection CutFaceSet;

	//
	// Outputs
	//
	struct FOpenBoundary
	{
		int Label; // optional ID, used to transfer label to new hole-fill triangles
		float NormalSign = 1; // -1 for the open boundary on the other side of the cut (for the CutWithoutDelete path)
		TArray<FEdgeLoop> CutLoops;
		TArray<FEdgeSpan> CutSpans;
		bool CutLoopsFailed = false;		// set to true if we could not compute cut loops/spans
		bool FoundOpenSpans = false;     // set to true if we found open spans in cut
	};
	// note: loops and spans within a single FOpenBoundary could be part of the same hole-fill triangulation
	//	separate open boundary structs will be considered separately and will not share hole fill triangles
	TArray<FOpenBoundary> OpenBoundaries;

	// Triangle IDs of hole fill triangles.  Outer array is 1:1 with the OpenBoundaries array
	TArray<TArray<int>> HoleFillTriangles;

	struct FCutResultRegion
	{
		int32 GroupID;
		TArray<int32> Triangles;
	};
	/** List of output cut regions (eg that have separate GroupIDs). Currently only calculated by SplitEdgesOnly() path */
	TArray<FCutResultRegion> ResultRegions;
	/** List of output cut triangles representing the seed triangles along the cut. Currently only calculated by SplitEdgesOnly() path */
	TArray<int32> ResultSeedTriangles;

public:

	/**
	 *  Cut mesh with plane. Assumption is that plane normal is Z value.
	 */
	FMeshPlaneCut(FDynamicMesh3* Mesh, FVector3d Origin, FVector3d Normal) : Mesh(Mesh), PlaneOrigin(Origin), PlaneNormal(Normal)
	{
	}
	virtual ~FMeshPlaneCut() {}
	
	/**
	 * @return EOperationValidationResult::Ok if we can apply operation, or error code if we cannot
	 */
	virtual EOperationValidationResult Validate()
	{
		// @todo validate inputs
		return EOperationValidationResult::Ok;
	}

	/**
	 * Compute the plane cut by splitting mesh edges that cross the cut plane, and then deleting any triangles
	 * on the positive side of the cutting plane.
	 * @return true if operation succeeds
	 */
	virtual bool Cut();

	/**
	 * Compute the plane cut by splitting mesh edges that cross the cut plane, but not deleting triangles on positive side.
	 * @param bSplitVerticesAtPlane if true, vertices on cutting plane are split into multiple vertices
	 * @param OffsetSeparatedPortion positive side of cut mesh is offset by this distance along plane normal
	 * @param TriLabels optional per-triangle integer labels
	 * @param NewLabelStartID starting new label value
	 * @param bAddBoundariesFirstHalf add open boundaries on "first" half to OpenBoundaries list (default true)
	 * @param bAddBoundariesSecondHalf add open boundaries on "second" half to OpenBoundaries list (default true)
	 * @return true if operation succeeds
	 */
	virtual bool CutWithoutDelete(bool bSplitVerticesAtPlane, float OffsetSeparatedPortion = 0.0f,
		TDynamicMeshScalarTriangleAttribute<int>* TriLabels = nullptr, int NewLabelStartID = 0,
		bool bAddBoundariesFirstHalf = true, bool bAddBoundariesSecondHalf = true);

	/**
	 * Compute the plane cut by splitting mesh edges that cross the cut plane, and then optionally update groups
	 * @param bAssignNewGroups if true, update group IDs such that each group-connected-component on either side of the cut plane is assigned a new unique group ID
	 * @return true if operation succeeds
	 */
	virtual bool SplitEdgesOnly(bool bAssignNewGroups);

	/**
	 *  Fill cut loops with FSimpleHoleFiller
	 */
	virtual bool SimpleHoleFill(int ConstantGroupID = -1);

	virtual bool MinimalHoleFill(int ConstantGroupID = -1);

	/**
	 *  Fill cut loops with FPlanarHoleFiller, using a caller-provided triangulation function
	 */
	virtual bool HoleFill(TFunction<TArray<FIndex3i>(const FGeneralPolygon2d&)> PlanarTriangulationFunc, bool bFillSpans, int ConstantGroupID = -1);

	
	virtual void TransferTriangleLabelsToHoleFillTriangles(TDynamicMeshScalarTriangleAttribute<int>* TriLabels);

protected:

	void CollapseDegenerateEdges(const TSet<int>& OnCutEdges, const TSet<int>& ZeroEdges);
	void SplitCrossingEdges(TArray<double>& Signs, TSet<int>& ZeroEdges, TSet<int>& OnCutEdges, bool bDeleteTrisOnPlane = true);
	void SplitCrossingEdges(TArray<double>& Signs, TSet<int>& ZeroEdges, TSet<int>& OnCutEdges, TSet<int>& OnSplitEdges, bool bDeleteTrisOnPlane = true);
	bool ExtractBoundaryLoops(const TSet<int>& OnCutEdges, const TSet<int>& ZeroEdges, FMeshPlaneCut::FOpenBoundary& Boundary);

	// set of vertices lying on plane after calling SplitCrossingEdges
	TSet<int32> OnCutVertices;
};


} // end namespace UE::Geometry
} // end namespace UE
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "FrameTypes.h"
#include "HAL/Platform.h" //int32
#include "Misc/Optional.h"
#include "VectorTypes.h"

class FProgressCancel;

namespace UE::Geometry
{

class FDynamicMesh3;

class FExtrudeBoundaryEdges
{
public:
	using FFrame3d = UE::Geometry::FFrame3d;
	
	// Represents a frame where the axes might not be unit scaled (but are still orthogonal).
	// Allows vertices to adjust the extrusion distance along one of their extrusion frame
	//  axes when trying to keep edges parallel.
	// When using extrusion frames, vertices will be moved in the XZ plane, usually along X.
	struct FExtrudeFrame
	{
		FExtrudeFrame() {}

		FExtrudeFrame(const FFrame3d& FrameIn, const FVector3d& ScalingIn = FVector3d::One())
			: Frame(FrameIn)
			, Scaling(ScalingIn)
		{}

		FFrame3d Frame;
		FVector3d Scaling = FVector3d::One();
	};

	// Data needed to create a new vert and its extrude frame
	struct FNewVertSourceData
	{
		int32 SourceVid = IndexConstants::InvalidID;
		// Neighboring edges, ordered by (incoming, outgoing)
		FIndex2i SourceEidPair = FIndex2i::Invalid();
	};

	// Inputs:

	/** The mesh that we are modifying */
	FDynamicMesh3* Mesh;

	/** The edges we're extruding */
	TArray<int32> InputEids;

	/** Whether to calculate local extrude frames and supply them to OffsetPositionFunc */
	bool bUsePerVertexExtrudeFrames = true;

	/**
	 * Function queried for new vertex positions. ExtrudeFrame origin is Position, unless it is not initialized
	 *  due to bUsePerVertexExtrudeFrames being false.
	 * The default given here assumes that bUsePerVertexExtrudeFrames is true, and extrudes along the X axis of the extrude frame.
	 */
	TFunction<FVector3d(const FVector3d& Position, const FExtrudeFrame& ExtrudeFrame, int32 SourceVid)> OffsetPositionFunc =
		[this](const FVector3d& Position, const FExtrudeFrame& ExtrudeFrame, int32 SourceVid)
	{
		return ExtrudeFrame.Frame.FromFramePoint(FVector3d(this->DefaultOffsetDistance, 0, 0) * ExtrudeFrame.Scaling);
	};

	/** When generating extrude frames, whether to use unselected neighbors for setting the frame. */
	bool bAssignAnyBoundaryNeighborToUnmatched = false;

	/** 
	 * If greater than 1, maximal amount by which a vertex can be moved in an attempt to keep edges parallel to
	 *  original edges while extruding. This "movement" is done by scaling the X axis of the extrude frame.
	 */
	double ScalingAdjustmentLimit = 1.0;

	/** 
	 * Optional mapping, 1:1 with Eids, that gives the group id to use for each generated quad. Otherwise all generated
	 *  triangles will be given the same new group id.
	 */
	TOptional<TArray<int32>> GroupsToSetPerEid;

	/** Used in the default OffsetPositionFunc. */
	double DefaultOffsetDistance = 1.0;


	// Outputs:
	TArray<int32> NewTids;
	TArray<int32> NewExtrudedEids;

public:
	FExtrudeBoundaryEdges(FDynamicMesh3* mesh);

	virtual ~FExtrudeBoundaryEdges() {}

	/**
	 * Apply the operation to the input mesh.
	 * @return true if the algorithm succeeds
	 */
	virtual bool Apply(FProgressCancel* Progress);

	/** 
	 * Pairs up edges across vertices to help in extrusion frame calculation. Public because it is used by
	 * the extrude edges activity to find an operational space for the gizmos used to set extrude distance.
	 * 
	 * @return false if there is an error.
	 */
	static bool GetInputEdgePairings(
		const FDynamicMesh3& Mesh, TArray<int32>& InputEids, bool bAssignAnyBoundaryNeighborToUnmatched,
		TArray<FNewVertSourceData>& NewVertDataOut, TMap<int32, FIndex2i>& EidToIndicesIntoNewVertsOut);

	/**
	 * Gets an extrude frame given a vert and its neighboring boundary edges. Like GetInputEdgePairings,
	 * public because it is used to set up UX for setting extrude distance.
	 * 
	 @return false if there is an error.
	 */
	static bool GetExtrudeFrame(const FDynamicMesh3& Mesh, int32 Vid,
		int32 IncomingEid, int32 OutgoingEid, FExtrudeFrame& ExtrudeFrameOut,
		double ScalingLimit);
};

} // end namespace UE::Geometry
// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DynamicMesh/DynamicMesh3.h"
#include "Util/ProgressCancel.h"

namespace UE
{
namespace Geometry
{

class DYNAMICMESH_API FMeshMirror
{

public:
	FMeshMirror(FDynamicMesh3* Mesh, FVector3d Origin, FVector3d Normal) 
		: Mesh(Mesh)
		, PlaneOrigin(Origin)
		, PlaneNormal(Normal)
	{
	}
	virtual ~FMeshMirror() {}

	FDynamicMesh3* Mesh;
	FVector3d PlaneOrigin;
	FVector3d PlaneNormal;

	/** Tolerance distance for considering a vertex to be "on the plane". */
	double PlaneTolerance = FMathf::ZeroTolerance * 10.0;

	/** Whether, when using MirrorAndAppend, vertices on the mirror plane should be welded. */
	bool bWeldAlongPlane = true;

	/**
	 * Whether, when welding, the creation of new bowtie vertices should be allowed (if a point lies 
	 * in the mirror plane without an edge in the plane).
	 */
	bool bAllowBowtieVertexCreation = false;

	/**
	 * Alters the existing mesh to be mirrored across the mirror plane.
	 *
	 * @param Progress Object used to cancel the operation early. This leaves the mesh in an undefined state.
	 */
	void Mirror(FProgressCancel *Progress = nullptr);

	/**
	 * Appends a mirrored copy of the mesh to the mesh.
	 *
	 * @param Progress Object used to cancel the operation early. This leaves the mesh in an undefined state.
	 */
	void MirrorAndAppend(FProgressCancel* Progress = nullptr);
};

} // end namespace UE::Geometry
} // end namespace UE
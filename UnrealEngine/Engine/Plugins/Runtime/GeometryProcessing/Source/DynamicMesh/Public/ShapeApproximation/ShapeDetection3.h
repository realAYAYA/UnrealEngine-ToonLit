// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VectorTypes.h"
#include "FrameTypes.h"
#include "SphereTypes.h"
#include "OrientedBoxTypes.h"
#include "CapsuleTypes.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace UE
{
	namespace Geometry
	{
		/**
		 * Detect if input Mesh is a meshed approximation of an analytic Sphere, and if so return best guess in SphereOut.
		 * Fits a sphere to input points with several rounds of incremental improvement, then measures chordal deviation of edge midpoints.
		 * @param RelativeDeviationTol distances from edge midpoints to sphere surface are allowed to deviate by 2*Radius*RelativeDeviationTol
		 * @return true if mesh is a Sphere and SphereOut is initialized
		 */
		bool DYNAMICMESH_API IsSphereMesh(const FDynamicMesh3& Mesh, FSphere3d& SphereOut, double RelativeDeviationTol = 0.025);


		/**
		 * Detect if input Mesh is a meshed box, and if so return analytic box in BoxOut.
		 * Clusters face normals, looking to find 6 unique normals grouped into 3 opposite-direction pairs.
		 * If this configuraiton is found, computing minimal box is trivial.
		 * @param AngleToleranceDeg normals are allowed to deviate by this amount and still be considered coplanar
		 * @return true if mesh is a Box and BoxOut is initialized
		 */
		bool DYNAMICMESH_API IsBoxMesh(const FDynamicMesh3& Mesh, FOrientedBox3d& BoxOut, double AngleToleranceDeg = 0.1);


		/**
		 * Detect if input Mesh is a meshed approximation of an analytic Capsule, and if so return best guess in CapsuleOut.
		 * Fits a capsule to input points, then measures chordal deviation of edge midpoints.
		 * @param RelativeDeviationTol distances from edge midpoints to capsule surface are allowed to deviate by 2*Radius*RelativeDeviationTol
		 * @return true if mesh is a Capsule and CapsuleOut is initialized
		 */
		bool DYNAMICMESH_API IsCapsuleMesh(const FDynamicMesh3& Mesh, FCapsule3d& CapsuleOut, double RelativeDeviationTol = 0.025);

	}
}
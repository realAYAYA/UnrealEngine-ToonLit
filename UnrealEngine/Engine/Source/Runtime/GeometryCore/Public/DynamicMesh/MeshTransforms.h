// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3cpp MeshWeights

#pragma once

#include "DynamicMesh/DynamicMesh3.h"
#include "FrameTypes.h"
#include "GeometryBase.h"
#include "Math/MathFwd.h"
#include "TransformTypes.h"

namespace UE { namespace Geometry { class FDynamicMesh3; } }
template <typename FuncType> class TFunctionRef;


/**
 * Utility functions for applying transformations to meshes
 */
namespace MeshTransforms
{
	using namespace UE::Geometry;

	/**
	 * Apply Translation to vertex positions of Mesh. Does not modify any other attributes.
	 */
	GEOMETRYCORE_API void Translate(FDynamicMesh3& Mesh, const FVector3d& Translation);

	/**
	 * Apply Scale to vertex positions of Mesh, relative to given Origin. Does not modify any other attributes (normals/etc)
	 * @param bReverseOrientationIfNeeded		If negative scaling inverts the mesh, also invert the triangle orientations
	 */
	GEOMETRYCORE_API void Scale(FDynamicMesh3& Mesh, const FVector3d& Scale, const FVector3d& Origin, bool bReverseOrientationIfNeeded = false);

	/**
	 * Transform Mesh into local coordinates of Frame
	 */
	GEOMETRYCORE_API void WorldToFrameCoords(FDynamicMesh3& Mesh, const FFrame3d& Frame);

	/**
	 * Transform Mesh out of local coordinates of Frame
	 */
	GEOMETRYCORE_API void FrameCoordsToWorld(FDynamicMesh3& Mesh, const FFrame3d& Frame);


	/**
	 * Apply given Transform to a Mesh.
	 * Modifies Vertex Positions and Normals, and any Per-Triangle Normal Overlays
	 * @param bReverseOrientationIfNeeded		If the Transform inverts the mesh with negative scale, also invert the triangle orientations
	 */
	GEOMETRYCORE_API void ApplyTransform(FDynamicMesh3& Mesh, const FTransformSRT3d& Transform, bool bReverseOrientationIfNeeded = false);


	/**
	 * Apply inverse of given Transform to a Mesh.
	 * Modifies Vertex Positions and Normals, and any Per-Triangle Normal Overlays
	  @param bReverseOrientationIfNeeded		If the Transform inverts the mesh with negative scale, also invert the triangle orientations
	 */
	GEOMETRYCORE_API void ApplyTransformInverse(FDynamicMesh3& Mesh, const FTransformSRT3d& Transform, bool bReverseOrientationIfNeeded = false);


	/**
	 * Apply given Transform to a Mesh.
	 * Modifies Vertex Positions and Normals, and any Per-Triangle Normal Overlays
	 */
	GEOMETRYCORE_API void ApplyTransform(FDynamicMesh3& Mesh, 
		TFunctionRef<FVector3d(const FVector3d&)> PositionTransform,
		TFunctionRef<FVector3f(const FVector3f&)> NormalTransform);

	/**
	 * If applying Transform would invert Mesh w/ a negative scale, then invert Mesh's triangle orientations.
	 * Note: Does not apply the transform.
	 */
	GEOMETRYCORE_API void ReverseOrientationIfNeeded(FDynamicMesh3& Mesh, const FTransformSRT3d& Transform);

};
// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3sharp FaceGroupUtil

#pragma once

#include "MathUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMeshEditor.h"

/**
 * Utility functions for dealing with Mesh FaceGroups
 */
namespace FaceGroupUtil
{
	using namespace UE::Geometry;

	/**
	 * Set group ID of all triangles in Mesh
	 */
	DYNAMICMESH_API void SetGroupID(FDynamicMesh3& Mesh, int32 to);

	/**
	 * Set group id of subset of triangles in Mesh
	 */
	DYNAMICMESH_API void SetGroupID(FDynamicMesh3& Mesh, const TArrayView<const int32>& triangles, int32 to);

	/**
	 * replace group id in Mesh
	 */
	DYNAMICMESH_API void SetGroupToGroup(FDynamicMesh3& Mesh, int32 from, int32 to);

	/**
	 * @return true if mesh has multiple face groups
	 */
	DYNAMICMESH_API bool HasMultipleGroups(const FDynamicMesh3& Mesh);

	/**
	 * find the set of group ids used in Mesh
	 */
	DYNAMICMESH_API void FindAllGroups(const FDynamicMesh3& Mesh, TSet<int32>& GroupsOut);

	/**
	 * count number of tris in each group in Mesh; TODO: does this need sparse storage?
	 */
	DYNAMICMESH_API void CountAllGroups(const FDynamicMesh3& Mesh, TArray<int32>& GroupCountsOut);

	/**
	 * collect triangles by group id. Returns array of triangle lists (stored as arrays).
	 * This requires 2 passes over Mesh, but each pass is linear
	 */
	DYNAMICMESH_API void FindTriangleSetsByGroup(const FDynamicMesh3& Mesh, TArray<TArray<int32>>& GroupTrisOut, int32 IgnoreGID = -1);

	/**
	 * find list of triangles in Mesh with specific group id
	 */
	DYNAMICMESH_API bool FindTrianglesByGroup(FDynamicMesh3& Mesh, int32 FindGroupID, TArray<int32>& TrianglesOut);

	/**
	* split input Mesh into submeshes based on group ID
	* **does not** separate disconnected components w/ same group ID
	*/
	DYNAMICMESH_API void SeparateMeshByGroups(FDynamicMesh3& Mesh, TArray<FDynamicMesh3>& SplitMeshes);

	/**
	* split input Mesh into submeshes based on group ID
	* **does not** separate disconnected components w/ same group ID
	*/
	DYNAMICMESH_API void SeparateMeshByGroups(FDynamicMesh3& Mesh, TArray<FDynamicMesh3>& SplitMeshes, TArray<int32>& GroupIDs);


}

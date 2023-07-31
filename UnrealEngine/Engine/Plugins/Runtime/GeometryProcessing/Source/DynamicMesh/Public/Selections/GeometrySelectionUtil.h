// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Selections/GeometrySelection.h"
#include "TriangleTypes.h"
#include "SegmentTypes.h"
#include "FrameTypes.h"
#include "Polygroups/PolygroupSet.h"

PREDECLARE_GEOMETRY(class FDynamicMesh3);

namespace UE
{
namespace Geometry
{

class FColliderMesh;
class FGroupTopology;
struct FGroupTopologySelection;

/**
 * Assuming that the uint64 values in the GeometrySelection are encoded FGeoSelectionID's, 
 * find the item that has a matching TopologyID, ignoring the GeometryID.
 * @param FoundValue the hash value found 
 * @return true if an item was found
 */
DYNAMICMESH_API bool FindInSelectionByTopologyID(
	const FGeometrySelection& GeometrySelection,
	uint32 TopologyID,
	uint64& FoundValue);


/**
 * Update a FGeometrySelection (via the Editor) of mesh vertices/edges/triangles
 * based on a raycast
 * @param UpdateConfig type of update (add/remove/replace) and other update controls
 * @param ResultOut selection delta and information on hit/miss are returned here
 */
DYNAMICMESH_API void UpdateTriangleSelectionViaRaycast(
	const FColliderMesh* ColliderMesh,
	FGeometrySelectionEditor* Editor,
	const FRay3d& LocalRay,
	const FGeometrySelectionUpdateConfig& UpdateConfig,
	FGeometrySelectionUpdateResult& ResultOut);

/**
 * Update a FGeometrySelection (via the Editor) of mesh polygroup faces/edges/corners,
 * based on a raycast
 * @param UpdateConfig type of update (add/remove/replace) and other update controls
 * @param ResultOut selection delta and information on hit/miss are returned here
 */
DYNAMICMESH_API void UpdateGroupSelectionViaRaycast(
	const FColliderMesh* ColliderMesh,
	const FGroupTopology* GroupTopology,
	FGeometrySelectionEditor* Editor,
	const FRay3d& LocalRay,
	const FGeometrySelectionUpdateConfig& UpdateConfig,
	FGeometrySelectionUpdateResult& ResultOut);


/**
 * Update a FGeometrySelection (via the Editor)
 * @param ChangeType type of change to make (add/remove/replace)
 * @param NewIDs set of new IDs to use to update the selection
 * @param Delta selection delta will be stored here, if non-nullptr is provided
 */
DYNAMICMESH_API bool UpdateSelectionWithNewElements(
	FGeometrySelectionEditor* Editor,
	EGeometrySelectionChangeType ChangeType,
	const TArray<uint64>& NewIDs,
	FGeometrySelectionDelta* DeltaOut = nullptr);


/**
 * Call VertexFunc for each selected Mesh element (vertex/edge/tri) in MeshSelection.
 * ApplyTransform will be applied to Vertex Positions before calling VertexFunc
 */
DYNAMICMESH_API bool EnumerateTriangleSelectionVertices(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	const FTransform& ApplyTransform,
	TFunctionRef<void(uint64, const FVector3d&)> VertexFunc
);
/**
 * Call VertexFunc for each selected Mesh element (vertex/edge/tri) in the set
 * of polygroup faces/edges/corners specified by GroupSelection (relative to GroupTopology parameter)
 * ApplyTransform will be applied to Vertex Positions before calling VertexFunc
 */
DYNAMICMESH_API bool EnumeratePolygroupSelectionVertices(
	const FGeometrySelection& GroupSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	const FGroupTopology* GroupTopology,
	const FTransform& ApplyTransform,
	TFunctionRef<void(uint64, const FVector3d&)> VertexFunc
);


/**
 * Call TriangleFunc for each mesh TriangleID included in MeshSelection.
 * TriangleFunc may be called multiple times for the same TriangleID.
 * This will forward to EnumerateTriangleSelectionTriangles() or 
 * EnumeratePolygroupSelectionTriangles() depending on the selection topology type.
 * If UseGroupSet and MeshSelection is for polygroups, the default Mesh group layer will be used.
 */
DYNAMICMESH_API bool EnumerateSelectionTriangles(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	TFunctionRef<void(int32)> TriangleFunc,
	const UE::Geometry::FPolygroupSet* UseGroupSet = nullptr
);
/**
 * Call TriangleFunc for each mesh TriangleID included in MeshSelection.
 * For Edges, both connected edges are included.
 * For Vertices, all triangles in the vertex one-ring are included.
 */
DYNAMICMESH_API bool EnumerateTriangleSelectionTriangles(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	TFunctionRef<void(int32)> TriangleFunc
);
/**
 * Call TriangleFunc for each mesh TriangleID included in MeshSelection, where MeshSelection has polygroup topology.
 * For Polygroup Faces, all triangles in the face are included.
 * For Polygroup Edges, currently all triangles in any group adjacent to the edge
 * For Polygroup Corners, currently all triangles in any group touching the corner
 */
DYNAMICMESH_API bool EnumeratePolygroupSelectionTriangles(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	const UE::Geometry::FPolygroupSet& GroupSet,
	TFunctionRef<void(int32)> TriangleFunc
);


/**
 * Call VertexFunc/EdgeFunc/TriangleFunc for the vertices/edges/triangles identified by MeshSelection.
 * Since a MeshSelection only stores vertices, edges, or triangles, but not combined, only one
 * of these functions will be invoked during a call to this function.
 * This function is useful to collect up geometry that needs to be rendered for a given MeshSelection
 * @param ApplyTransform if non-null, transform is applied to the 3D geometry
 */
DYNAMICMESH_API bool EnumerateTriangleSelectionElements(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	TFunctionRef<void(int32, const FVector3d&)> VertexFunc,
	TFunctionRef<void(int32, const FSegment3d&)> EdgeFunc,
	TFunctionRef<void(int32, const FTriangle3d&)> TriangleFunc,
	const FTransform* ApplyTransform = nullptr
);
/**
 * Call VertexFunc/EdgeFunc/TriangleFunc for the mesh vertices/edges/triangles identified by MeshSelection,
 * where MeshSelection has polygroup topology referring to the provided GroupTopology
 * Since a MeshSelection only stores vertices, edges, or triangles, but not combined, only one
 * of these functions will be invoked during a call to this function.
 * This function is useful to collect up geometry that needs to be rendered for a given MeshSelection
 * @param ApplyTransform if non-null, transform is applied to the 3D geometry
 */
DYNAMICMESH_API bool EnumeratePolygroupSelectionElements(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	const FGroupTopology* GroupTopology,
	TFunctionRef<void(int32, const FVector3d&)> VertexFunc,
	TFunctionRef<void(int32, const FSegment3d&)> EdgeFunc,
	TFunctionRef<void(int32, const FTriangle3d&)> TriangleFunc,
	const FTransform* ApplyTransform = nullptr
);


/**
 * Convert a MeshSelection with Polygroup topology type to a FGroupTopologySelection
 */
DYNAMICMESH_API bool ConvertPolygroupSelectionToTopologySelection(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	const FGroupTopology* GroupTopology,
	FGroupTopologySelection& TopologySelectionOut
);


/**
 * Compute a 3D Frame suitable for use as a 3D transform gizmo position/orientation
 * for the given MeshSelection
 */
DYNAMICMESH_API bool GetTriangleSelectionFrame(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	FFrame3d& SelectionFrameOut);


}
}
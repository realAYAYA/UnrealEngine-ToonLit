// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DynamicMesh/DynamicMesh3.h"
#include "Util/ProgressCancel.h"

namespace UE
{
namespace Geometry
{

class FGroupTopology;
class FDynamicMeshChangeTracker;
class FEdgeLoop;


/**
 * FMeshBevel applies a "Bevel" operation to edges of a FDynamicMesh3. Bevel is not strictly well-defined,
 * there are a wide range of possible cases to handle and currently only some are supported.
 * See this website for a discussion of many interesting cases (most are not currently supported): https://wiki.blender.org/wiki/User:Howardt/Bevel
 * 
 * The bevel operation is applied in-place to the input mesh. The bevel is mesh-topological, ie implemented by
 * un-stitching mesh edges and inserting new triangles, rather than via booleans/etc. 
 * 
 * Currently supports:
 *   - Bevel an isolated closed loop of edges, that edge-loop becomes a quad-strip (simplest case)
 *   - Bevel a set of open edge spans that may meet a T-junctions
 *       - if incoming-span-valence at a vertex is >= 3, vertex is replaced by a polygon
 *       - if incoming-span-valence at a vertex is 1, bevel is "terminated" by expanding the vertex into an edge (this is the messiest case)
 *          - vertex-on-boundary is a special case that is simpler
 *       - edge spans are replaced by quad-strips
 * 
 * Currently does not support:
 *   - Beveling an isolated vertex
 *   - partial bevels of a GroupTopology Edge
 *   - multiple-segment bevel (eg to do rounds/etc)
 *   - ???
 * 
 * Generally this FMeshBevel is intended to be applied to group-boundary "edges" in a FGroupTopology. However the FGroupTopology
 * is currently only used in the initial setup, so other methods could be used to construct the bevel topology. 
 * 
 * Currently "updating" the bevel shape, once the topology is known, is not supported, but could be implemented
 * relatively easily, as all the relevant information is already tracked and stored.
 */
class DYNAMICMESH_API FMeshBevel
{
public:

	//
	// Inputs
	// 

	/** Distance that bevel edges/vertices are inset from their initial position. Not guaranteed to hold for all vertices, though. */
	double InsetDistance = 5.0;
	
	/** Options for MaterialID assignment on the new triangles generated for the bevel */
	enum class EMaterialIDMode
	{
		ConstantMaterialID,
		InferMaterialID,
		InferMaterialID_ConstantIfAmbiguous
	};
	/** Which MaterialID assignment mode to use */
	EMaterialIDMode MaterialIDMode = EMaterialIDMode::ConstantMaterialID;
	/** Constant MaterialID used for various MaterialIDMode settings */
	int32 SetConstantMaterialID = 0;

	/** Set this member to support progress/cancel in the computations below */
	FProgressCancel* Progress = nullptr;


	//
	// Outputs
	//

	/** list of all new triangles created by the operation */
	TArray<int32> NewTriangles;

	/** status of the operation, warnings/errors may be returned here */
	FGeometryResult ResultInfo = FGeometryResult(EGeometryResultType::InProgress);

public:
	/**
	 * Initialize the bevel with all edges of the given GroupTopology
	 */
	void InitializeFromGroupTopology(const FDynamicMesh3& Mesh, const FGroupTopology& Topology);

	/**
	 * Initialize the bevel with the specified edges of a GroupTopology
	 */
	void InitializeFromGroupTopologyEdges(const FDynamicMesh3& Mesh, const FGroupTopology& Topology, const TArray<int32>& GroupEdges);

	/**
	* Initialize the bevel with the specified faces of a GroupTopology
	* @return false if any selection-bowtie vertices were found, in this case we cannot compute the bevel
	*/
	bool InitializeFromGroupTopologyFaces(const FDynamicMesh3& Mesh, const FGroupTopology& Topology, const TArray<int32>& GroupFaces);

	/**
	* Initialize the bevel with border loops of the selected triangles. 
	* @return false if any selection-bowtie vertices were found, in this case we cannot compute the bevel
	*/
	bool InitializeFromTriangleSet(const FDynamicMesh3& Mesh, const TArray<int32>& Triangles);

	/**
	 * Apply the bevel operation to the mesh, and optionally track changes
	 */
	bool Apply(FDynamicMesh3& Mesh, FDynamicMeshChangeTracker* ChangeTracker);




public:

	//
	// Current Bevel computation strategy is basically to fully precompute all the necessary info for the entire bevel,
	// then "unlink" all the edges and vertices, and then stitch it all back together. 
	// Various incremental strategies were attempted, however certain cases like a two "bevel vertices" connected by a 
	// single mesh edge greatly complicate any attempt to decompose the problem into sub-parts. 
	// 
	// The data structures below are used to track this topological information during the operation.
	// Note that some parts of these data structures may become invalid/incorrect as the operation proceeds...
	//

	// POSSIBLE IMPROVEMENTS:
	// * compute wedges for Loop/Edge in setup? would avoid having to deal w/ possibly weird 
	//   configurations introduced by unlink of corners...

	// FBevelLoop is the accumulated data for a closed loop of mesh-edges, with no T-junctions w/ other bevel-edges.
	// This is the easiest case as each mesh-edge of the loop expands out into a quad, with no complex vertex-polygons/etc
	struct FBevelLoop
	{
		// initial topological information that defines what happens in unlink/displace/mesh steps
		TArray<int32> MeshVertices;		// sequential list of mesh vertex IDs along edge loop
		TArray<int32> MeshEdges;		// sequential list of mesh edge IDs along edge loop
		TArray<FIndex2i> MeshEdgeTris;	// the one or two triangles associated w/ each MeshEdges element in the input mesh

		// new mesh topology computed during unlink step
		TArray<int32> NewMeshVertices;		// list of new vertices on "other" side of unlinked edge, 1-1 with MeshVertices
		TArray<int32> NewMeshEdges;			// list of new edges on "other" side of unlinked edge, 1-1 with MeshEdges

		// buffers for new vertex positions computed during displace step
		TArray<FVector3d> NewPositions0;	// new positions for MeshVertices list
		TArray<FVector3d> NewPositions1;	// new positions for NewMeshVertices list

		// new geometry computed during mesh step
		TArray<int32> NewGroupIDs;
		TArray<FIndex2i> StripQuads;		// triangle-ID-pairs for each new quad added along edge, 1-1 with MeshEdges
	};

	// FBevelEdge is the accumulated data for an open span of mesh-edges, which possibly meets up with other bevel-edges
	// at the vertices on either end of the span. Each mesh-edge of the bevel-edge will become a quad
	struct FBevelEdge
	{
		// initial topological information that defines what happens in unlink/displace/mesh steps
		TArray<int32> MeshVertices;		// sequential list of mesh vertex IDs along edge
		TArray<int32> MeshEdges;		// sequential list of mesh edge IDs along edge
		TArray<FIndex2i> MeshEdgeTris;	// the one or two triangles associated w/ each MeshEdges element in the input mesh
		int32 GroupEdgeID;				// ID of this edge in external topology (eg FGroupTopology)
		FIndex2i GroupIDs;				// topological IDs of groups on either side of topological edge
		bool bEndpointBoundaryFlag[2];	// flag defining whether vertex at start/end of MeshVertices was a boundary vertex

		// new mesh topology computed during unlink step
		TArray<int32> NewMeshVertices;		// list of new vertices on "other" side of unlinked edge, 1-1 with MeshVertices
		TArray<int32> NewMeshEdges;			// list of new edges on "other" side of unlinked edge, 1-1 with MeshEdges

		// buffers for new vertex positions computed during displace step
		TArray<FVector3d> NewPositions0;	// new positions for MeshVertices list
		TArray<FVector3d> NewPositions1;	// new positions for NewMeshVertices list

		// new geometry computed during mesh step
		int32 NewGroupID;
		TArray<FIndex2i> StripQuads;		// triangle-ID-pairs for each new quad added along edge, 1-1 with MeshEdges
	};


	// A FOneRingWedge represents a sequential set of connected triangles around a vertex, ie a subset of an ordered triangle one-ring.
	// Used to represent the desired bevel topology in FBevelVertex
	struct FOneRingWedge
	{
		TArray<int32> Triangles;				// list of sequential triangles in this wedge
		FIndex2i BorderEdges;					// "first" and "last" Edges of sequential triangles in Triangles list (connected to central Vertex)
		FIndex2i BorderEdgeTriEdgeIndices;		// index 0/1/2 of BorderEdges[j] in start/ed Triangles

		int32 WedgeVertex;						// central vertex of this wedge (updated by unlink functions)

		FVector3d NewPosition;					// new calculated position for vertex of this wedge
	};

	// a FBevelVertex can have various types, depending on the topology of the bevel edge graph and input mesh
	enum class EBevelVertexType
	{
		// A JunctionVertex is a vertex at which 2 or more FBevelEdges meet (ie is an endpoint of 2 or more of those vertex-spans)
		// If N>=3 or more edges meet at a JunctionVertex, it will become a polygon with N vertices, one for each "wedge"
		JunctionVertex,
		// A TerminatorVertex is a vertex at which a single FBevelEdge terminates, ie the N=1 case. This requires different handling
		// because we essentially want to turn that vertex into an edge, which means inserting a triangle into the adjacent one-ring
		TerminatorVertex,
		// a BoundaryVertex is a junction/terminator on the mesh boundary
		BoundaryVertex,
		// An Unknown vertex is one at which we don't know what to do, or some error occurred while processing as a Junction/Terminator.
		Unknown
	};

	// A FBevelVertex repesents/stores the accumulated data at a "bevel vertex", which is the mesh vertex at the end of a FBevelEdge.
	// A FBevelVertex may be expanded out into a polygon or just an edge, depending on its Type
	struct FBevelVertex
	{
		int32 VertexID;												// Initial Mesh Vertex ID for the Bevel Vertex
		int32 CornerID;												// Initial Group Topology Corner ID for the Bevel Vertex (if exists)
		EBevelVertexType VertexType = EBevelVertexType::Unknown;	// Type of the Bevel Vertex

		TArray<int32> IncomingBevelMeshEdges;						// Set of (unsorted) Mesh Edges that are destined to be Beveled, coming into the vertex
		TArray<int32> IncomingBevelTopoEdges;						// Set of (unsorted) Group Topology Edges that are to be Beveled, coming into the vertex

		TArray<int32> SortedTriangles;			// ordered triangle one-ring around VertexID
		TArray<FOneRingWedge> Wedges;			// ordered decomposition of one-ring into "wedges" between incoming bevel edges (no correspondence w/ IncomingBevelMeshEdges list)

		int32 NewGroupID;						// new polygroup allocated for the beveled polygon generated by this vertex (if NumEdges > 2)
		TArray<int32> NewTriangles;				// new triangles that make up the beveled polygon for this vertex (if NumEdges > 2)

		FIndex2i TerminatorInfo;				// for TerminatorVertex type, store [EdgeID, FarVertexID] in one-ring, used to unlink/fill (see usage)
		int32 ConnectedBevelVertex = -1;		// If set to another FBevelVertex index, then the TerminatorInfo.EdgeID directly connects to that vertex and special handling is needed
	};



public:
	TMap<int32, int32> VertexIDToIndexMap;		// map of mesh-vertex-IDs to indices into Vertices list
	TArray<FBevelVertex> Vertices;				// list of FBevelVertex data structures for mesh vertices that need beveling

	TArray<FBevelEdge> Edges;					// list of FBevelEdge data structures for mesh edge-spans that need beveling
	TArray<FBevelLoop> Loops;					// list of FBevelLoop data structures for mesh edge-loops that need beveling

	TMap<int32, int32> MeshEdgePairs;			// Many edges of the input mesh will be split into edge pairs, which are then stitched together with quads.
												// This map stores the authoritative correspondences between these edge pairs. Both pairs, ie (a,b) and (b,a) are stored.



protected:

	FBevelVertex* GetBevelVertexFromVertexID(int32 VertexID);

	// Setup phase: register Edges (spans) and (isolated) Loops that need to be beveled and precompute/store any mesh topology that must be tracked across the operation
	// Required BevelVertex's are added by AddBevelGroupEdge()
	// Once edges are configured, BuildVertexSets() is called to precompute the vertex topological information

	void AddBevelGroupEdge(const FDynamicMesh3& Mesh, const FGroupTopology& Topology, int32 GroupEdgeID);
	void AddBevelEdgeLoop(const FDynamicMesh3& Mesh, const FEdgeLoop& Loop);
	void BuildVertexSets(const FDynamicMesh3& Mesh);
	void BuildJunctionVertex(FBevelVertex& Vertex, const FDynamicMesh3& Mesh);
	void BuildTerminatorVertex(FBevelVertex& Vertex, const FDynamicMesh3& Mesh);

	// Unlink phase - disconnect triangles along bevel edges/loops, and at vertices. 
	// Vertices may expand out into multiple "wedges" depending on incoming bevel-edge topology.

	void UnlinkEdges( FDynamicMesh3& Mesh, FDynamicMeshChangeTracker* ChangeTracker);
	void UnlinkBevelEdgeInterior( FDynamicMesh3& Mesh, FBevelEdge& BevelEdge, FDynamicMeshChangeTracker* ChangeTracker);

	void UnlinkLoops( FDynamicMesh3& Mesh, FDynamicMeshChangeTracker* ChangeTracker);
	void UnlinkBevelLoop( FDynamicMesh3& Mesh, FBevelLoop& BevelLoop, FDynamicMeshChangeTracker* ChangeTracker);

	void UnlinkVertices( FDynamicMesh3& Mesh, FDynamicMeshChangeTracker* ChangeTracker);
	void UnlinkJunctionVertex(FDynamicMesh3& Mesh, FBevelVertex& BevelVertex, FDynamicMeshChangeTracker* ChangeTracker);
	void UnlinkTerminatorVertex(FDynamicMesh3& Mesh, FBevelVertex& BevelVertex, FDynamicMeshChangeTracker* ChangeTracker);

	void FixUpUnlinkedBevelEdges(FDynamicMesh3& Mesh);

	// Displace phase - move unlinked vertices to new positions

	void DisplaceVertices(FDynamicMesh3& Mesh, double Distance);

	// Meshing phase - append quad-strips between unlinked edge spans/loops, polygons at junction vertices where required,
	// and triangles at terminator vertices

	void CreateBevelMeshing(FDynamicMesh3& Mesh);
	void AppendJunctionVertexPolygon(FDynamicMesh3& Mesh, FBevelVertex& Vertex);
	void AppendTerminatorVertexTriangle(FDynamicMesh3& Mesh, FBevelVertex& Vertex);
	void AppendTerminatorVertexPairQuad(FDynamicMesh3& Mesh, FBevelVertex& Vertex0, FBevelVertex& Vertex1);
	void AppendEdgeQuads(FDynamicMesh3& Mesh, FBevelEdge& Edge);
	void AppendLoopQuads(FDynamicMesh3& Mesh, FBevelLoop& Loop);

	// Normals phase - calculate normals for new geometry

	void ComputeNormals(FDynamicMesh3& Mesh);
	void ComputeUVs(FDynamicMesh3& Mesh);
	void ComputeMaterialIDs(FDynamicMesh3& Mesh);
};



} // end namespace UE::Geometry
} // end namespace UE
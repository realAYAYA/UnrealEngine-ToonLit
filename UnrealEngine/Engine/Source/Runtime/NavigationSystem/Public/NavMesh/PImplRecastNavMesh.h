// Copyright Epic Games, Inc. All Rights Reserved.

//
// Private implementation for communication with Recast library
// 
// All functions should be called through RecastNavMesh actor to make them thread safe!
//

#pragma once 

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/RecastQueryFilter.h"
#include "AI/NavigationSystemBase.h"
#include "VisualLogger/VisualLogger.h"

#if RECAST_INTERNAL_DEBUG_DATA
#include "NavMesh/RecastInternalDebugData.h"
#endif

#if WITH_RECAST
#include "Detour/DetourNavMesh.h"
#include "Detour/DetourNavMeshQuery.h"
#endif

class FRecastNavMeshGenerator;

#if WITH_RECAST

#define RECAST_VERY_SMALL_AGENT_RADIUS 0.0f

// LWC_TODO_AI: Costs and pathing distance should be FReal. Not until after 5.0!
/** Engine Private! - Private Implementation details of ARecastNavMesh */
class NAVIGATIONSYSTEM_API FPImplRecastNavMesh
{
public:

	/** Constructor */
	FPImplRecastNavMesh(ARecastNavMesh* Owner);

	/** Dtor */
	~FPImplRecastNavMesh();

	/**
	 * Serialization.
	 * @param Ar - The archive with which to serialize.
	 * @returns true if serialization was successful.
	 */
	void Serialize(FArchive& Ar, int32 NavMeshVersion);

	/* Gather debug geometry.
	 * @params OutGeometry Output geometry.
	 * @params TileIndex Used to collect geometry for a specific tile, INDEX_NONE will gather all tiles.
	 * @return True if done collecting.
	 */
	bool GetDebugGeometryForTile(FRecastDebugGeometry& OutGeometry, int32 TileIndex) const;
	
	/** Returns bounding box for the whole navmesh. */
	FBox GetNavMeshBounds() const;

	/** Returns bounding box for a given navmesh tile. */
	FBox GetNavMeshTileBounds(int32 TileIndex) const;

	/** Retrieves XY and layer coordinates of tile specified by index */
	bool GetNavMeshTileXY(int32 TileIndex, int32& OutX, int32& OutY, int32& OutLayer) const;

	/** Retrieves XY coordinates of tile specified by position */
	bool GetNavMeshTileXY(const FVector& Point, int32& OutX, int32& OutY) const;

	/** Retrieves all tile indices at matching XY coordinates */
	void GetNavMeshTilesAt(int32 TileX, int32 TileY, TArray<int32>& Indices) const;

	/** Retrieves list of tiles that intersect specified bounds */
	void GetNavMeshTilesIn(const TArray<FBox>& InclusionBounds, TArray<int32>& Indices) const;

	/** Retrieves number of tiles in this navmesh */
	FORCEINLINE int32 GetNavMeshTilesCount() const { return DetourNavMesh ? DetourNavMesh->getMaxTiles() : 0; }

	/** Supported queries */

	// @TODONAV
	/** Generates path from the given query. Synchronous. */
	UE_DEPRECATED(4.25, "Use the version with the added CostLimit parameter (FLT_MAX can be used as default).")
	ENavigationQueryResult::Type FindPath(const FVector& StartLoc, const FVector& EndLoc, FNavMeshPath& Path, const FNavigationQueryFilter& Filter, const UObject* Owner) const;
	
	/** Generates path from the given query. Synchronous. */
	ENavigationQueryResult::Type FindPath(const FVector& StartLoc, const FVector& EndLoc, const float CostLimit, FNavMeshPath& Path, const FNavigationQueryFilter& Filter, const UObject* Owner) const;

	/** Check if path exists */
	ENavigationQueryResult::Type TestPath(const FVector& StartLoc, const FVector& EndLoc, const FNavigationQueryFilter& Filter, const UObject* Owner, int32* NumVisitedNodes = 0) const;

	template< typename TRecastAStar, typename TRecastAStartGraph, typename TRecastGraphAStarFilter, typename TRecastAStarResult >
	ENavigationQueryResult::Type FindPathCustomAStar(TRecastAStartGraph& RecastGraphWrapper, TRecastAStar& AStarAlgo, const FVector& StartLoc, const FVector& EndLoc, const float CostLimit, FNavMeshPath& Path, const FNavigationQueryFilter& Filter, const UObject* Owner) const;

	/** Checks if the whole segment is in navmesh */
	void Raycast(const FVector& StartLoc, const FVector& EndLoc, const FNavigationQueryFilter& InQueryFilter, const UObject* Owner,
		ARecastNavMesh::FRaycastResult& RaycastResult, NavNodeRef StartNode = INVALID_NAVNODEREF) const;

	/** Generates path from given query and collect data for every step of A* algorithm */
	int32 DebugPathfinding(const FVector& StartLoc, const FVector& EndLoc, const float CostLimit, const FNavigationQueryFilter& Filter, const UObject* Owner, TArray<FRecastDebugPathfindingData>& Steps);

	/** Returns a random location on the navmesh. */
	FNavLocation GetRandomPoint(const FNavigationQueryFilter& Filter, const UObject* Owner) const;

#if WITH_NAVMESH_CLUSTER_LINKS
	/** Check if path exists using cluster graph */
	ENavigationQueryResult::Type TestClusterPath(const FVector& StartLoc, const FVector& EndLoc, int32* NumVisitedNodes = 0) const;

	/** Returns a random location on the navmesh within cluster */
	bool GetRandomPointInCluster(NavNodeRef ClusterRef, FNavLocation& OutLocation) const;
#endif // WITH_NAVMESH_CLUSTER_LINKS

	/**	Tries to move current nav location towards target constrained to navigable area. Faster than ProjectPointToNavmesh.
	 *	@param OutLocation if successful this variable will be filed with result
	 *	@return true if successful, false otherwise
	 */
	bool FindMoveAlongSurface(const FNavLocation& StartLocation, const FVector& TargetPosition, FNavLocation& OutLocation, const FNavigationQueryFilter& Filter, const UObject* Owner) const;

	bool ProjectPointToNavMesh(const FVector& Point, FNavLocation& Result, const FVector& Extent, const FNavigationQueryFilter& Filter, const UObject* Owner) const;
	
	/** Project single point and grab all vertical intersections */
	bool ProjectPointMulti(const FVector& Point, TArray<FNavLocation>& OutLocations, const FVector& Extent,
		FVector::FReal MinZ, FVector::FReal MaxZ, const FNavigationQueryFilter& Filter, const UObject* Owner) const;

	/** Returns nearest navmesh polygon to Loc, or INVALID_NAVMESHREF if Loc is not on the navmesh. */
	NavNodeRef FindNearestPoly(FVector const& Loc, FVector const& Extent, const FNavigationQueryFilter& Filter, const UObject* Owner) const;

	/** Finds the polygons along the navigation graph that touch the specified circle. Return true if found any. */
	bool FindPolysAroundCircle(const FVector& CenterPos, const NavNodeRef CenterNodeRef, const FVector::FReal Radius, const FNavigationQueryFilter& Filter, const UObject* Owner, TArray<NavNodeRef>* OutPolys = nullptr, TArray<NavNodeRef>* OutPolysParent = nullptr, TArray<float>* OutPolysCost = nullptr, int32* OutPolysCount = nullptr) const;

	/** Retrieves all polys within given pathing distance from StartLocation.
	 *	@NOTE query is not using string-pulled path distance (for performance reasons),
	 *		it measured distance between middles of portal edges, do you might want to 
	 *		add an extra margin to PathingDistance */
	bool GetPolysWithinPathingDistance(FVector const& StartLoc, const float PathingDistance,
		const FNavigationQueryFilter& Filter, const UObject* Owner,
		TArray<NavNodeRef>& FoundPolys, FRecastDebugPathfindingData* DebugData) const;

	//@todo document
	void GetEdgesForPathCorridor(const TArray<NavNodeRef>* PathCorridor, TArray<FNavigationPortalEdge>* PathCorridorEdges) const;

	/** finds stringpulled path from given corridor */
	bool FindStraightPath(const FVector& StartLoc, const FVector& EndLoc, const TArray<NavNodeRef>& PathCorridor, TArray<FNavPathPoint>& PathPoints, TArray<uint32>* CustomLinks = NULL) const;

	/** Filters nav polys in PolyRefs with Filter */
	bool FilterPolys(TArray<NavNodeRef>& PolyRefs, const FRecastQueryFilter* Filter, const UObject* Owner) const;

	/** Get all polys from tile */
	bool GetPolysInTile(int32 TileIndex, TArray<FNavPoly>& Polys) const;

	/** Updates area on polygons creating point-to-point connection with given UserId */
	void UpdateNavigationLinkArea(int32 UserId, uint8 AreaType, uint16 PolyFlags) const;

#if WITH_NAVMESH_SEGMENT_LINKS
	/** Updates area on polygons creating segment-to-segment connection with given UserId */
	void UpdateSegmentLinkArea(int32 UserId, uint8 AreaType, uint16 PolyFlags) const;
#endif // WITH_NAVMESH_SEGMENT_LINKS

	/** Retrieves center of the specified polygon. Returns false on error. */
	bool GetPolyCenter(NavNodeRef PolyID, FVector& OutCenter) const;
	/** Retrieves the vertices for the specified polygon. Returns false on error. */
	bool GetPolyVerts(NavNodeRef PolyID, TArray<FVector>& OutVerts) const;
	/** Retrieves a random point inside the specified polygon. Returns false on error. */
	bool GetRandomPointInPoly(NavNodeRef PolyID, FVector& OutPoint) const;
	/** Retrieves the flags for the specified polygon. Returns false on error. */
	bool GetPolyData(NavNodeRef PolyID, uint16& Flags, uint8& AreaType) const;
	/** Retrieves area ID for the specified polygon. */
	uint32 GetPolyAreaID(NavNodeRef PolyID) const;
	/** Sets area ID for the specified polygon. */
	void SetPolyAreaID(NavNodeRef PolyID, uint8 AreaID);
	/** Finds all polys connected with specified one */
	bool GetPolyNeighbors(NavNodeRef PolyID, TArray<FNavigationPortalEdge>& Neighbors) const;
	/** Finds all polys connected with specified one, results expressed as array of NavNodeRefs */
	bool GetPolyNeighbors(NavNodeRef PolyID, TArray<NavNodeRef>& Neighbors) const;
	/** Finds all polys connected with specified one */
	bool GetPolyEdges(NavNodeRef PolyID, TArray<FNavigationPortalEdge>& Edges) const;
	/** Finds closest point constrained to given poly */
	bool GetClosestPointOnPoly(NavNodeRef PolyID, const FVector& TestPt, FVector& PointOnPoly) const;
	/** Decode poly ID into tile index and poly index */
	bool GetPolyTileIndex(NavNodeRef PolyID, uint32& PolyIndex, uint32& TileIndex) const;
	/** Decode poly ID into FNavTileRef and poly index */
	bool GetPolyTileRef(NavNodeRef PolyId, uint32& OutPolyIndex, FNavTileRef& OutTileRef) const;
	/** Retrieves user ID for given offmesh link poly */
	uint32 GetLinkUserId(NavNodeRef LinkPolyID) const;
	/** Retrieves start and end point of offmesh link */
	bool GetLinkEndPoints(NavNodeRef LinkPolyID, FVector& PointA, FVector& PointB) const;
	/** Check if poly is a custom link */
	bool IsCustomLink(NavNodeRef PolyRef) const;

#if	WITH_NAVMESH_CLUSTER_LINKS
	/** Retrieves bounds of cluster. Returns false on error. */
	bool GetClusterBounds(NavNodeRef ClusterRef, FBox& OutBounds) const;
	NavNodeRef GetClusterRefFromPolyRef(const NavNodeRef PolyRef) const;
#endif // WITH_NAVMESH_CLUSTER_LINKS

	uint32 GetTileIndexFromPolyRef(const NavNodeRef PolyRef) const { return DetourNavMesh != NULL ? DetourNavMesh->decodePolyIdTile(PolyRef) : uint32(INDEX_NONE); }

	static uint16 GetFilterForbiddenFlags(const FRecastQueryFilter* Filter);
	static void SetFilterForbiddenFlags(FRecastQueryFilter* Filter, uint16 ForbiddenFlags);

	void OnAreaCostChanged();

public:
	dtNavMesh const* GetRecastMesh() const { return DetourNavMesh; };
	dtNavMesh* GetRecastMesh() { return DetourNavMesh; };
	void ReleaseDetourNavMesh();

	void RemoveTileCacheLayers(int32 TileX, int32 TileY);
	void RemoveTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx);
	void AddTileCacheLayers(int32 TileX, int32 TileY, const TArray<FNavMeshTileData>& Layers);
	void AddTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx, const FNavMeshTileData& LayerData);
	void MarkEmptyTileCacheLayers(int32 TileX, int32 TileY);
	FNavMeshTileData GetTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx) const;
	TArray<FNavMeshTileData> GetTileCacheLayers(int32 TileX, int32 TileY) const;
	bool HasTileCacheLayers(int32 TileX, int32 TileY) const;

	/** Assigns recast generated navmesh to this instance.
	 *	@param bOwnData if true from now on this FPImplRecastNavMesh instance will be responsible for this piece 
	 *		of memory
	 */
	void SetRecastMesh(dtNavMesh* NavMesh);

	float GetTotalDataSize() const;

	/** Gets the size of the compressed tile cache, this is slow */
#if !UE_BUILD_SHIPPING
	int32 GetCompressedTileCacheSize();
#endif

	/** Called on world origin changes */
	void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift);

	/** calculated cost of given segment if traversed on specified poly. Function measures distance between specified points
	 *	and returns cost of traversing this distance on given poly.
	 *	@note no check if segment is on poly is performed. */
	float CalcSegmentCostOnPoly(NavNodeRef PolyID, const dtQueryFilter* Filter, const FVector& StartLoc, const FVector& EndLoc) const;

	ARecastNavMesh* NavMeshOwner;
	
	/** Recast's runtime navmesh data that we can query against */
	dtNavMesh* DetourNavMesh;

	/** Compressed layers data, can be reused for tiles generation */
	TMap<FIntPoint, TArray<FNavMeshTileData> > CompressedTileCacheLayers;

#if RECAST_INTERNAL_DEBUG_DATA
	TMap<FIntPoint, FRecastInternalDebugData> DebugDataMap;
#endif

	/** query used for searching data on game thread */
	mutable dtNavMeshQuery SharedNavQuery;

	/** Helper function to serialize a single Recast tile. */
	static void SerializeRecastMeshTile(FArchive& Ar, int32 NavMeshVersion, unsigned char*& TileData, int32& TileDataSize);

	/** Helper function to serialize a Recast tile compressed data. */
	static void SerializeCompressedTileCacheData(FArchive& Ar, int32 NavMeshVersion, unsigned char*& CompressedData, int32& CompressedDataSize);

	/** Initialize data for pathfinding */
	bool InitPathfinding(const FVector& UnrealStart, const FVector& UnrealEnd, 
		const dtNavMeshQuery& Query, const dtQueryFilter* Filter,
		FVector& RecastStart, dtPolyRef& StartPoly,
		FVector& RecastEnd, dtPolyRef& EndPoly) const;

	/** Marks path flags, perform string pulling if needed */
	void PostProcessPath(dtStatus PathfindResult, FNavMeshPath& Path,
		const dtNavMeshQuery& Query, const dtQueryFilter* Filter,
		NavNodeRef StartNode, NavNodeRef EndNode,
		FVector UnrealStart, FVector UnrealEnd,
		FVector RecastStart, FVector RecastEnd,
		dtQueryResult& PathResult) const;

	void GetDebugPolyEdges(const dtMeshTile& Tile, bool bInternalEdges, bool bNavMeshEdges, TArray<FVector>& InternalEdgeVerts, TArray<FVector>& NavMeshEdgeVerts) const;

	/** workhorse function finding portal edges between corridor polys */
	void GetEdgesForPathCorridorImpl(const TArray<NavNodeRef>* PathCorridor, TArray<FNavigationPortalEdge>* PathCorridorEdges, const dtNavMeshQuery& NavQuery) const;

protected:
	/** 
	 *	@param ForbiddenFlags polys with flags matching the fillter will get added to 
	 */
	int32 GetTilesDebugGeometry(const FRecastNavMeshGenerator* Generator, const dtMeshTile& Tile, int32 VertBase, FRecastDebugGeometry& OutGeometry, int32 TileIdx = INDEX_NONE, uint16 ForbiddenFlags = 0) const;

	ENavigationQueryResult::Type PostProcessPathInternal(dtStatus FindPathStatus, FNavMeshPath& Path, 
		const dtNavMeshQuery& NavQuery, const dtQueryFilter* QueryFilter, 
		NavNodeRef StartPolyID, NavNodeRef EndPolyID, 
		const FVector& RecastStartPos, const FVector& RecastEndPos, 
		dtQueryResult& PathResult) const;
};

template< typename TRecastAStar, typename TRecastAStartGraph, typename TRecastGraphAStarFilter, typename TRecastAStarResult >
ENavigationQueryResult::Type FPImplRecastNavMesh::FindPathCustomAStar(TRecastAStartGraph& RecastGraphWrapper, TRecastAStar& AStarAlgo, const FVector& StartLoc, const FVector& EndLoc, const float CostLimit, FNavMeshPath& Path, const FNavigationQueryFilter& InQueryFilter, const UObject* Owner) const
{
	const FRecastQueryFilter* FilterImplementation = (const FRecastQueryFilter*)(InQueryFilter.GetImplementation());
	if (FilterImplementation == nullptr)
	{
		UE_VLOG(NavMeshOwner, LogNavigation, Error, TEXT("FPImplRecastNavMesh::FindPath failed due to passed filter having NULL implementation!"));
		return ENavigationQueryResult::Error;
	}

	const dtQueryFilter* QueryFilter = FilterImplementation->GetAsDetourQueryFilter();
	if (QueryFilter == nullptr)
	{
		UE_VLOG(NavMeshOwner, LogNavigation, Warning, TEXT("FPImplRecastNavMesh::FindPath failing due to QueryFilter == NULL"));
		return ENavigationQueryResult::Error;
	}

	// initialize recast wrapper with the NavMeshOwner, not multithread safe!
	RecastGraphWrapper.Initialize(NavMeshOwner);
	TRecastGraphAStarFilter AStarFilter(RecastGraphWrapper, *FilterImplementation, InQueryFilter.GetMaxSearchNodes(), CostLimit, Owner);

	FVector RecastStartPos, RecastEndPos;
	NavNodeRef StartPolyID, EndPolyID;
	const bool bCanSearch = InitPathfinding(StartLoc, EndLoc, RecastGraphWrapper.GetRecastQuery(), QueryFilter, RecastStartPos, StartPolyID, RecastEndPos, EndPolyID);
	if (!bCanSearch)
	{
		return ENavigationQueryResult::Error;
	}

	typename TRecastAStar::FSearchNode StartNode(StartPolyID, RecastStartPos);
	typename TRecastAStar::FSearchNode EndNode(EndPolyID, RecastEndPos);
	StartNode.Initialize(RecastGraphWrapper);
	EndNode.Initialize(RecastGraphWrapper);
	TRecastAStarResult PathResult;
	auto AStarResult = AStarAlgo.FindPath(StartNode, EndNode, AStarFilter, PathResult);

	dtStatus FindPathStatus = RecastGraphWrapper.ConvertToRecastStatus(AStarAlgo, AStarFilter, AStarResult);
	return PostProcessPathInternal(FindPathStatus, Path, RecastGraphWrapper.GetRecastQuery(), FilterImplementation, StartNode.NodeRef, EndNode.NodeRef, 
		FVector(StartNode.Position[0], StartNode.Position[1], StartNode.Position[2]), 
		FVector(EndNode.Position[0], EndNode.Position[1], EndNode.Position[2]),
		PathResult);
}

#endif	// WITH_RECAST

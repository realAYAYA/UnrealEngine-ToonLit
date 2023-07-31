// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Polygroups/PolygroupSet.h"

using UE::Geometry::FDynamicMesh3;


namespace UE
{
namespace Geometry
{

/**
 * FPolygroupsGenerator generates (face/tri/poly)groups for an input mesh based on the geometry and attributes of the input mesh.
 * The various FindPolygroupsFromX() are the driving functions, each performs the full computation. 
 */
class DYNAMICMESH_API FPolygroupsGenerator
{
public:

	FPolygroupsGenerator() {}
	FPolygroupsGenerator(FDynamicMesh3* MeshIn);

	// 
	// Input parameters
	//

	// source mesh
	FDynamicMesh3* Mesh = nullptr;

	// if true, then groups will be post-processed to optimize them, based on parmeters below
	bool bApplyPostProcessing = true;

	// if > 1, groups with a triangle count smaller than this will be merged with a neighbouring group
	int32 MinGroupSize = 0;

	// if true, after groups are computed they will be copied to the output mesh
	bool bCopyToMesh = true;

	//
	// Outputs
	//

	// lists of triangle IDs, each list defines a polygroup/polygon
	TArray<TArray<int>> FoundPolygroups;

	// list of edge IDs of mesh edges that are on polygroup borders
	TArray<int> PolygroupEdges;


	/**
	 * Find Polygroups by randomly picking initial seed triangles and then flood-filling outwards,
	 * stopping when the opening angle at an edge is larger than the angle defined by the DotTolerance.
	 */
	bool FindPolygroupsFromFaceNormals(
		double DotTolerance = 0.0001,
		bool bRespectUVSeams = false,
		bool bRespectNormalSeams = false);

	/**
	 * Find Polygroups based on UV Islands, ie each UV Island becomes a Polygroup
	 */
	bool FindPolygroupsFromUVIslands(int32 UVLayer = 0);

	/**
	* Find Polygroups based on Seams in UV Overlay
	*/
	bool FindPolygroupsFromHardNormalSeams();

	/**
	 * Find Polygroups based on mesh connectivity, ie each connected-component becomes a Polygroup
	 */
	bool FindPolygroupsFromConnectedTris();

	/**
	* Find Polygroups by trying to invert triangluation of a polygon mesh
	*/
	bool FindSourceMeshPolygonPolygroups(
		bool bRespectUVSeams = true,
		bool bRespectNormalSeams = false,
		double QuadAdjacencyWeight = 1.0,
		double QuadMetricClamp = 1.0,
		int MaxSearchRounds = 1
	);

	/**
	 * Weight potions for algorithms below
	 */
	enum class EWeightingType
	{
		None,
		NormalDeviation
	};

	/**
	 * Incrementally compute approximate-geodesic-based furthest-point sampling of the mesh until NumPoints
	 * samples have been found, then compute local geodesic patches (eg approximate surface voronoi diagaram).
	 * Optionally weight geodesic-distance computation, which will produce different patch shapes.
	 * If StartingGroups are provided, then new groups are constrained to subdivide existing groups, ie
	 * none of the new groups will cross the boundaries of the StartingGroups
	 */
	bool FindPolygroupsFromFurthestPointSampling(
		int32 NumPoints, 
		EWeightingType WeightingType, 
		FVector3d WeightingCoeffs = FVector3d::One(),
		FPolygroupSet* StartingGroups = nullptr);


	/**
	* Copy the computed Polygroups to the input Mesh. Will be automatically called by the 
	* FindPolygroupsFromX() functions above if bCopyToMesh=true
	*/
	void CopyPolygroupsToMesh();

	/**
	* Copy the computed Polygroups to the given PolygroupSet and Mesh
	*/
	void CopyPolygroupsToPolygroupSet(FPolygroupSet& Polygroups, FDynamicMesh3& TargetMesh);

	/**
	 * Initialize the PolygroupEdges output member by finding all the mesh edges that are on polygroup borders.
	 * Requires that polygroups have been computed and copied to the mesh
	 */
	bool FindPolygroupEdges();


protected:
	void PostProcessPolygroups(bool bApplyMerging, TFunctionRef<bool(int32, int32)> TrisConnectedPredicate = [](int, int) { return true; });
	void OptimizePolygroups(TFunctionRef<bool(int32, int32)> TrisConnectedPredicate = [](int, int) { return true; });

	void GetSeamConstraintEdges(bool bUVSeams, bool bNormalSeams, TSet<int32>& ConstraintEdgesOut) const;
};


}	// end namespace Geometry
}	// end namespace UE

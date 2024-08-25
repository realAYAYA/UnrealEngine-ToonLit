// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "PhysicsEngine/AggregateGeom.h"

class UPrimitiveComponent;
class UStaticMesh;
class UMaterialInterface;
struct FMeshDescription;

/**
 * The CombineMeshInstances modular feature is used to provide a mechanism
 * for merging a set of instances of meshes (ie mesh + transform + materials + ...)
 * into a smaller set of meshes. Generally this involves creating simpler versions
 * of the instances and appending them into one or a small number of combined meshes.
 */
class IGeometryProcessing_CombineMeshInstances : public IModularFeature
{
public:
	virtual ~IGeometryProcessing_CombineMeshInstances() {}


	enum class EMeshDetailLevel
	{
		Base = 0,
		Standard = 1,
		Small = 2,
		Decorative = 3
	};


	enum class EApproximationType
	{
		NoConstraint = 0,
		AxisAlignedBox = 1 << 0,
		OrientedBox = 1 << 1,
		SweptHull = 1 << 2,
		ConvexHull = 1 << 3,
		SweptProjection = 1 << 4,
		All = 0xFFFF
	};


	/**
	 * FMeshInstanceGroupData is data shared among one or more FBaseMeshInstances.
	 * For example all instances in an ISMC can share a single FMeshInstanceGroupData.
	 */
	struct FMeshInstanceGroupData
	{
		TArray<UMaterialInterface*> MaterialSet;
		
		bool bHasConstantOverrideVertexColor = false;
		FColor OverrideVertexColor;

		bool bPreserveUVs = false;
		bool bAllowMerging = true;		// if false, cannot merge the geometry from this mesh with adjacent meshes to reduce triangle count

		bool bAllowApproximation = true;			// if false, only Copied or Simplified LODs will be used for this Part. This flag will be combined w/ the Instance-level flag.

		// ApproximationConstraint can be used to control which types of Approximation are used for LODs of this Part.
		// This is a bitmask, any unset EApproximationType bits should be ignored by the CombineMeshInstances implementation
		// Note however that 0 (all bits unset) is 'NoConstraint', implementations are intended to treat this as 'Allow All Types'
		EApproximationType ApproximationConstraint = EApproximationType::NoConstraint;
	};


	/**
	 * FBaseMeshInstance is a base-struct for the various instance types below (FStaticMeshInstance, FMeshLODSetInstance)
	 */
	struct FBaseMeshInstance
	{
		EMeshDetailLevel DetailLevel = EMeshDetailLevel::Standard;
		TArray<FTransform3d> TransformSequence;		// set of transforms on this instance. Often just a single transform.
		int32 GroupDataIndex = -1;					// index into FSourceInstanceList::InstanceGroupDatas

		bool bAllowApproximation = true;			// if false, only Copied or Simplified LODs will be used for this part Instance. Will be combined w/ the GroupData flag.

		int32 FilterLODLevel = -1;					// LOD level to filter out this mesh, value -1 disable this option. 

		// in some cases it may be desirable to have "groups" of instances which should be output as separate meshes, but
		// be jointly processed in terms of (eg) the part LODs. If any InstanceSubsetID is non-zero, then instance subsets
		// are grouped/extracted by integer ID and will be returned as separate FOutputMesh's in the FResults. 
		// No hidden-removal, triangle merging optimization, etc will be performed between Instance Subsets.
		int32 InstanceSubsetID = 0;
	};


	/**
	 * FStaticMeshInstance represents a single instance of a static mesh asset
	 */
	struct FStaticMeshInstance : FBaseMeshInstance
	{
		UStaticMesh* SourceMesh = nullptr;
		UPrimitiveComponent* SourceComponent = nullptr;
		int32 SourceInstanceIndex = 0;		// custom index, eg if SourceComponent is an InstancedStaticMeshComponent, this is the Instance index
	};

	/**
	 * FMeshLODSet represents a list of LOD meshes that are used by one or more instances (eg like a StaticMesh, but could be other sources).
	 */
	struct FMeshLODSet
	{
		TArray<const FMeshDescription*> ReferencedMeshLODs;
		FKAggregateGeom SimpleCollisionShapes;
	};

	/**
	 * FMeshLODSetInstance represents a single instance of a FMeshLODSet
	 */
	struct FMeshLODSetInstance : FBaseMeshInstance
	{
		int32 MeshLODSetIndex = -1;		// index into a list of fixed Mesh LODs shared across multiple instances (eg FSourceInstanceList.MeshLODSets)

		int32 ExternalInstanceID = 0;	// external identifier used for Instance for debugging/convenience purposes, this is not used internally by CombineMeshInstances, 
	};

	/**
	 * FSourceInstanceList provides a flattened list of mesh instances to CombineMeshInstances.
	 * Each Instance can refer to shared data in the InstanceGroupDatas list
	 * 
	 * This data structure may be replaced in future w/ something more structured
	 */
	struct FSourceInstanceList
	{
		TArray<FStaticMeshInstance> StaticMeshInstances;
		TArray<FMeshLODSetInstance> MeshLODSetInstances;

		// mesh sets shared across instances
		TArray<FMeshLODSet> MeshLODSets;

		// sets of data shared across multiple instances
		TArray<FMeshInstanceGroupData> InstanceGroupDatas;
	};


	enum class ERemoveHiddenFacesMode
	{
		None = 0,
		Fastest = 1,

		ExteriorVisibility = 5,
		OcclusionBased = 6
	};


	enum class ECoarseApproximationStrategy
	{
		Automatic = 0,
		VoxelBasedSolidApproximation = 1,
		SweptPlanarProjection = 2,
		IntersectSweptPlanarProjections = 3
	};

	enum class EVertexColorMappingMode
	{
		None = 0,
		TriangleCountMetric = 1
	};

	struct FOptions
	{
		// number of requested LODs
		int32 NumLODs = 5;

		// settings for Copied LODs, these are directly copied from Source Geometry
		int32 BaseCopiedLOD = 0;
		int32 NumCopiedLODs = 1;

		//
		// Settings for Simplifed LODs
		//
		int32 NumSimplifiedLODs = 3;
		double SimplifyBaseTolerance = 1.0;
		double SimplifyLODLevelToleranceScale = 2.0;
		// if true, UVs will be preserved in Simplified LODs. This generally will result in lower-quality geometric shape approximation.
		bool bSimplifyPreserveUVs = false;
		// if true, vertex colors will be preserved in Simplified LODs. This generally will result in lower-quality geometric shape approximation.
		bool bSimplifyPreserveVertexColors = false;
		// if true, geometrically-detected "sharp" corners (eg like the corners of a box) will be preserved with hard constraints in Simplified LODs. This can be desirable on mechanical/geometric shapes.
		bool bSimplifyPreserveCorners = true;
		double SimplifySharpEdgeAngleDeg = 44.0;
		double SimplifyMinSalientDimension = 1.0;

		//
		// settings for Approximate LODs
		//
		
		// which LOD to use as basis for Approximations. Index is interpreted relative to [CopiedLODs...SimplifiedLODs] set, ie can be set to a Simplified LOD
		int32 ApproximationSourceLOD = 0;
		double OptimizeBaseTriCost = 0.7;
		double OptimizeLODLevelTriCostScale = 1.5;
		double MaxAllowableApproximationDeviation = 5.0;

		//
		// Settings for Coarse LODs. Coarse LODs are the lowest (furthest) LODs and generally assumed to have 
		// very low triangle counts. Two strategies are supported:
		//   1) VoxelBasedSolidApproximation - does a solid approximation with topological closure (outset + inset), which will fill in any small holes/gaps
		//      or small features. CoarseApproximationDetailSize is used as the closure radius/distance. 
		//   2) SweptPlanarProjection - The mesh is effectively projected to 2D along each X/Y/Z axis, with 2D polygon boolean & topological closure using CoarseApproximationDetailSize,
		//      as well as various other cleanup, and then that 2D polygon is triangulated and swept along the part bounding-box extent. The max distance
		//      is measured from the approximation to the original mesh, and the axis-swept-mesh with the smallest max-distance is used.
		// 
		// In either case, the resulting mesh is simplified to CoarseLODMaxTriCountBase, and then further simplified for each additional coarse LOD,
		// by halving the target triangle count. 
		// 
		// The 'Automatic' strategy uses the SweptPlanarProjection if it's max approximation deviation is within K*CoarseApproximationDetailSize (currently K=2),
		// and otherwise falls back to using VoxelBasedSolidApproximation
		//
		ECoarseApproximationStrategy CoarseLODStrategy = ECoarseApproximationStrategy::Automatic;
		int32 NumCoarseLODs = 1;
		double CoarseLODBaseTolerance = 1.0;
		int32 CoarseLODMaxTriCountBase = 500;
		double CoarseApproximationDetailSize = 10.0;

		//
		// Hidden Faces removal options
		// 
		
		// overall strategy to use for removing hidden faces
		ERemoveHiddenFacesMode RemoveHiddenFacesMethod = ERemoveHiddenFacesMode::None;
		// start removing hidden faces at this LOD level 
		int32 RemoveHiddenStartLOD = 0;
		// (approximately) spacing between samples on triangle faces used for determining exterior visibility
		double RemoveHiddenSamplingDensity = 1.0;
		// treat faces as double-sided for hidden removal
		bool bDoubleSidedHiddenRemoval = false;

		// LOD level to filter out detail parts
		int32 FilterDecorativePartsLODLevel = 2;
		// Decorative part will be approximated by simple shape for this many LOD levels before Filter level
		int ApproximateDecorativePartLODs = 1;

		// opening angle used to detect/assign sharp edges
		double HardNormalAngleDeg = 15.0;

		// UVs on input geometry will be preserved up to this LOD level (inclusive). 
		// Note that this setting will severely constrain and/or fully disable many other optimizations. 
		// In particular, Coplanar merging and retriangulation cannot be applied if UVs are to be preserved. 
		// WARNING: this LOD level must be <= (NumCopiedLODs+NumSimplifiedLODs)
		int PreserveUVLODLevel = -1;

		// Attempt to merge/weld coplanar areas after hidden removal, and then further simplify those merged areas
		// Coplanar merging is never applied between areas with different Materials. 
		// TriangleGroupingIDFunc below can be used to further demarcate important internal shape boundaries.
		bool bMergeCoplanarFaces = true;
		// LOD level at which coplanar face merging is applied. 
		int32 MergeCoplanarFacesStartLOD = 1;
		// TriangleGroupingIDFunc allows client to specify external adjacency relationships between triangles via an integer ID tuple.
		// Adjacent triangles will only be considered for Coplanar Merging if they have the same FIndex3i ID.
		TFunction<UE::Geometry::FIndex3i(const UE::Geometry::FDynamicMesh3& Mesh, int32 TriangleID)> TriangleGroupingIDFunc;

		// LOD level to attempt extraction and retriangulation of detected planar polygonal areas, after removing hidden faces.
		// This occurs after coplanar face merging, and may further merge adjacent coplanar faces.
		// Can significantly reduce triangle count, but attributes on the interiors of polygonal areas will be completely discarded
		int32 PlanarPolygonRetriangulationStartLOD = -1;

		// Triangles with Materials assigned that are also in this set will not be allowed to be combined/retriangulated
		// with adjacent triangles. This can be used to preserve topology/UVs/etc for specific material areas.
		TArray<UMaterialInterface*> PreventMergingMaterialSet;

		// If enabled, attempt to retriangulate planar areas of Source LODs to remove redundant coplanar geometry. 
		// This option affects individual parts and not the combined prefab.
		bool bRetriangulateSourceLODs = true;
		// which Source LOD to start planar retriangulation at
		int32 StartRetriangulateSourceLOD = 1;

		//
		// Optional Support for hitting explicit triangle counts for different LODs. 
		// The HardLODBudgets list should be provided in LOD order, ie first for Copied LODs,
		// then for Simplified LODs, then for Approximate LODs (Voxel LOD triangle counts are configured via VoxWrapMaxTriCountBase)
		// 
		// Currently the only explicit tri-count strategy in use is Part Promotion, where a coarser approximations
		// (eg Lower Copied LOD, Simplified LOD, Approximate LOD) are "promoted" upwards into higher combined-mesh LODs
		// as necessary to achieve the triangle count.
		// 
		// Note that final LOD triangle counts cannot be guaranteed, due to the combinatorial nature of the approximation.
		// For example the coarsest part LOD is a box with 12 triangles, so NumParts*12 is a lower bound on the initial combined mesh.
		// The Part Promotion strategy is applied *before* hidden removal and further mesh processing (eg coplanar merging),
		// so the final triangle count may be substantially lower than the budget (this is why the Multiplier is used below)
		//

		// list of fixed-triangle-count LOD budgets, in LOD order (ie LOD0, LOD1, LOD2, ...). If a triangle budgets are not specified for
		// a LOD, either by placing -1 in the array or truncating the array, that LOD will be left as-is. 
		TArray<int32> HardLODBudgets;
		// enable/disable the Part Promotion LOD strategy (described above)
		bool bEnableBudgetStrategy_PartLODPromotion = false;
		// Multiplier on LOD Budgets for PartLODPromotion strategy. This can be used to compensate for hidden-geometry removal and other optimizations done after the strategy is applied.
		double PartLODPromotionBudgetMultiplier = 2.0;



		//
		// Final processing options applied to each LOD after generation
		//

		// ensure that all triangles have some UV values set. Various geometry processing steps may discard UVs, this flag will
		// cause missing UVs to be recomputed (currently using a box projection)
		bool bAutoGenerateMissingUVs = false;

		// generate tangents on the output meshes - requires that UVs be available
		bool bAutoGenerateTangents = false;


		//
		// Debug/utility options
		// 

		// Color mapping modes for vertex colors, primarily used for debugging
		EVertexColorMappingMode VertexColorMappingMode = EVertexColorMappingMode::None;
	};


	struct FOutputMesh
	{
		TArray<UE::Geometry::FDynamicMesh3> MeshLODs;
		TArray<UMaterialInterface*> MaterialSet;

		FKAggregateGeom SimpleCollisionShapes;

		// All part instances accumulated into the MeshLODs will have had this InstanceSubsetID in their input FBaseMeshInstance's
		int32 InstanceSubsetID = 0;
	};


	struct FResults
	{
		//EResultCode ResultCode = EResultCode::UnknownError;

		TArray<FOutputMesh> CombinedMeshes;
	};




	virtual FOptions ConstructDefaultOptions()
	{
		check(false);		// not implemented in base class
		return FOptions();
	}


	virtual void CombineMeshInstances(
		const FSourceInstanceList& MeshInstances,
		const FOptions& Options, 
		FResults& ResultsOut) 
	{
		check(false);		// not implemented in base class
	}



	// Modular feature name to register for retrieval during runtime
	static const FName GetModularFeatureName()
	{
		return TEXT("GeometryProcessing_CombineMeshInstances");
	}

};
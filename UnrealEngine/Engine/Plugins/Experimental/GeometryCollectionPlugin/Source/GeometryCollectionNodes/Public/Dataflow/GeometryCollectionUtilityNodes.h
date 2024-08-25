// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowEngine.h"
#include "Dataflow/DataflowSelection.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollectionConvexUtility.h"
#include "CompGeom/ConvexDecomposition3.h"

#include "FractureEngineConvex.h"

#include "GeometryCollectionUtilityNodes.generated.h"


UENUM(BlueprintType)
enum class EConvexOverlapRemovalMethodEnum : uint8
{
	Dataflow_EConvexOverlapRemovalMethod_None UMETA(DisplayName = "None"),
	Dataflow_EConvexOverlapRemovalMethod_All UMETA(DisplayName = "All"),
	Dataflow_EConvexOverlapRemovalMethod_OnlyClusters UMETA(DisplayName = "Only Clusters"),
	Dataflow_EConvexOverlapRemovalMethod_OnlyClustersVsClusters UMETA(DisplayName = "Only Clusters vs Clusters"),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

USTRUCT()
struct FDataflowConvexDecompositionSettings
{
	GENERATED_USTRUCT_BODY()

public:
	// If greater than zero, the minimum geometry size (cube root of volume) to consider for convex decomposition
	UPROPERTY(EditAnywhere, Category = Filter, meta = (ClampMin = 0.0))
	float MinSizeToDecompose = 0.f;

	// If the geo volume / hull volume ratio is greater than this, do not consider convex decomposition
	UPROPERTY(EditAnywhere, Category = Filter, meta = (ClampMin = 0.0, ClampMax = 1.0))
	float MaxGeoToHullVolumeRatioToDecompose = 1.f;

	// Stop splitting when hulls have error less than this (expressed in cm; will be cubed for volumetric error).
	// Note: ErrorTolerance must be > 0 or MaxHullsPerGeometry > 1, or decomposition will not be performed.
	UPROPERTY(EditAnywhere, Category = Decomposition, meta = (ClampMin = 0.0))
	float ErrorTolerance = 0.f;

	// If greater than zero, maximum number of convex hulls to use in each convex decomposition.
	//Note: ErrorTolerance must be > 0 or MaxHullsPerGeometry > 1, or decomposition will not be performed.
	UPROPERTY(EditAnywhere, Category = Decomposition, meta = (ClampMin = -1))
	int32 MaxHullsPerGeometry = -1;

	// Optionally specify a minimum thickness (in cm) for convex parts; parts below this thickness will always be merged away. Overrides NumOutputHulls and ErrorTolerance when needed.
	UPROPERTY(EditAnywhere, Category = Decomposition, meta = (ClampMin = 0.0))
	float MinThicknessTolerance = 0.f;

	// Control the search effort spent per convex decomposition: larger values will require more computation but may find better convex decompositions
	UPROPERTY(EditAnywhere, Category = Decomposition, meta = (ClampMin = 0))
	int32 NumAdditionalSplits = 4;
};


//~ TODO: Ideally this would be generated from the above FDataflowConvexDecompositionSettings struct
// Provide settings for running convex decomposition of geometry
USTRUCT(meta = (DataflowGeometryCollection))
struct FMakeDataflowConvexDecompositionSettingsNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeDataflowConvexDecompositionSettingsNode, "MakeConvexDecompositionSettings", "GeometryCollection|Utilities", "")

public:
	// If greater than zero, the minimum geometry size (cube root of volume) to consider for convex decomposition
	UPROPERTY(EditAnywhere, Category = Filter, meta = (ClampMin = 0.0, DataflowInput))
	float MinSizeToDecompose = 0.f;
	
	// If the geo volume / hull volume ratio is greater than this, do not consider convex decomposition
	UPROPERTY(EditAnywhere, Category = Filter, meta = (ClampMin = 0.0, ClampMax = 1.0, DataflowInput))
	float MaxGeoToHullVolumeRatioToDecompose = 1.f;

	// Stop splitting when hulls have error less than this (expressed in cm; will be cubed for volumetric error).
	// Note: ErrorTolerance must be > 0 or MaxHullsPerGeometry > 1, or decomposition will not be performed.
	UPROPERTY(EditAnywhere, Category = Decomposition, meta = (ClampMin = 0.0, DataflowInput))
	float ErrorTolerance = 0.f;

	// If greater than zero, maximum number of convex hulls to use in each convex decomposition.
	//Note: ErrorTolerance must be > 0 or MaxHullsPerGeometry > 1, or decomposition will not be performed.
	UPROPERTY(EditAnywhere, Category = Decomposition, meta = (ClampMin = -1, DataflowInput))
	int32 MaxHullsPerGeometry = -1;

	// Optionally specify a minimum thickness (in cm) for convex parts; parts below this thickness will always be merged away. Overrides NumOutputHulls and ErrorTolerance when needed.
	UPROPERTY(EditAnywhere, Category = Decomposition, meta = (ClampMin = 0.0, DataflowInput))
	float MinThicknessTolerance = 0.f;

	// Control the search effort spent per convex decomposition: larger values will require more computation but may find better convex decompositions
	UPROPERTY(EditAnywhere, Category = Decomposition, meta = (ClampMin = 0, DataflowInput))
	int32 NumAdditionalSplits = 4;

	UPROPERTY(meta = (DataflowOutput))
	FDataflowConvexDecompositionSettings DecompositionSettings;

	FMakeDataflowConvexDecompositionSettingsNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

USTRUCT(meta = (DataflowGeometryCollection))
struct FCreateLeafConvexHullsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCreateLeafConvexHullsDataflowNode, "CreateLeafConvexHulls", "GeometryCollection|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput))
	FManagedArrayCollection Collection;

	/** Optional transform selection to compute leaf hulls on -- if not provided, all leaf hulls will be computed. */
	UPROPERTY(meta = (DataflowInput))
	FDataflowTransformSelection OptionalSelectionFilter;

	/** How convex hulls are generated -- computed from geometry, imported from external collision shapes, or an intersection of both options. */
	UPROPERTY(EditAnywhere, Category = Options)
	EGenerateConvexMethod GenerateMethod = EGenerateConvexMethod::ExternalCollision;

	/** If GenerateMethod is Intersect, only actually intersect when the volume of the Computed Hull is less than this fraction of the volume of the External Hull(s). */
	UPROPERTY(EditAnywhere, Category = IntersectionFilters, meta = (ClampMin = 0.0, ClampMax = 1.0, EditCondition = "GenerateMethod == EGenerateConvexMethod::IntersectExternalWithComputed"))
	float IntersectIfComputedIsSmallerByFactor = 1.0f;

	/** If GenerateMethod is Intersect, only actually intersect if the volume of the External Hull(s) exceed this threshold. */
	UPROPERTY(EditAnywhere, Category = IntersectionFilters, meta = (ClampMin = 0.0, EditCondition = "GenerateMethod == EGenerateConvexMethod::IntersectExternalWithComputed"))
	float MinExternalVolumeToIntersect = 0.0f;

	/** Whether to compute the intersection before computing convex hulls. Typically should be enabled. */
	UPROPERTY(EditAnywhere, Category = Convex, meta = (EditCondition = "GenerateMethod == EGenerateConvexMethod::IntersectExternalWithComputed"))
	bool bComputeIntersectionsBeforeHull = true;

	/** Computed convex hulls are simplified to keep points spaced at least this far apart (except where needed to keep the hull from collapsing to zero volume). */
	UPROPERTY(EditAnywhere, Category = Convex, meta = (DataflowInput, ClampMin = 0.f, EditCondition = "GenerateMethod != EGenerateConvexMethod::ExternalCollision"))
	float SimplificationDistanceThreshold = 10.f;

	UPROPERTY(EditAnywhere, Category = Convex, meta = (DataflowInput))
	FDataflowConvexDecompositionSettings ConvexDecompositionSettings;

	FCreateLeafConvexHullsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

USTRUCT(meta = (DataflowGeometryCollection))
struct FSimplifyConvexHullsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSimplifyConvexHullsDataflowNode, "SimplifyConvexHulls", "GeometryCollection|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput))
	FManagedArrayCollection Collection;

	/** Optional transform selection to compute leaf hulls on -- if not provided, all leaf hulls will be computed. */
	UPROPERTY(meta = (DataflowInput))
	FDataflowTransformSelection OptionalSelectionFilter;

	UPROPERTY(EditAnywhere, Category = "Convex")
	EConvexHullSimplifyMethod SimplifyMethod = EConvexHullSimplifyMethod::MeshQSlim;

	/** Simplified hull should preserve angles larger than this (in degrees).  Used by the AngleTolerance simplification method. */
	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput, ClampMin = 0.f, EditCondition = "SimplifyMethod == EConvexHullSimplifyMethod::AngleTolerance"))
	float SimplificationAngleThreshold = 10.f;

	/** Simplified hull should stay within this distance of the initial convex hull. Used by the MeshQSlim simplification method. */
	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput, ClampMin = 0.f, EditCondition = "SimplifyMethod == EConvexHullSimplifyMethod::MeshQSlim"))
	float SimplificationDistanceThreshold = 10.f;

	/** The minimum number of faces to use for the convex hull. For MeshQSlim simplification, this is a triangle count, which may be further reduced on conversion back to a convex hull. */
	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DisplayName = "Min Target Face Count", DataflowInput, ClampMin = 4))
	int32 MinTargetTriangleCount = 12;

	/** Whether to restrict the simplified hulls to only use vertices from the original hulls. */
	UPROPERTY(EditAnywhere, Category = "Convex")
	bool bUseExistingVertices = false;

	FSimplifyConvexHullsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 *
 * Generates convex hull representation for the bones for simulation
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FCreateNonOverlappingConvexHullsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCreateNonOverlappingConvexHullsDataflowNode, "CreateNonOverlappingConvexHulls", "GeometryCollection|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput))
	FManagedArrayCollection Collection;

	/** Fraction (of geometry volume) by which a cluster's convex hull volume can exceed the actual geometry volume before instead using the hulls of the children.  0 means the convex volume cannot exceed the geometry volume; 1 means the convex volume is allowed to be 100% larger (2x) the geometry volume. */
	UPROPERTY(EditAnywhere, Category = Convex, meta = (DataflowInput, DisplayName = "Allow Larger Hull Fraction", ClampMin = 0.f))
	float CanExceedFraction = .5f;

	/** Computed convex hulls are simplified to keep points spaced at least this far apart (except where needed to keep the hull from collapsing to zero volume) */
	UPROPERTY(EditAnywhere, Category = Convex, meta = (DataflowInput, ClampMin = 0.f))
	float SimplificationDistanceThreshold = 10.f;

	/** Whether and in what cases to automatically cut away overlapping parts of the convex hulls, to avoid the simulation 'popping' to fix the overlaps */
	UPROPERTY(EditAnywhere, Category = AutomaticOverlapRemoval, meta = (DisplayName = "Remove Overlaps"))
	EConvexOverlapRemovalMethodEnum OverlapRemovalMethod = EConvexOverlapRemovalMethodEnum::Dataflow_EConvexOverlapRemovalMethod_All;

	/** Overlap removal will be computed as if convex hulls were this percentage smaller (in range 0-100) */
	UPROPERTY(EditAnywhere, Category = AutomaticOverlapRemoval, meta = (DataflowInput, ClampMin = 0.f, ClampMax = 99.9f))
	float OverlapRemovalShrinkPercent = 0.f;

	/** Fraction of the convex hulls for a cluster that we can remove before using the hulls of the children */
	UPROPERTY(EditAnywhere, Category = AutomaticOverlapRemoval, meta = (DataflowInput, DisplayName = "Max Removal Fraction", ClampMin = 0.01f, ClampMax = 1.f))
	float CanRemoveFraction = 0.3f;

	FCreateNonOverlappingConvexHullsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

//~ Simple wrapper class to make the sphere covering data possible to pass via dataflow
// A set of spheres generated to represent empty space when creating a minimal set of convex hulls, e.g. in one of the Generate Cluster Convex Hulls nodes
USTRUCT()
struct FDataflowSphereCovering
{
	GENERATED_USTRUCT_BODY()

public:
	UE::Geometry::FSphereCovering Spheres;
};

//~ Dataflow-specific copy of the negative space sampling method enum in ConvexDecomposition3.h,
//~ so that it can be exposed as a UENUM
// Method to distribute sampling spheres
UENUM()
enum class ENegativeSpaceSampleMethodDataflowEnum : uint8
{
	// Place sample spheres in a uniform grid pattern
	Uniform,
	// Use voxel-based subtraction and offsetting methods to specifically target concavities
	VoxelSearch
};

/**
 *
 * Generates cluster convex hulls for leafs hulls
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGenerateClusterConvexHullsFromLeafHullsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGenerateClusterConvexHullsFromLeafHullsDataflowNode, "GenerateClusterConvexHullsFromLeafHulls", "GeometryCollection|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput))
	FManagedArrayCollection Collection;

	// A representation of the negative space protected by the 'protect negative space' option. If negative space is not protected, this will contain zero spheres.
	UPROPERTY(meta = (DataflowOutput))
	FDataflowSphereCovering SphereCovering;

	/** Maximum number of convex to generate for a specific cluster. Will be ignored if error tolerance is used instead */
	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput, EditCondition = "ErrorTolerance == 0"))
	int32 ConvexCount = 2;
	
	/** 
	* Error tolerance to use to decide to merge leaf convex together. 
	* This is in centimeters and represents the side of a cube, the volume of which will be used as threshold
	* to know if the volume of the generated convex is too large compared to the sum of the volume of the leaf convex
	*/
	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput, UIMin = "0", UIMax = "100.", Units = cm))
	double ErrorTolerance = 0.0;
	
	/** Whether to prefer available External (imported) collision shapes instead of the computed convex hulls on the Collection */
	UPROPERTY(EditAnywhere, Category = "Convex")
	bool bPreferExternalCollisionShapes = true;

	/** Method to determine which convex hull pairs can potentially be merged */
	UPROPERTY(EditAnywhere, Category = "Convex")
	EAllowConvexMergeMethod AllowMerges = EAllowConvexMergeMethod::ByProximity;

	/** Optional transform selection to compute cluster hulls on -- if not provided, all cluster hulls will be computed. */
	UPROPERTY(meta = (DataflowInput))
	FDataflowTransformSelection OptionalSelectionFilter;

	/** Whether to use a sphere cover to define negative space that should not be covered by convex hulls */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput))
	bool bProtectNegativeSpace = false;

	/** Method to use to find and sample negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (EditCondition = "bProtectNegativeSpace", EditConditionHides))
	ENegativeSpaceSampleMethodDataflowEnum SampleMethod = ENegativeSpaceSampleMethodDataflowEnum::Uniform;

	/** Whether to require that all candidate locations identified by Voxel Search are covered by negative space samples, up to the specified Min Sample Spacing. Only applies to Voxel Search. */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (EditCondition = "bProtectNegativeSpace && SampleMethod == ENegativeSpaceSampleMethodDataflowEnum::VoxelSearch", EditConditionHides))
	bool bRequireSearchSampleCoverage = false;

	/** When performing Voxel Search, only look for negative space that is connected out to the convex hull. This removes inaccessable internal negative space from consideration. Only applies to Voxel Search. */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (EditCondition = "bProtectNegativeSpace && SampleMethod == ENegativeSpaceSampleMethodDataflowEnum::VoxelSearch", EditConditionHides))
	bool bOnlyConnectedToHull = false;

	/** Approximate number of spheres to consider when covering negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = 1, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	int32 TargetNumSamples = 50;

	/** Minimum desired spacing between spheres; if > 0, will attempt not to place sphere centers closer than this */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = 0, Units = cm, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	double MinSampleSpacing = 1.0;

	/** Amount of space to leave between convex hulls and protected negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = .01, UIMin = .1, Units = cm, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	double NegativeSpaceTolerance = 2.0;

	/** Spheres smaller than this are not included in the negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = 0, Units = cm, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	double MinRadius = 10.0;

	FGenerateClusterConvexHullsFromLeafHullsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

/**
 *
 * Generates cluster convex hulls for children hulls
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGenerateClusterConvexHullsFromChildrenHullsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGenerateClusterConvexHullsFromChildrenHullsDataflowNode, "GenerateClusterConvexHullsFromChildrenHulls", "GeometryCollection|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput))
	FManagedArrayCollection Collection;
	
	// A representation of the negative space protected by the 'protect negative space' option. If negative space is not protected, this will contain zero spheres.
	UPROPERTY(meta = (DataflowOutput))
	FDataflowSphereCovering SphereCovering;

	/** Maximum number of convex to generate for a specific cluster. Will be ignored if error tolerance is used instead */
	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput, EditCondition = "ErrorTolerance == 0"))
	int32 ConvexCount = 2;

	/**
	* Error tolerance to use to decide to merge leaf convex together.
	* This is in centimeters and represents the side of a cube, the volume of which will be used as threshold
	* to know if the volume of the generated convex is too large compared to the sum of the volume of the leaf convex
	*/
	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput, UIMin = "0", UIMax = "100.", Units = cm))
	double ErrorTolerance = 0.0;
	
	/** Whether to prefer available External (imported) collision shapes instead of the computed convex hulls on the Collection */
	UPROPERTY(EditAnywhere, Category = "Convex")
	bool bPreferExternalCollisionShapes = true;

	/** Optional transform selection to compute cluster hulls on -- if not provided, all cluster hulls will be computed. */
	UPROPERTY(meta = (DataflowInput))
	FDataflowTransformSelection OptionalSelectionFilter;

	/** Whether to use a sphere cover to define negative space that should not be covered by convex hulls */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput))
	bool bProtectNegativeSpace = false;

	/** Method to use to find and sample negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (EditCondition = "bProtectNegativeSpace", EditConditionHides))
	ENegativeSpaceSampleMethodDataflowEnum SampleMethod = ENegativeSpaceSampleMethodDataflowEnum::Uniform;

	/** Whether to require that all candidate locations identified by Voxel Search are covered by negative space samples, up to the specified Min Sample Spacing. Only applies to Voxel Search. */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (EditCondition = "bProtectNegativeSpace && SampleMethod == ENegativeSpaceSampleMethodDataflowEnum::VoxelSearch", EditConditionHides))
	bool bRequireSearchSampleCoverage = false;

	/** When performing Voxel Search, only look for negative space that is connected out to the convex hull. This removes inaccessable internal negative space from consideration. Only applies to Voxel Search. */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (EditCondition = "bProtectNegativeSpace && SampleMethod == ENegativeSpaceSampleMethodDataflowEnum::VoxelSearch", EditConditionHides))
	bool bOnlyConnectedToHull = false;

	/** Approximate number of spheres to consider when covering negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = 1, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	int32 TargetNumSamples = 50;

	/** Minimum desired spacing between spheres; if > 0, will attempt not to place sphere centers closer than this */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = 0, Units = cm, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	double MinSampleSpacing = 1.0;

	/** Amount of space to leave between convex hulls and protected negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = .01, UIMin = .1, Units = cm, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	double NegativeSpaceTolerance = 2.0;

	/** Spheres smaller than this are not included in the negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = 0, Units = cm, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	double MinRadius = 10.0;

	FGenerateClusterConvexHullsFromChildrenHullsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/** Clear convex hulls from the selected transforms */
USTRUCT(meta = (DataflowGeometryCollection))
struct FClearConvexHullsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FClearConvexHullsDataflowNode, "ClearConvexHulls", "GeometryCollection|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** Convex hulls will be cleared from these transforms */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	FDataflowTransformSelection TransformSelection;

	FClearConvexHullsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&TransformSelection);

		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


/** Merge convex hulls on transforms with multiple hulls */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMergeConvexHullsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMergeConvexHullsDataflowNode, "MergeConvexHulls", "GeometryCollection|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput))
	FManagedArrayCollection Collection;

	// A representation of the negative space protected by the 'protect negative space' option. If negative space is not protected, this will contain zero spheres.
	UPROPERTY(meta = (DataflowOutput))
	FDataflowSphereCovering SphereCovering;

	/** Maximum number of convex to generate per transform. Ignored if < 0. */
	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput))
	int32 MaxConvexCount = -1;

	/**
	* Error tolerance to use to decide to merge leaf convex together.
	* This is in centimeters and represents the side of a cube, the volume of which will be used as threshold
	* to know if the volume of the generated convex is too large compared to the sum of the volume of the leaf convex
	*/
	UPROPERTY(EditAnywhere, Category = "Convex", meta = (DataflowInput, ClampMin = "0", UIMax = "100.", Units = cm))
	double ErrorTolerance = 0.0;

	/** Optional transform selection to compute cluster hulls on -- if not provided, all cluster hulls will be computed. */
	UPROPERTY(meta = (DataflowInput))
	FDataflowTransformSelection OptionalSelectionFilter;

	/** Whether to use a sphere cover to define negative space that should not be covered by convex hulls */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput))
	bool bProtectNegativeSpace = false;

	/** Whether to compute separate negative space for each bone. Otherwise, a single negative space will be computed once and re-used for all bones. */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (EditCondition = "bProtectNegativeSpace", EditConditionHides))
	bool bComputeNegativeSpacePerBone = false;

	/** Method to use to find and sample negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (EditCondition = "bProtectNegativeSpace", EditConditionHides))
	ENegativeSpaceSampleMethodDataflowEnum SampleMethod = ENegativeSpaceSampleMethodDataflowEnum::Uniform;

	/** Whether to require that all candidate locations identified by Voxel Search are covered by negative space samples, up to the specified Min Sample Spacing. Only applies to Voxel Search. */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (EditCondition = "bProtectNegativeSpace && SampleMethod == ENegativeSpaceSampleMethodDataflowEnum::VoxelSearch", EditConditionHides))
	bool bRequireSearchSampleCoverage = false;

	/** When performing Voxel Search, only look for negative space that is connected out to the convex hull. This removes inaccessable internal negative space from consideration. Only applies to Voxel Search. */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (EditCondition = "bProtectNegativeSpace && SampleMethod == ENegativeSpaceSampleMethodDataflowEnum::VoxelSearch", EditConditionHides))
	bool bOnlyConnectedToHull = false;

	/** Approximate number of spheres to consider when covering negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = 1, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	int32 TargetNumSamples = 50;

	/** Minimum desired spacing between spheres; if > 0, will attempt not to place sphere centers closer than this */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = 0, Units = cm, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	double MinSampleSpacing = 1.0;

	/** Amount of space to leave between convex hulls and protected negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = .01, UIMin = .1, Units = cm, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	double NegativeSpaceTolerance = 2.0;

	/** Spheres smaller than this are not included in the negative space */
	UPROPERTY(EditAnywhere, Category = NegativeSpace, meta = (DataflowInput, ClampMin = 0, Units = cm, EditCondition = "bProtectNegativeSpace", EditConditionHides))
	double MinRadius = 10.0;

	FMergeConvexHullsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Update the Volume and Size attributes on the target Collection (and add them if they were not present)
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FUpdateVolumeAttributesDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FUpdateVolumeAttributesDataflowNode, "UpdateVolumeAttributes", "GeometryCollection|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput))
	FManagedArrayCollection Collection;

	FUpdateVolumeAttributesDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Get the sum of volumes of the convex hulls on the selected nodes
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetConvexHullVolumeDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetConvexHullVolumeDataflowNode, "GetConvexHullVolume", "GeometryCollection|Utilities", "")

public:
	UPROPERTY(meta = (DataflowInput))
	FManagedArrayCollection Collection;

	/** The transforms to consider */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	FDataflowTransformSelection TransformSelection;

	/** Sum of convex hull volumes */
	UPROPERTY(meta = (DataflowOutput));
	float Volume = 0.f;

	/** For any cluster transform that has no convex hulls, whether to fall back to the convex hulls of the cluster's children. Otherwise, the cluster will not add to the total volume sum. */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bSumChildrenForClustersWithoutHulls = true;

	/** Whether to take the volume of the union of selected hulls, rather than the sum of each hull volume separately. This is more expensive but more accurate when hulls overlap. */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bVolumeOfUnion = false;

	FGetConvexHullVolumeDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


namespace Dataflow
{
	void GeometryCollectionUtilityNodes();
}


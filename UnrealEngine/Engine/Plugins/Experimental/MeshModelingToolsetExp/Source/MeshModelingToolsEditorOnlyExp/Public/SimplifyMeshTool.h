// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "MeshOpPreviewHelpers.h"
#include "Properties/RemeshProperties.h"
#include "CleaningOps/SimplifyMeshOp.h"		// required in header for enum types
#include "SimplifyMeshTool.generated.h"

class UMeshStatisticsProperties;
class UMeshElementsVisualizer;
PREDECLARE_GEOMETRY(class FDynamicMesh3);
PREDECLARE_GEOMETRY(typedef TMeshAABBTree3<FDynamicMesh3> FDynamicMeshAABBTree3);

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USimplifyMeshToolBuilder : public USingleSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};



/**
 * Standard properties of the Simplify operation
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USimplifyMeshToolProperties : public UMeshConstraintProperties
{
	GENERATED_BODY()
public:
	USimplifyMeshToolProperties();

	/** Simplification Scheme  */
	UPROPERTY(EditAnywhere, Category = Options)
	ESimplifyType SimplifierType;

	/** Simplification Target Type  */
	UPROPERTY(EditAnywhere, Category = Options, meta = (EditCondition = "SimplifierType != ESimplifyType::MinimalPlanar && SimplifierType != ESimplifyType::MinimalPolygroup"))
	ESimplifyTargetType TargetMode;

	/** Target percentage of original triangle count */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "0", UIMax = "100",
				EditCondition = "SimplifierType != ESimplifyType::MinimalPolygroup && SimplifierType != ESimplifyType::MinimalPlanar && TargetMode == ESimplifyTargetType::Percentage"))
	int TargetPercentage;

	/** Target edge length */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "3.0", UIMax = "10.0", ClampMin = "0.001", ClampMax = "1000.0",
		EditCondition = "TargetMode == ESimplifyTargetType::EdgeLength && SimplifierType != ESimplifyType::UEStandard && SimplifierType != ESimplifyType::MinimalPlanar && SimplifierType != ESimplifyType::MinimalPolygroup"))
	float TargetEdgeLength;

	/** Target triangle count */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "4", UIMax = "10000", ClampMin = "1", ClampMax = "9999999999",
				EditCondition = "TargetMode == ESimplifyTargetType::TriangleCount && SimplifierType != ESimplifyType::MinimalPlanar && SimplifierType != ESimplifyType::MinimalPolygroup"))
	int TargetTriangleCount;

	/** Target vertex count */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "4", UIMax = "10000", ClampMin = "1", ClampMax = "9999999999",
				EditCondition = "TargetMode == ESimplifyTargetType::VertexCount && SimplifierType != ESimplifyType::MinimalPlanar"))
	int TargetVertexCount;

	/** Angle threshold in degrees used for testing if two triangles should be considered coplanar, or two lines collinear */
	UPROPERTY()
	float MinimalAngleThreshold = 0.01;

	//~ Note PolyEdgeAngleTolerance is very similar to MinimalAngleThreshold, but not redundant b/c the useful ranges are very different (MinimalAngleThreshold should generally be kept very small)
	/** Threshold angle change (in degrees) along a polygroup edge, above which a vertex must be added */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "0.001", ClampMin = "0.0", UIMax = "90.0", ClampMax = "180.0", EditCondition = "SimplifierType == ESimplifyType::MinimalPolygroup"))
	float PolyEdgeAngleTolerance = .1;

	/** If true, UVs and Normals are discarded  */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bDiscardAttributes;

	/** If true, then simplification will consider geometric deviation with the input mesh  */
	UPROPERTY(EditAnywhere, Category = Options, meta = (EditCondition = "SimplifierType != ESimplifyType::MinimalPolygroup"))
	bool bGeometricConstraint;

	/** Geometric deviation tolerance used when bGeometricConstraint is enabled, to limit the geometric deviation between the simplified and original meshes */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "0.0", UIMax = "10.0", ClampMin = "0.0", ClampMax = "10000000.0",
				EditCondition = "bGeometricConstraint && SimplifierType != ESimplifyType::UEStandard && SimplifierType != ESimplifyType::MinimalPolygroup"))
	float GeometricTolerance;

	/** Display colors corresponding to the mesh's polygon groups */
	UPROPERTY(EditAnywhere, Category = Display)
	bool bShowGroupColors = false;

	/** Enable projection back to input mesh */
	UPROPERTY(EditAnywhere, Category = Options, AdvancedDisplay)
	bool bReproject;
};




/**
 * Simple Mesh Simplifying Tool
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USimplifyMeshTool : public USingleSelectionMeshEditingTool, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

private:
	UPROPERTY()
	TObjectPtr<USimplifyMeshToolProperties> SimplifyProperties;

	UPROPERTY()
	TObjectPtr<UMeshStatisticsProperties> MeshStatisticsProperties;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> Preview;

	UPROPERTY()
	TObjectPtr<UMeshElementsVisualizer> MeshElementsDisplay;

	TSharedPtr<FMeshDescription, ESPMode::ThreadSafe> OriginalMeshDescription;
	// Dynamic Mesh versions precomputed in Setup (rather than recomputed for every simplify op)
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	TSharedPtr<UE::Geometry::FDynamicMeshAABBTree3, ESPMode::ThreadSafe> OriginalMeshSpatial;

	void UpdateVisualization();
};

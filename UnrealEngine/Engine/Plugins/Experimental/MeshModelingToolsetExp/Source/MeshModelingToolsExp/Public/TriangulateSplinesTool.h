// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolManager.h"
#include "MeshOpPreviewHelpers.h"
#include "InteractiveToolQueryInterfaces.h"
#include "PropertySets/CreateMeshObjectTypeProperties.h"

#include "CurveOps/TriangulateCurvesOp.h"


#include "TriangulateSplinesTool.generated.h"

class USplineComponent;
class UWorld;


/**
 * Parameters for controlling the spline triangulation
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UTriangulateSplinesToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	// How far to allow the triangulation boundary can deviate from the spline curve before we add more vertices
	UPROPERTY(EditAnywhere, Category = Spline, meta = (ClampMin = 0.001))
	double ErrorTolerance = 1.0;
	
	// Whether and how to flatten the curves. If curves are flattened, they can also be offset and combined
	UPROPERTY(EditAnywhere, Category = Spline)
	EFlattenCurveMethod FlattenMethod = EFlattenCurveMethod::DoNotFlatten;

	// Whether or how to combine the curves
	UPROPERTY(EditAnywhere, Category = Spline, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten", EditConditionHides))
	ECombineCurvesMethod CombineMethod = ECombineCurvesMethod::LeaveSeparate;

	// If > 0, Extrude the triangulation by this amount
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.0))
	double Thickness = 0.0;

	// Whether to flip the facing direction of the generated mesh
	UPROPERTY(EditAnywhere, Category = Mesh)
	bool bFlipResult = false;

	// How to handle open curves: Either offset them, or treat them as closed curves
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten", EditConditionHides))
	EOffsetOpenCurvesMethod OpenCurves = EOffsetOpenCurvesMethod::Offset;

	// How much offset to apply to curves
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten", EditConditionHides))
	double CurveOffset = 0.0;

	// Whether and how to apply offset to closed curves
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten && CurveOffset != 0", EditConditionHides))
	EOffsetClosedCurvesMethod OffsetClosedCurves = EOffsetClosedCurvesMethod::OffsetOuterSide;

	// The shape of the ends of offset curves
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten && OpenCurves != EOffsetOpenCurvesMethod::TreatAsClosed && CurveOffset != 0", EditConditionHides))
	EOpenCurveEndShapes EndShapes = EOpenCurveEndShapes::Square;

	// The shape of joins between segments of an offset curve
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten && CurveOffset != 0", EditConditionHides))
	EOffsetJoinMethod JoinMethod = EOffsetJoinMethod::Square;

	// How far a miter join can extend before it is replaced by a square join
	UPROPERTY(EditAnywhere, Category = Offset, meta = (ClampMin = 0.0, EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten && CurveOffset != 0 && JoinMethod == EOffsetJoinMethod::Miter", EditConditionHides))
	double MiterLimit = 1.0;

};

/**
 * Tool to create a mesh from a set of selected Spline Components
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UTriangulateSplinesTool : public UInteractiveTool, public IInteractiveToolEditorGizmoAPI, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:

	UTriangulateSplinesTool() = default;

	// IInteractiveToolEditorGizmoAPI -- allow editor gizmo so users can live-edit the splines

	virtual bool GetAllowStandardEditorGizmos() override
	{
		return true;
	}

	//
	// InteractiveTool API - generally does not need to be modified by subclasses
	//

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	void SetSplineActors(TArray<TWeakObjectPtr<AActor>> InSplineActors)
	{
		ActorsWithSplines = MoveTemp(InSplineActors);
	}

	virtual void SetWorld(UWorld* World);
	virtual UWorld* GetTargetWorld();

	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator();

private:

	virtual void GenerateAsset(const FDynamicMeshOpResult& OpResult);

	UPROPERTY()
	TObjectPtr<UTriangulateSplinesToolProperties> TriangulateProperties;

	UPROPERTY()
	TObjectPtr<UCreateMeshObjectTypeProperties> OutputTypeProperties;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> Preview;

	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	// Note: We track actors instead of the USplineComponents here because the USplineComponents objects are often deleted / swapped for identical but new objects
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ActorsWithSplines;

private:

	// Helper to track the splines we are triangulating, so we can re-triangulate when they are moved or changed
	void PollSplineUpdates();
	// Track the spline 'Version' integer, which is incremented when splines are changed
	TArray<uint32> LastSplineVersions;
	// Track the spline component's transform (to world space)
	TArray<FTransform> LastSplineTransforms;
};



/**
 * Base Tool Builder for tools that operate on a selection of Spline Components
 */
UCLASS(Transient)
class MESHMODELINGTOOLSEXP_API UTriangulateSplinesToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	/** @return true if spline component sources can be found in the active selection */
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	/** @return new Tool instance initialized with selected spline source(s) */
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	/** Called by BuildTool to configure the Tool with the input spline source(s) based on the SceneState */
	virtual void InitializeNewTool(UTriangulateSplinesTool* Tool, const FToolBuilderState& SceneState) const;
};




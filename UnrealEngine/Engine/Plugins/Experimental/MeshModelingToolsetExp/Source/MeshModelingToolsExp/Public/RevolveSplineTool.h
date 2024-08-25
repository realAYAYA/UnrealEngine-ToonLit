// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Components/SplineComponent.h" // (to use with TWeakObjectPtr)
#include "Engine/World.h" // (to use with TWeakObjectPtr)
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h"
#include "ModelingOperators.h" // IDynamicMeshOperatorFactory
#include "Properties/RevolveProperties.h"

#include "RevolveSplineTool.generated.h"

class AActor;
struct FDynamicMeshOpResult;
class UConstructionPlaneMechanic;
class UCreateMeshObjectTypeProperties;
class UMeshOpPreviewWithBackgroundCompute;
class UNewMeshMaterialProperties;
class URevolveSplineTool;
class USplineComponent;
class UWorld;

//~ TODO: Might want to have some shared enum for sampling splines in a util folder,
//~ but hesitant to prescribe it until other tools want it.
UENUM()
enum class ERevolveSplineSampleMode : uint8
{
	// Place points only at the spline control points
	ControlPointsOnly,

	// Place points along the spline such that the resulting polyline has no more than 
	// some maximum deviation from the curve.
	PolyLineMaxError,

	// Place points along spline that are an equal spacing apart, and so that the spacing
	// is as close as possible to some max spacing.
	UniformSpacingAlongCurve,
};



UCLASS()
class MESHMODELINGTOOLSEXP_API URevolveSplineToolProperties : public URevolveProperties
{
	GENERATED_BODY()

public:

	/** Determines how points to revolve are actually picked from the spline. */
	UPROPERTY(EditAnywhere, Category = Spline)
	ERevolveSplineSampleMode SampleMode = ERevolveSplineSampleMode::ControlPointsOnly;

	/** How far to allow the triangulation boundary can deviate from the spline curve before we add more vertices. */
	UPROPERTY(EditAnywhere, Category = Spline, meta = (ClampMin = 0.001, EditConditionHides,
		EditCondition = "SampleMode == ERevolveSplineSampleMode::PolyLineMaxError"))
	double ErrorTolerance = 1.0;

	/** The maximal distance that the spacing should be allowed to be. */
	UPROPERTY(EditAnywhere, Category = Spline, meta = (ClampMin = 0.01, EditConditionHides,
		EditCondition = "SampleMode == ERevolveSplineSampleMode::UniformSpacingAlongCurve"))
	double MaxSampleDistance = 50.0;

	/** Determines how end caps are created. This is not relevant if the end caps are not visible or if the path is not closed. */
	UPROPERTY(EditAnywhere, Category = Revolve, AdvancedDisplay, meta = (DisplayAfter = "QuadSplitMode",
		EditCondition = "HeightOffsetPerDegree != 0 || RevolveDegrees != 360"))
	ERevolvePropertiesCapFillMode CapFillMode = ERevolvePropertiesCapFillMode::Delaunay;

	/** Connect the ends of an open path to the axis to add caps to the top and bottom of the revolved result.
	  * This is not relevant for paths that are already closed. */
	UPROPERTY(EditAnywhere, Category = Revolve)
	bool bClosePathToAxis = true;
	
	/** Sets the revolution axis origin. */
	UPROPERTY(EditAnywhere, Category = RevolutionAxis, meta = (DisplayName = "Origin",
		Delta = 5, LinearDeltaSensitivity = 1))
	FVector AxisOrigin = FVector(0, 0, 0);

	/** Sets the revolution axis pitch and yaw. */
	UPROPERTY(EditAnywhere, Category = RevolutionAxis, meta = (DisplayName = "Orientation",
		UIMin = -180, UIMax = 180, ClampMin = -180000, ClampMax = 180000))
	FVector2D AxisOrientation;

	/** 
	 * If true, the revolution axis is re-fit to the input spline on each tool start. If false, the previous
	 * revolution axis is kept.
	 */
	UPROPERTY(EditAnywhere, Category = RevolutionAxis, AdvancedDisplay)
	bool bResetAxisOnStart = true;

protected:
	virtual ERevolvePropertiesCapFillMode GetCapFillMode() const override
	{
		return CapFillMode;
	}
};

enum class ERevolveSplineToolAction
{
	ResetAxis
};

UCLASS()
class MESHMODELINGTOOLSEXP_API URevolveSplineToolActionPropertySet : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<URevolveSplineTool> ParentTool;

	void Initialize(URevolveSplineTool* ParentToolIn) { ParentTool = ParentToolIn; }
	void PostAction(ERevolveSplineToolAction Action);

	/** Fit the axis to the current curve(by aligning it to the startand end points) */
	UFUNCTION(CallInEditor, Category = Actions, meta = (DisplayPriority = 1))
	void ResetAxis() { PostAction(ERevolveSplineToolAction::ResetAxis); }
};

//~ We might someday decide to merge this tool as a separate path in the DrawAndRevolveTool...
/**
 * Revolves a selected spline to create a new mesh.
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API URevolveSplineTool : public UInteractiveTool, 
	public IInteractiveToolEditorGizmoAPI, 
	public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:

	virtual void SetSpline(USplineComponent* SplineComponent);

	virtual void SetWorld(UWorld* World) { TargetWorld = World; }
	virtual UWorld* GetTargetWorld() { return TargetWorld.Get(); }
	virtual void RequestAction(ERevolveSplineToolAction ActionType);

	// UInteractiveTool
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IDynamicMeshOperatorFactory
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator();

	// IInteractiveToolEditorGizmoAPI
	virtual bool GetAllowStandardEditorGizmos() override { return true; }

private:

	UPROPERTY()
	TObjectPtr<URevolveSplineToolProperties> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<UNewMeshMaterialProperties> MaterialProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UCreateMeshObjectTypeProperties> OutputTypeProperties = nullptr;

	UPROPERTY()
	TObjectPtr<URevolveSplineToolActionPropertySet> ToolActions = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> Preview = nullptr;

	UPROPERTY()
	TObjectPtr<UConstructionPlaneMechanic> PlaneMechanic = nullptr;

	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	TWeakObjectPtr<USplineComponent> Spline = nullptr;
	
	// Used to try to re-acquire the spline if it is inside a BP actor, where it will be destroyed and recreated
	// whenever the user edits it.
	TWeakObjectPtr<AActor> SplineOwningActor = nullptr;
	int32 SplineComponentIndex;
	// If failed to reacquire once, used to avoid trying to reaquire again.
	bool bLostInputSpline = false;

	// The actual points to be resolved, sampled from the spline
	TArray<FVector3d> ProfileCurve;
	bool bProfileCurveIsClosed;

	// See if the spline has changed, and update our data if so
	void PollSplineUpdates();
	// Track the spline 'Version' integer, which is incremented when splines are changed
	uint32 LastSplineVersion = 0;
	// Used to make sure we update when we first get a spline, regardless of LastSplineVersion
	bool bForceSplineUpdate = true;
	// Update the profile curve and fit plane from spline
	void UpdatePointsFromSpline();

	// Axis direction in vector form (since the user modifiable values are a pitch and yaw)
	FVector3d RevolutionAxisDirection;
	// This duplicates Settings->AxisOrigin, but kept for cleanliness since we do need RevolutionAxisDirection
	FVector3d RevolutionAxisOrigin;
	void UpdateRevolutionAxis();

	void ResetAxis();

	FVector3d SplineFitPlaneOrigin;
	FVector3d SplineFitPlaneNormal;

	virtual void GenerateAsset(const FDynamicMeshOpResult& OpResult);
};



UCLASS(Transient)
class MESHMODELINGTOOLSEXP_API URevolveSplineToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	/** @return true if spline component sources can be found in the active selection */
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	/** @return new Tool instance initialized with selected spline source(s) */
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

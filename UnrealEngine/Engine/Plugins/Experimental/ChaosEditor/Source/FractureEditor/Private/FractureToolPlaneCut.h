// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FractureToolCutter.h"

#include "FractureToolPlaneCut.generated.h"

class FFractureToolContext;


UCLASS(config = EditorPerProjectUserSettings)
class UFracturePlaneCutSettings : public UFractureToolSettings
{
public:
	GENERATED_BODY()

	UFracturePlaneCutSettings(const FObjectInitializer& ObjInit)
		: Super(ObjInit)
		, NumberPlanarCuts(1) {}

	/** Number of cutting planes. Only used when "Use Gizmo" is disabled */
	UPROPERTY(EditAnywhere, Category = PlaneCut, meta = (DisplayName = "Number of Cuts", UIMin = "1", UIMax = "20", ClampMin = "1", EditCondition = "bCanCutWithMultiplePlanes", HideEditConditionToggle))
	int32 NumberPlanarCuts;

	UPROPERTY()
	bool bCanCutWithMultiplePlanes = false;
};


UCLASS(DisplayName = "Plane Cut Tool", Category = "FractureTools")
class UFractureToolPlaneCut : public UFractureToolCutterBase
{
public:
	GENERATED_BODY()

	UFractureToolPlaneCut(const FObjectInitializer& ObjInit);

	// UFractureTool Interface
	virtual FText GetDisplayText() const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetToolIcon() const override;
	virtual void SelectedBonesChanged() override;

	void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;

	virtual void RegisterUICommand(FFractureEditorCommands* BindingContext) override;

	virtual TArray<UObject*> GetSettingsObjects() const override;

	virtual void FractureContextChanged() override;
	virtual int32 ExecuteFracture(const FFractureToolContext& FractureContext) override;

	virtual void Setup() override;
	virtual void Shutdown() override;

protected:
	virtual void ClearVisualizations() override
	{
		Super::ClearVisualizations();
		RenderCuttingPlanesTransforms.Empty();
		PlanesMappings.Empty();
	}

	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;

private:
	// Slicing
	UPROPERTY(EditAnywhere, Category = Slicing)
	TObjectPtr<UFracturePlaneCutSettings> PlaneCutSettings;

	UPROPERTY(EditAnywhere, Category = Uniform)
	TObjectPtr<UFractureTransformGizmoSettings> GizmoSettings;

	void GenerateSliceTransforms(const FFractureToolContext& Context, TArray<FTransform>& CuttingPlaneTransforms);

	float RenderCuttingPlaneSize;
	TArray<FTransform> RenderCuttingPlanesTransforms;
	FVisualizationMappings PlanesMappings;
};



// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FractureTool.h"
#include "FractureToolCutter.h"

#include "FractureToolRadial.generated.h"

class FFractureToolContext;

UCLASS(config = EditorPerProjectUserSettings)
class UFractureRadialSettings : public UFractureToolSettings
{
public:
	GENERATED_BODY()

	UFractureRadialSettings(const FObjectInitializer& ObjInit)
		: Super(ObjInit)
		, Center(FVector(0,0,0))
		, Normal(FVector(0, 0, 1))
		, Radius(50.0f)
		, AngularSteps(5)
		, RadialSteps(5)
		, AngleOffset(0.0f)
		, Variability(0.0f)
	{}

	/** Center of generated pattern. Only used when "Use Gizmo" is disabled */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi, meta = (EditCondition = "!bPositionedByGizmo", HideEditConditionToggle))
	FVector Center;

	/** Normal to plane in which sites are generated. Only used when "Use Gizmo" is disabled */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi, meta = (EditCondition = "!bPositionedByGizmo", HideEditConditionToggle))
	FVector Normal;

	UPROPERTY()
	bool bPositionedByGizmo = true;

	/** Pattern radius (in cm) */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi, meta = (DisplayName = "Radius", UIMin = "0.0", ClampMin = "0.0"))
	float Radius;

	/** Number of angular steps */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi, meta = (DisplayName = "Angular Steps", UIMin = "1", UIMax = "50", ClampMin = "1"))
	int AngularSteps;

	/** Number of radial steps */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi, meta = (DisplayName = "Radial Steps", UIMin = "1", UIMax = "50", ClampMin = "1"))
	int RadialSteps;

	/** Angle offset at each radial step (in degrees) */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi, meta = (DisplayName = "Angle Offset"))
	float AngleOffset;

	/** Amount to randomly displace each Voronoi site (in cm) */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi, meta = (DisplayName = "Variability", UIMin = "0.0", ClampMin = "0.0"))
	float Variability;
};


UCLASS(DisplayName="Radial Voronoi", Category="FractureTools")
class UFractureToolRadial : public UFractureToolVoronoiCutterBase
{
public:
	GENERATED_BODY()

	UFractureToolRadial(const FObjectInitializer& ObjInit);

	// UFractureTool Interface
	virtual FText GetDisplayText() const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetToolIcon() const override;
	virtual TArray<UObject*> GetSettingsObjects() const override;
	virtual void SelectedBonesChanged() override;

	virtual void RegisterUICommand( FFractureEditorCommands* BindingContext ) override;

	// Radial Voronoi Fracture Input Settings
	UPROPERTY(EditAnywhere, Category = Uniform)
	TObjectPtr<UFractureRadialSettings> RadialSettings;

	UPROPERTY(EditAnywhere, Category = Uniform)
	TObjectPtr<UFractureTransformGizmoSettings> GizmoSettings;

	virtual void Setup() override;
	virtual void Shutdown() override;

	virtual void UpdateUseGizmo(bool bUseGizmo) override
	{
		Super::UpdateUseGizmo(bUseGizmo);
		RadialSettings->bPositionedByGizmo = bUseGizmo;
		NotifyOfPropertyChangeByTool(RadialSettings);
	}

protected:
	void GenerateVoronoiSites(const FFractureToolContext& Context, TArray<FVector>& Sites) override;

};
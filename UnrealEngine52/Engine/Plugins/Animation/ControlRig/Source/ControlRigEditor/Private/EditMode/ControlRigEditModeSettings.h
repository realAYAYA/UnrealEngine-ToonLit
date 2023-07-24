// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/LazyObjectPtr.h"
#include "ControlRigEditModeSettings.generated.h"

/** Settings object used to show useful information in the details panel */
UCLASS(config=EditorPerProjectUserSettings, MinimalAPI)
class UControlRigEditModeSettings : public UObject
{
	GENERATED_BODY()

	UControlRigEditModeSettings()
		: bDisplayHierarchy(false)
		, bDisplayNulls(false)
		, bHideControlShapes(false)
		, bShowAllProxyControls(false)
		, bShowControlsAsOverlay(false)
		, bDisplayAxesOnSelection(false)
		, AxisScale(10.f)
		, bCoordSystemPerWidgetMode(true)
		, bOnlySelectRigControls(false)
		, bLocalTransformsInEachLocalSpace(true)
		, GizmoScale(1.0f)
	{
		LastInViewportTweenWidgetLocation = FVector2D(EForceInit::ForceInitToZero);
		DrivenControlColor = FLinearColor::White * FLinearColor(FVector::OneVector * 0.8f);
	}

	// UObject interface
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif
public:

	/** Whether to show all bones in the hierarchy */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	bool bDisplayHierarchy;

	/** Whether to show all nulls in the hierarchy */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	bool bDisplayNulls;

	/** Should we always hide control shapes in viewport */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	bool bHideControlShapes;

	/** Should we always hide control shapes in viewport */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings", meta = (EditCondition = "!bHideControlShapes"))
	bool bShowAllProxyControls;

	/** Determins if controls should be rendered on top of other controls */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings", meta = (EditCondition = "!bHideControlShapes"))
	bool bShowControlsAsOverlay;

	/** Indicates a control being driven by a proxy control */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings", meta = (EditCondition = "!bHideControlShapes"))
	FLinearColor DrivenControlColor;

	/** Should we show axes for the selected elements */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	bool bDisplayAxesOnSelection;

	/** The scale for axes to draw on the selection */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	float AxisScale;

	/** If true we restore the coordinate space when changing Widget Modes in the Viewport*/
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	bool bCoordSystemPerWidgetMode;

	/** If true we can only select Rig Controls in the scene not other Actors. */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	bool bOnlySelectRigControls;

	/** If true when we transform multiple selected objects in the viewport they each transforms along their own local transform space */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	bool bLocalTransformsInEachLocalSpace;
	
	/** The scale for Gizmos */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	float GizmoScale;

	UPROPERTY(config)
	FVector2D LastInViewportTweenWidgetLocation;
};
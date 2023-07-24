// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/MeshSurfacePointTool.h"
#include "InteractiveTool.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_1
#include "Engine/EngineTypes.h"
#endif
#include "Engine/HitResult.h"
#include "BaseBrushTool.generated.h"

class UBrushStampIndicator;
/**
 * Standard properties for a Brush-type Tool
 */
UCLASS()
class INTERACTIVETOOLSFRAMEWORK_API UBrushBaseProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UBrushBaseProperties();

	/** Relative size of brush */
	UPROPERTY(EditAnywhere, Category = Brush, meta = (DisplayName = "Size", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "10.0", DisplayPriority = 1, HideEditConditionToggle, EditCondition = "bSpecifyRadius == false"))
	float BrushSize;

	/** If true, ignore relative Brush Size and use explicit world Radius */
	UPROPERTY(EditAnywhere, Category = Brush, AdvancedDisplay)
	bool bSpecifyRadius;

	/** Radius of brush */
	UPROPERTY(EditAnywhere, Category = Brush, AdvancedDisplay, meta = (EditCondition = "bSpecifyRadius",
		DisplayName = "Radius", UIMin = "1.0", UIMax = "1000.0", ClampMin = "0.1", ClampMax = "50000.0"))
	float BrushRadius;

	/** Strength of the brush (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, Category = Brush, meta = (DisplayName = "Strength", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", HideEditConditionToggle, EditConditionHides, EditCondition = "bShowStrength", DisplayPriority = 2))
	float BrushStrength;

	/** Amount of falloff to apply (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, Category = Brush, meta = (DisplayName = "Falloff", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", HideEditConditionToggle, EditConditionHides, EditCondition="bShowFalloff", DisplayPriority = 3))
	float BrushFalloffAmount;

	/** If false, then BrushStrength will not be shown in DetailsView panels (otherwise no effect) */
	UPROPERTY( meta = (TransientToolProperty))
	bool bShowStrength = true;

	/** If false, then BrushFalloffAmount will not be shown in DetailsView panels (otherwise no effect) */
	UPROPERTY(meta = (TransientToolProperty))
	bool bShowFalloff = true;
};


/**
 * Generic Brush Stamp data
 */
USTRUCT()
struct INTERACTIVETOOLSFRAMEWORK_API FBrushStampData
{
	GENERATED_BODY();
	/** Radius of brush stamp */
	float Radius;
	/** World Position of brush stamp */
	FVector WorldPosition;
	/** World Normal of brush stamp */
	FVector WorldNormal;

	/** Hit Result provided by implementations - may not be fully populated */
	FHitResult HitResult;

	/** Falloff of brush stamp */
	float Falloff;
};



/**
 * UBaseBrushTool implements standard brush-style functionality for an InteractiveTool.
 * This includes:
 *   1) brush radius property set with dimension-relative brush sizing and default brush radius hotkeys
 *   2) brush indicator visualization
 *   3) tracking of last brush stamp location via .LastBrushStamp FProperty
 *   4) status of brush stroke via .bInBrushStroke FProperty
 */
UCLASS()
class INTERACTIVETOOLSFRAMEWORK_API UBaseBrushTool : public UMeshSurfacePointTool
{
	GENERATED_BODY()

public:
	UBaseBrushTool();

	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;


	//
	// UMeshSurfacePointTool implementation
	//
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;

public:

	/** Properties that control brush size/etc*/
	UPROPERTY()
	TObjectPtr<UBrushBaseProperties> BrushProperties;

	/** Set to true by Tool if user is currently in an active brush stroke*/
	UPROPERTY()
	bool bInBrushStroke = false;

	/** Uniform scale factor that scales from world space (where brush usually exists) to local space */
	UPROPERTY()
	float WorldToLocalScale = 1.0f;

	/** Position of brush at last update (both during stroke and during Hover) */
	UPROPERTY()
	FBrushStampData LastBrushStamp;

public:

	virtual void IncreaseBrushSizeAction();
	virtual void DecreaseBrushSizeAction();
	virtual void IncreaseBrushStrengthAction();
	virtual void DecreaseBrushStrengthAction();
	virtual void IncreaseBrushFalloffAction();
	virtual void DecreaseBrushFalloffAction();

	virtual bool IsInBrushStroke() const { return bInBrushStroke; }

	virtual double GetCurrentBrushRadius() const { return CurrentBrushRadius; }
	virtual double GetCurrentBrushRadiusLocal() const { return CurrentBrushRadius * WorldToLocalScale; }

protected:

	/**
	 * Subclasses should implement this to give an estimate of target dimension for brush size scaling
	 */
	virtual double EstimateMaximumTargetDimension() { return 100.0; }

protected:
	TInterval<float> BrushRelativeSizeRange;
	double CurrentBrushRadius;
	void RecalculateBrushRadius();

	UPROPERTY()
	TSoftClassPtr<UBrushBaseProperties> PropertyClass;

	//
	// Brush Indicator support
	//
protected:

	UPROPERTY()
	TObjectPtr<UBrushStampIndicator> BrushStampIndicator;

	virtual void SetupBrushStampIndicator();
	virtual void UpdateBrushStampIndicator();
	virtual void ShutdownBrushStampIndicator();
};

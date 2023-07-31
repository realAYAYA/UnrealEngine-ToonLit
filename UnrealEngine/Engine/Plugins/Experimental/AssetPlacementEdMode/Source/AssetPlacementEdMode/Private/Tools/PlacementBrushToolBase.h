// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputState.h"
#include "BaseTools/BaseBrushTool.h"
#include "Elements/Framework/TypedElementListFwd.h"

#include "PlacementBrushToolBase.generated.h"

struct FHitResult;
class UAssetPlacementSettings;

UCLASS(Abstract, MinimalAPI)
class UPlacementToolBuilderBase : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

protected:
	virtual UPlacementBrushToolBase* FactoryToolInstance(UObject* Outer) const PURE_VIRTUAL(UPlacementToolBuilderBase::FactoryToolInstance, return nullptr; );
};

UCLASS(Abstract, MinimalAPI)
class UPlacementBrushToolBase : public UBaseBrushTool
{
	GENERATED_BODY()

	friend class UPlacementToolBuilderBase;
	
public:
	virtual bool HitTest(const FRay& Ray, FHitResult& OutHit) override;
	virtual bool AreAllTargetsValid() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;

protected:
	virtual double EstimateMaximumTargetDimension() override;
	bool FindHitResultWithStartAndEndTraceVectors(FHitResult& OutHit, const FVector& TraceStart, const FVector& TraceEnd, float TraceRadius = 0.0f);
	static FTransform GenerateTransformFromHitLocationAndNormal(const FVector& InLocation, const FVector& InNormal);

	// Gets a random rotation, and aligns it, based on the placement settings.
	static FQuat GenerateRandomRotation(const UAssetPlacementSettings* PlacementSettings);

	// Generates a random scale, based on placement settings.
	static FVector GenerateRandomScale(const UAssetPlacementSettings* PlacementSettings);

	// Updates the last generated rotation to realign with the current brush position and normal.
	static FQuat AlignRotationWithNormal(const FQuat& InRotation, const FVector& InNormal, EAxis::Type InAlignmentAxis, bool bInvertAxis);
	static FTransform FinalizeTransform(const FTransform& OriginalTransform, const FVector& InNormal, const UAssetPlacementSettings* PlacementSettings);
	FTypedElementListRef GetElementsInBrushRadius(const FInputDeviceRay& DragPos) const;

	float LastBrushStampWorldToPixelScale;
	FInputDeviceRay LastDeviceInputRay = FInputDeviceRay(FRay());
};

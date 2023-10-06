// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectAnnotation.h"
#include "SmartObjectAnnotation_SlotUserCollision.generated.h"

/**
 * Annotation to check if a smart object user can stand on the slot without overlaps.
 * Uses validation filter to get the user capsule for collisions testing, or use specific capsule defined in the annotation.
 */
USTRUCT(meta = (DisplayName="Slot User Collision"))
struct SMARTOBJECTSMODULE_API FSmartObjectAnnotation_SlotUserCollision : public FSmartObjectSlotAnnotation
{
	GENERATED_BODY()

#if WITH_EDITOR
	virtual void DrawVisualization(FSmartObjectVisualizationContext& VisContext) const override;
	virtual void DrawVisualizationHUD(FSmartObjectVisualizationContext& VisContext) const override;
#endif
	
	virtual void GetColliders(const FSmartObjectUserCapsuleParams& UserCapsule, const FTransform& SlotTransform, TArray<FSmartObjectAnnotationCollider>& OutColliders) const;

#if WITH_GAMEPLAY_DEBUGGER
	virtual void CollectDataForGameplayDebugger(FSmartObjectAnnotationGameplayDebugContext& DebugContext) const override;
#endif // WITH_GAMEPLAY_DEBUGGER	

	/** If true, the user capsule size is got from validation filter and used for testing. */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bUseUserCapsuleSize = true;

	/** Dimensions of the capsule to test at the slot location. */
	UPROPERTY(EditAnywhere, Category="Default", meta = (EditCondition = "bUseUserCapsuleSize == false", EditConditionHides))
	FSmartObjectUserCapsuleParams Capsule;
};

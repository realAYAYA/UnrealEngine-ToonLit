// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectAnnotation.h"
#include "SmartObjectSlotEntryAnnotation.generated.h"

/**
 * Annotation to define a navigation entry for a Smart Object Slot.
 * This can be used to add multiple entry points to a slot, or to validate the entries against navigation data. 
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectSlotEntryAnnotation : public FSmartObjectSlotAnnotation
{
	GENERATED_BODY()

#if UE_ENABLE_DEBUG_DRAWING
	virtual void DrawVisualization(FSmartObjectVisualizationContext& VisContext) const override;
	virtual void DrawVisualizationHUD(FSmartObjectVisualizationContext& VisContext) const override;
	virtual TOptional<FTransform> GetWorldTransform(const FTransform& SlotTransform) const override;
	virtual void AdjustWorldTransform(const FTransform& SlotTransform, const FVector& DeltaTranslation, const FRotator& DeltaRotation) override;
#endif
	
	/** Local space offset of the entry. */
	UPROPERTY(EditAnywhere, Category="Default")
	FVector3f Offset = FVector3f(0.f);

	/** Local space rotation of the entry. */
	UPROPERTY(EditAnywhere, Category="Default")
	FRotator Rotation = FRotator(0.f);
};

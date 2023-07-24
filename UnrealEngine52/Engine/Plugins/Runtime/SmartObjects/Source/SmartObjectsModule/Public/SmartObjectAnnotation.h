// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "SmartObjectAnnotation.generated.h"

struct FSmartObjectVisualizationContext;

/**
 * Base class for Smart Object Slot annotations. Annotation is a specific type of slot definition data that has methods to visualize it.
 */
USTRUCT(meta=(Hidden))
struct SMARTOBJECTSMODULE_API FSmartObjectSlotAnnotation : public FSmartObjectSlotDefinitionData
{
	GENERATED_BODY()
	virtual ~FSmartObjectSlotAnnotation() override {}

#if UE_ENABLE_DEBUG_DRAWING
	// @todo: Try to find a way to add visualization without requiring virtual functions.

	/** Methods to override to draw 3D visualization of the annotation. */
	virtual void DrawVisualization(FSmartObjectVisualizationContext& VisContext) const {}

	/** Methods to override to draw canvas visualization of the annotation. */
	virtual void DrawVisualizationHUD(FSmartObjectVisualizationContext& VisContext) const {}
	
	/**
	 * Returns the world space transform of the annotation.
	 * @param SlotTransform World space transform of the slot.
	 * @return world transform of the annotation, or empty if annotation does not have transform.
	 */
	virtual TOptional<FTransform> GetWorldTransform(const FTransform& SlotTransform) const { return TOptional<FTransform>(); }
	
	/**
	 * Called in editor to adjust the transform of the annotation.
	 * @param SlotTransform World space transform of the slot.
	 * @param DeltaTranslation World space delta translation to apply.
	 * @param DeltaRotation World space delta rotation to apply.
	 **/
	virtual void AdjustWorldTransform(const FTransform& SlotTransform, const FVector& DeltaTranslation, const FRotator& DeltaRotation) {}
#endif
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "SmartObjectAnnotation.generated.h"

struct FSmartObjectVisualizationContext;
class FGameplayDebuggerCategory;
struct FSmartObjectSlotView;
class USmartObjectDefinition;
struct FSmartObjectAnnotationGameplayDebugContext;

/**
 * Base class for Smart Object Slot annotations. Annotation is a specific type of slot definition data that has methods to visualize it.
 */
USTRUCT(meta=(Hidden))
struct SMARTOBJECTSMODULE_API FSmartObjectSlotAnnotation : public FSmartObjectDefinitionData
{
	GENERATED_BODY()
	virtual ~FSmartObjectSlotAnnotation() override {}

#if WITH_EDITOR
	// @todo: Try to find a way to add visualization without requiring virtual functions.

	/** Methods to override to draw 3D visualization of the annotation. */
	virtual void DrawVisualization(FSmartObjectVisualizationContext& VisContext) const {}

	/** Methods to override to draw canvas visualization of the annotation. */
	virtual void DrawVisualizationHUD(FSmartObjectVisualizationContext& VisContext) const {}

	/**
	 * Called in editor to adjust the transform of the annotation.
	 * @param SlotTransform World space transform of the slot.
	 * @param DeltaTranslation World space delta translation to apply.
	 * @param DeltaRotation World space delta rotation to apply.
	 **/
	virtual void AdjustWorldTransform(const FTransform& SlotTransform, const FVector& DeltaTranslation, const FRotator& DeltaRotation) {}
#endif // WITH_EDITOR

	/** @return true if the the annotation has transform. Annotations with transforms can be selected and edited in the editor viewport. */
	virtual bool HasTransform() const { return false; }
	
	/**
	 * Returns the world space transform of the annotation.
	 * @param SlotTransform World space transform of the slot.
	 * @return World space transform of the annotation.
	 */
	virtual FTransform GetAnnotationWorldTransform(const FTransform& SlotTransform) const { return FTransform(); };

	UE_DEPRECATED(5.3, "Use HasTransform() and GetWorldTransform() instead.")
	virtual TOptional<FTransform> GetWorldTransform(const FTransform& SlotTransform) const final { return TOptional<FTransform>(); }

#if WITH_GAMEPLAY_DEBUGGER
	virtual void CollectDataForGameplayDebugger(FSmartObjectAnnotationGameplayDebugContext& DebugContext) const {}
#endif // WITH_GAMEPLAY_DEBUGGER	
	
};

/**
 * Context passed to CollectDataForGameplayDebugger to show gameplay debugger information.
 */
struct SMARTOBJECTSMODULE_API FSmartObjectAnnotationGameplayDebugContext
{
	explicit FSmartObjectAnnotationGameplayDebugContext(FGameplayDebuggerCategory& InCategory, const USmartObjectDefinition& InDefinition)
		: Category(InCategory)
		, Definition(InDefinition)
	{
	}
	
	FGameplayDebuggerCategory& Category;
	const USmartObjectDefinition& Definition;
	const AActor* SmartObjectOwnerActor = nullptr;
	const AActor* DebugActor = nullptr;
	FTransform SlotTransform;
	FVector ViewLocation;
	FVector ViewDirection;
};

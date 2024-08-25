// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectAnnotation.h"
#include "SmartObjectSlotLinkAnnotation.generated.h"

/**
 * Annotation to allow to find slots based on a Gameplay Tag.
 * This can be used to reuse same behavior on different slots, allowing to use a tag to identify a related slot. 
 */
USTRUCT(meta = (DisplayName="Slot Link"))
struct SMARTOBJECTSMODULE_API FSmartObjectSlotLinkAnnotation : public FSmartObjectSlotAnnotation
{
	GENERATED_BODY()
	virtual ~FSmartObjectSlotLinkAnnotation() override {}

#if WITH_EDITOR
	virtual void DrawVisualization(FSmartObjectVisualizationContext& VisContext) const override;
	virtual void DrawVisualizationHUD(FSmartObjectVisualizationContext& VisContext) const override;
#endif
	
	/** Tag to identify this slot. */
	UPROPERTY(EditAnywhere, Category="Default")
	FGameplayTag Tag;

	/** The slot the annotation points at. */
	UPROPERTY(EditAnywhere, Category="Default", meta = (NoBinding))
	FSmartObjectSlotReference LinkedSlot;
};

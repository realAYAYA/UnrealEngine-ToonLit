// Copyright Epic Games, Inc. All Rights Reserved.

#include "Annotations/SmartObjectSlotLinkAnnotation.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectVisualizationContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectSlotLinkAnnotation)

#if UE_ENABLE_DEBUG_DRAWING

void FSmartObjectSlotLinkAnnotation::DrawVisualization(FSmartObjectVisualizationContext& VisContext) const
{
	if (!LinkedSlot.IsValid() || !VisContext.Definition.IsValidSlotIndex(LinkedSlot.GetIndex()))
	{
		return;
	}
	
	const FSmartObjectSlotIndex SlotIndex(VisContext.SlotIndex);
	const FSmartObjectSlotIndex LinkedSlotIndex(LinkedSlot.GetIndex());
	const TOptional<FTransform> Transform = VisContext.Definition.GetSlotTransform(VisContext.OwnerLocalToWorld, SlotIndex);
	const TOptional<FTransform> TargetTransform = VisContext.Definition.GetSlotTransform(VisContext.OwnerLocalToWorld, LinkedSlotIndex);
	
	if (Transform.IsSet() && TargetTransform.IsSet())
	{
		FLinearColor Color = FLinearColor::White;
		if (VisContext.bIsSlotSelected)
		{
			Color = VisContext.SelectedColor;
		}
		VisContext.DrawArrow(Transform.GetValue().GetLocation(), TargetTransform.GetValue().GetLocation(), Color, 15.0f, 15.0f, /*DepthPrioGroup*/0, /*Thickness*/1.0f, /*DepthBias*/2.0);
	}
}

void FSmartObjectSlotLinkAnnotation::DrawVisualizationHUD(FSmartObjectVisualizationContext& VisContext) const
{
	if (!LinkedSlot.IsValid() || !VisContext.Definition.IsValidSlotIndex(LinkedSlot.GetIndex()))
	{
		return;
	}
	const FSmartObjectSlotIndex SlotIndex(VisContext.SlotIndex);
	const FSmartObjectSlotIndex LinkedSlotIndex(LinkedSlot.GetIndex());
	const TOptional<FTransform> Transform = VisContext.Definition.GetSlotTransform(VisContext.OwnerLocalToWorld, SlotIndex);
	const TOptional<FTransform> TargetTransform = VisContext.Definition.GetSlotTransform(VisContext.OwnerLocalToWorld, LinkedSlotIndex);

	if (VisContext.bIsSlotSelected
		&& Transform.IsSet() && TargetTransform.IsSet())
	{
		const FVector LabelPos = FMath::Lerp(Transform.GetValue().GetLocation(), TargetTransform.GetValue().GetLocation(), 0.3f);
		VisContext.DrawString(LabelPos, *Tag.ToString(), FLinearColor::White);
	}
}

#endif

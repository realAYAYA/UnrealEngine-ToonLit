// Copyright Epic Games, Inc. All Rights Reserved.

#include "Annotations/SmartObjectSlotLinkAnnotation.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectVisualizationContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectSlotLinkAnnotation)

#if WITH_EDITOR

void FSmartObjectSlotLinkAnnotation::DrawVisualization(FSmartObjectVisualizationContext& VisContext) const
{
	constexpr float DepthBias = 2.0f;
	constexpr bool Screenspace = true;

	if (!LinkedSlot.IsValid() || !VisContext.Definition.IsValidSlotIndex(LinkedSlot.GetIndex()))
	{
		return;
	}
	
	const FTransform Transform = VisContext.Definition.GetSlotWorldTransform(VisContext.SlotIndex, VisContext.OwnerLocalToWorld);
	const FTransform TargetTransform = VisContext.Definition.GetSlotWorldTransform(LinkedSlot.GetIndex(), VisContext.OwnerLocalToWorld);
	
	FLinearColor Color = FLinearColor::White;
	if (VisContext.bIsSlotSelected)
	{
		Color = VisContext.SelectedColor;
	}
	VisContext.DrawArrow(Transform.GetLocation(), TargetTransform.GetLocation(), Color, 5.0f, 5.0f, /*DepthPrioGroup*/0, /*Thickness*/1.0f, DepthBias, Screenspace);
}

void FSmartObjectSlotLinkAnnotation::DrawVisualizationHUD(FSmartObjectVisualizationContext& VisContext) const
{
	if (!LinkedSlot.IsValid() || !VisContext.Definition.IsValidSlotIndex(LinkedSlot.GetIndex()))
	{
		return;
	}
	const FTransform Transform = VisContext.Definition.GetSlotWorldTransform(VisContext.SlotIndex, VisContext.OwnerLocalToWorld);
	const FTransform TargetTransform = VisContext.Definition.GetSlotWorldTransform(LinkedSlot.GetIndex(), VisContext.OwnerLocalToWorld);

	if (VisContext.bIsSlotSelected)
	{
		const FVector LabelPos = FMath::Lerp(Transform.GetLocation(), TargetTransform.GetLocation(), 0.3);
		VisContext.DrawString(LabelPos, *Tag.ToString(), FLinearColor::White);
	}
}

#endif // WITH_EDITOR

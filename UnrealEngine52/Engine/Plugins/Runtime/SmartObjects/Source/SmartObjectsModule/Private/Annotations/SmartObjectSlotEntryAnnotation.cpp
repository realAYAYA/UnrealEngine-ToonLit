// Copyright Epic Games, Inc. All Rights Reserved.

#include "Annotations/SmartObjectSlotEntryAnnotation.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectVisualizationContext.h"
#include "SceneManagement.h" // FPrimitiveDrawInterface

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectSlotEntryAnnotation)

#if UE_ENABLE_DEBUG_DRAWING

namespace UE::SmartObject::Annotations
{
	static constexpr FColor EntryColor(0, 64, 192); 

};

void FSmartObjectSlotEntryAnnotation::DrawVisualization(FSmartObjectVisualizationContext& VisContext) const
{
	constexpr FVector::FReal MarkerRadius = 30.0;
	constexpr FVector::FReal TickSize = 10.0;
	constexpr FVector::FReal MinArrowDrawDistance = 30.0;

	const FSmartObjectSlotIndex SlotIndex(VisContext.SlotIndex);
	const TOptional<FTransform> SlotTransform = VisContext.Definition.GetSlotTransform(VisContext.OwnerLocalToWorld, SlotIndex);

	if (SlotTransform.IsSet())
	{
		FLinearColor Color = UE::SmartObject::Annotations::EntryColor;
		if (VisContext.bIsAnnotationSelected)
		{
			Color = VisContext.SelectedColor; 
		}

		const TOptional<FTransform> AnnotationTransform = GetWorldTransform(*SlotTransform);
		if (AnnotationTransform.IsSet())
		{
			const FVector SlotWorldLocation = SlotTransform->GetTranslation();
			const FVector EntryWorldLocation = AnnotationTransform->GetTranslation();
			const FVector AxisX = AnnotationTransform->GetUnitAxis(EAxis::X);
			const FVector AxisY = AnnotationTransform->GetUnitAxis(EAxis::Y);

			// Triangle pointing to forward direction.
			const FVector V0 = EntryWorldLocation + AxisX * MarkerRadius;
			const FVector V1 = EntryWorldLocation - AxisX * MarkerRadius * 0.5 + AxisY * MarkerRadius;
			const FVector V2 = EntryWorldLocation - AxisX * MarkerRadius * 0.5 - AxisY * MarkerRadius;
			VisContext.PDI->DrawTranslucentLine(V0, V1, Color, SDPG_World, 2.0f);
			VisContext.PDI->DrawTranslucentLine(V1, V2, Color, SDPG_World, 2.0f);
			VisContext.PDI->DrawTranslucentLine(V2, V0, Color, SDPG_World, 2.0f);

			// Tick at the center.
			VisContext.PDI->DrawTranslucentLine(EntryWorldLocation - AxisX * TickSize, EntryWorldLocation + AxisX * TickSize, Color, SDPG_World, 1.0f);
			VisContext.PDI->DrawTranslucentLine(EntryWorldLocation - AxisY * TickSize, EntryWorldLocation + AxisY * TickSize, Color, SDPG_World, 1.0f);

			// Arrow pointing at the the slot, if far enough from the slot.
			if (FVector::DistSquared(EntryWorldLocation, SlotWorldLocation) > FMath::Square(MinArrowDrawDistance))
			{
				VisContext.DrawArrow(EntryWorldLocation, SlotWorldLocation, Color, 15.0f, 15.0f, /*DepthPrioGroup*/0, /*Thickness*/1.0f, /*DepthBias*/2.0);
			}
		}
	}
}

void FSmartObjectSlotEntryAnnotation::DrawVisualizationHUD(FSmartObjectVisualizationContext& VisContext) const
{
	const FSmartObjectSlotIndex SlotIndex(VisContext.SlotIndex);
	const TOptional<FTransform> SlotTransform = VisContext.Definition.GetSlotTransform(VisContext.OwnerLocalToWorld, SlotIndex);
	
	if (SlotTransform.IsSet() && VisContext.bIsAnnotationSelected)
	{
		TOptional<FTransform> AnnotationTransform = GetWorldTransform(*SlotTransform);
		if (AnnotationTransform.IsSet())
		{
			const FVector EntryWorldLocation = AnnotationTransform->GetTranslation();
			VisContext.DrawString(EntryWorldLocation, TEXT("Entry"), UE::SmartObject::Annotations::EntryColor);
		}
	}
}

TOptional<FTransform> FSmartObjectSlotEntryAnnotation::GetWorldTransform(const FTransform& SlotTransform) const
{
	const FTransform LocalTransform(Rotation, FVector(Offset));
	return TOptional(LocalTransform * SlotTransform);
}

void FSmartObjectSlotEntryAnnotation::AdjustWorldTransform(const FTransform& SlotTransform, const FVector& DeltaTranslation, const FRotator& DeltaRotation)
{
	if (!DeltaTranslation.IsZero())
	{
		const FVector LocalTranslation = SlotTransform.InverseTransformVector(DeltaTranslation);
		Offset += FVector3f(LocalTranslation);
	}

	if (!DeltaRotation.IsZero())
	{
		const FRotator LocalRotation = SlotTransform.InverseTransformRotation(DeltaRotation.Quaternion()).Rotator();
		Rotation += LocalRotation;
		Rotation.Normalize();
	}
}

#endif

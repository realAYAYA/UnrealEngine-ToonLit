// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectComponentVisualizer.h"
#include "SmartObjectComponent.h"
#include "SceneManagement.h"

void FSmartObjectComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	const USmartObjectComponent* SOComp = Cast<const USmartObjectComponent>(Component);
	if (SOComp == nullptr)
	{
		return;
	}

	const USmartObjectDefinition* Definition = SOComp->GetDefinition();
	if (Definition == nullptr)
	{
		return;
	}

	FColor Color = FColor::White;

	const FTransform OwnerLocalToWorld = SOComp->GetComponentTransform();
	for (int32 i = 0; i < Definition->GetSlots().Num(); ++i)
	{
		constexpr float DebugCylinderRadius = 40.f;
		TOptional<FTransform> Transform = Definition->GetSlotTransform(OwnerLocalToWorld, FSmartObjectSlotIndex(i));
		if (!Transform.IsSet())
		{
			continue;
		}
#if WITH_EDITORONLY_DATA
		Color = Definition->GetSlots()[i].DEBUG_DrawColor;
#endif

		const FVector Location = Transform.GetValue().GetLocation();

		DrawDirectionalArrow(PDI, Transform.GetValue().ToMatrixNoScale(), Color, 2.f*DebugCylinderRadius, /*ArrowSize*/1.f, SDPG_World, /*Thickness*/1.0f);
		DrawCircle(PDI, Location, FVector::XAxisVector, FVector::YAxisVector, Color, DebugCylinderRadius, /*NumSides*/64, SDPG_World, /*Thickness*/2.f);
	}
}

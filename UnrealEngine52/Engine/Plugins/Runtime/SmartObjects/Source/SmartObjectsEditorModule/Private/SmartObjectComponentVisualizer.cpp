// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectComponentVisualizer.h"
#include "Engine/Engine.h"
#include "SmartObjectComponent.h"
#include "SceneManagement.h"
#include "SmartObjectAnnotation.h"
#include "SmartObjectVisualizationContext.h"
#include "Misc/EnumerateRange.h"
#include "Settings/EditorStyleSettings.h"

IMPLEMENT_HIT_PROXY(HSmartObjectSlotProxy, HComponentVisProxy);


namespace UE::SmartObjects::Editor
{

void Draw(const USmartObjectDefinition& Definition, TConstArrayView<FSelectedItem> Selection, const FTransform& OwnerLocalToWorld, const FSceneView& View, FPrimitiveDrawInterface& PDI)
{
	FSmartObjectVisualizationContext VisContext(Definition);
	VisContext.OwnerLocalToWorld = OwnerLocalToWorld;
	VisContext.View = &View;
	VisContext.PDI = &PDI;
	VisContext.Font = GEngine->GetSmallFont();
	VisContext.SelectedColor = GetDefault<UEditorStyleSettings>()->SelectionColor;

	if (!VisContext.IsValidForDraw())
	{
		return;
	}

	FLinearColor Color = FColor::White;
	FGuid SlotID;
	bool bIsSelected = false;

	const TConstArrayView<FSmartObjectSlotDefinition> Slots = Definition.GetSlots();
	for (TConstEnumerateRef<FSmartObjectSlotDefinition> Slot : EnumerateRange(Slots))
	{
		constexpr FVector::FReal DebugCylinderRadius = 40.0;
		constexpr FVector::FReal TickSize = 10.0;

		TOptional<FTransform> Transform = Definition.GetSlotTransform(OwnerLocalToWorld, FSmartObjectSlotIndex(Slot.GetIndex()));
		if (!Transform.IsSet())
		{
			continue;
		}
		bIsSelected = false;
#if WITH_EDITORONLY_DATA
		Color = Slot->bEnabled ? Slot->DEBUG_DrawColor : FColor::Silver;
		SlotID = Slot->ID;

		if (Selection.Contains(FSelectedItem(Slot->ID)))
		{
			Color = VisContext.SelectedColor;
			bIsSelected = true;
		}
#endif 

		PDI.SetHitProxy(new HSmartObjectSlotProxy(/*Component*/nullptr, SlotID));

		{
			const FVector Location = Transform->GetLocation();
			const FVector AxisX = Transform->GetUnitAxis(EAxis::X);
			const FVector AxisY = Transform->GetUnitAxis(EAxis::Y);

			// Arrow with tick at base.
			VisContext.DrawArrow(Location - AxisX * TickSize, Location + AxisX * DebugCylinderRadius * 2.0, Color, /*ArrowHeadLength*/ 20.0f, /*EndLocationInset*/ 0.0f, SDPG_World);
			PDI.DrawTranslucentLine(Location - AxisY * TickSize, Location + AxisY * TickSize, Color, SDPG_World, 1.0f);

			// Circle and direction arrow.
			DrawCircle(&PDI, Location, AxisX, AxisY, Color, DebugCylinderRadius, /*NumSides*/64, SDPG_World, /*Thickness*/2.f);
		}
			
		PDI.SetHitProxy(nullptr);

		for (TConstEnumerateRef<FInstancedStruct> Data : EnumerateRange(Slot->Data))
		{
			if (const FSmartObjectSlotAnnotation* Annotation = Data->GetPtr<FSmartObjectSlotAnnotation>())
			{
				PDI.SetHitProxy(new HSmartObjectSlotProxy(/*Component*/nullptr, SlotID, Data.GetIndex()));

				VisContext.SlotIndex = FSmartObjectSlotIndex(Slot.GetIndex());
				VisContext.bIsSlotSelected = bIsSelected;
				VisContext.bIsAnnotationSelected = Selection.Contains(FSelectedItem(Slot->ID, Data.GetIndex()));

				Annotation->DrawVisualization(VisContext);
			}
		}
		
	}
}

void DrawCanvas(const USmartObjectDefinition& Definition, TConstArrayView<FSelectedItem> Selection, const FTransform& OwnerLocalToWorld, const FSceneView& View, FCanvas& Canvas)
{
	FSmartObjectVisualizationContext VisContext(Definition);
	VisContext.OwnerLocalToWorld = OwnerLocalToWorld;
	VisContext.View = &View;
	VisContext.Canvas = &Canvas;
	VisContext.Font = GEngine->GetSmallFont();
	VisContext.SelectedColor = GetDefault<UEditorStyleSettings>()->SelectionColor;

	if (!VisContext.IsValidForDrawHUD())
	{
		return;
	}

	FColor Color = FColor::White;
	bool bIsSelected = false;

	const TConstArrayView<FSmartObjectSlotDefinition> Slots = Definition.GetSlots();
	for (TConstEnumerateRef<FSmartObjectSlotDefinition> Slot : EnumerateRange(Slots))
	{
		TOptional<FTransform> Transform = Definition.GetSlotTransform(OwnerLocalToWorld, FSmartObjectSlotIndex(Slot.GetIndex()));
		if (!Transform.IsSet())
		{
			continue;
		}

		bIsSelected = false;
#if WITH_EDITORONLY_DATA
		Color = Slot->bEnabled ? Slot->DEBUG_DrawColor : FColor::Silver;

		if (Selection.Contains(Slot->ID))
		{
			Color = FColor::Red;
			bIsSelected = true;
		}
#endif 

		// Slot name
		const FVector SlotLocation = Transform->GetLocation();
		VisContext.DrawString(SlotLocation, *Slot->Name.ToString(), Color);

		// Slot data annotations
		for (TConstEnumerateRef<FInstancedStruct> Data : EnumerateRange(Slot->Data))
		{
			if (const FSmartObjectSlotAnnotation* Annotation = Data->GetPtr<FSmartObjectSlotAnnotation>())
			{
				VisContext.SlotIndex = FSmartObjectSlotIndex(Slot.GetIndex());
				VisContext.bIsSlotSelected = bIsSelected;
				VisContext.bIsAnnotationSelected = Selection.Contains(FSelectedItem(Slot->ID, Data.GetIndex()));
				
				Annotation->DrawVisualizationHUD(VisContext);
			}
		}
	}
}

}; // UE::SmartObjects::Editor

void FSmartObjectComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if (View == nullptr || PDI == nullptr)
	{
		return;
	}
	
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

	const FTransform OwnerLocalToWorld = SOComp->GetComponentTransform();

	UE::SmartObjects::Editor::Draw(*Definition, {}, OwnerLocalToWorld, *View, *PDI);
}


void FSmartObjectComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	if (View == nullptr || Canvas == nullptr)
	{
		return;
	}

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

	const FTransform OwnerLocalToWorld = SOComp->GetComponentTransform();

	UE::SmartObjects::Editor::DrawCanvas(*Definition, {}, OwnerLocalToWorld, *View, *Canvas);
}

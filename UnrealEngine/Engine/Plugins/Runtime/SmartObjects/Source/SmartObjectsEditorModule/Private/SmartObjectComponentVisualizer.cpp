// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectComponentVisualizer.h"
#include "Engine/Engine.h"
#include "SmartObjectComponent.h"
#include "SceneManagement.h"
#include "SmartObjectAnnotation.h"
#include "SmartObjectVisualizationContext.h"
#include "Misc/EnumerateRange.h"
#include "Settings/EditorStyleSettings.h"

IMPLEMENT_HIT_PROXY(HSmartObjectItemProxy, HComponentVisProxy);


namespace UE::SmartObject::Editor
{

void Draw(const USmartObjectDefinition& Definition, TConstArrayView<FGuid> Selection, const FTransform& OwnerLocalToWorld, const FSceneView& View, FPrimitiveDrawInterface& PDI, const UWorld& World, const AActor* PreviewActor)
{
	constexpr float DepthBias = 2.0f;
	constexpr bool Screenspace = true;

	FSmartObjectVisualizationContext VisContext(Definition, World);
	VisContext.OwnerLocalToWorld = OwnerLocalToWorld;
	VisContext.View = &View;
	VisContext.PDI = &PDI;
	VisContext.Font = GEngine->GetSmallFont();
	VisContext.SelectedColor = GetDefault<UEditorStyleSettings>()->SelectionColor;
	VisContext.PreviewActor = PreviewActor;

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

		const FTransform Transform = Definition.GetSlotWorldTransform(Slot.GetIndex(), OwnerLocalToWorld);

		bIsSelected = false;
		float SlotSize = DebugCylinderRadius;
		ESmartObjectSlotShape SlotShape = ESmartObjectSlotShape::Circle;
		
#if WITH_EDITORONLY_DATA
		Color = Slot->bEnabled ? Slot->DEBUG_DrawColor : FColor::Silver;
		SlotID = Slot->ID;
		SlotShape = Slot->DEBUG_DrawShape;
		SlotSize = Slot->DEBUG_DrawSize;

		if (Selection.Contains(Slot->ID))
		{
			Color = VisContext.SelectedColor;
			bIsSelected = true;
		}
#endif 

		PDI.SetHitProxy(new HSmartObjectItemProxy(SlotID));

		{
			const FVector Location = Transform.GetLocation();
			const FVector AxisX = Transform.GetUnitAxis(EAxis::X);
			const FVector AxisY = Transform.GetUnitAxis(EAxis::Y);

			// Arrow with tick at base.
			VisContext.DrawArrow(Location - AxisX * TickSize, Location + AxisX * SlotSize * 2.0, Color, /*ArrowHeadLength*/ 10.0f, /*EndLocationInset*/ 0.0f, SDPG_World, 2.0f, DepthBias, Screenspace);
			PDI.DrawTranslucentLine(Location - AxisY * TickSize, Location + AxisY * TickSize, Color, SDPG_World, 1.0f, DepthBias, Screenspace);

			// Circle and direction arrow.
			if (SlotShape == ESmartObjectSlotShape::Circle)
			{
				DrawCircle(&PDI, Location, AxisX, AxisY, Color, SlotSize, /*NumSides*/64, SDPG_World, /*Thickness*/4.f, DepthBias, Screenspace);
			}
			else if (SlotShape == ESmartObjectSlotShape::Rectangle)
			{
				DrawRectangle(&PDI, Location, AxisX, AxisY, Color.ToFColor(/*bSRGB*/true), SlotSize * 2.0f, SlotSize * 2.0f, SDPG_World, /*Thickness*/4.f, DepthBias, Screenspace);
			}
		}
			
		PDI.SetHitProxy(nullptr);

		for (TConstEnumerateRef<const FSmartObjectDefinitionDataProxy> DataProxy : EnumerateRange(Slot->DefinitionData))
		{
			if (const FSmartObjectSlotAnnotation* Annotation = DataProxy->Data.GetPtr<FSmartObjectSlotAnnotation>())
			{
				PDI.SetHitProxy(new HSmartObjectItemProxy(DataProxy->ID));

				VisContext.SlotIndex = Slot.GetIndex();
				VisContext.AnnotationIndex = DataProxy.GetIndex();
				VisContext.bIsSlotSelected = bIsSelected;
				VisContext.bIsAnnotationSelected = Selection.Contains(DataProxy->ID);

				Annotation->DrawVisualization(VisContext);
			}
		}
		
	}
}

void DrawCanvas(const USmartObjectDefinition& Definition, TConstArrayView<FGuid> Selection, const FTransform& OwnerLocalToWorld, const FSceneView& View, FCanvas& Canvas, const UWorld& World, const AActor* PreviewActor)
{
	FSmartObjectVisualizationContext VisContext(Definition, World);
	VisContext.OwnerLocalToWorld = OwnerLocalToWorld;
	VisContext.View = &View;
	VisContext.Canvas = &Canvas;
	VisContext.Font = GEngine->GetSmallFont();
	VisContext.SelectedColor = GetDefault<UEditorStyleSettings>()->SelectionColor;
	VisContext.PreviewActor = PreviewActor;

	if (!VisContext.IsValidForDrawHUD())
	{
		return;
	}

	FColor Color = FColor::White;
	bool bIsSelected = false;

	const TConstArrayView<FSmartObjectSlotDefinition> Slots = Definition.GetSlots();
	for (TConstEnumerateRef<FSmartObjectSlotDefinition> Slot : EnumerateRange(Slots))
	{
		const FTransform Transform = Definition.GetSlotWorldTransform(Slot.GetIndex(), OwnerLocalToWorld);

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
		const FVector SlotLocation = Transform.GetLocation();
		VisContext.DrawString(SlotLocation, *Slot->Name.ToString(), Color);

		// Slot data annotations
		for (TConstEnumerateRef<const FSmartObjectDefinitionDataProxy> DataProxy : EnumerateRange(Slot->DefinitionData))
		{
			if (const FSmartObjectSlotAnnotation* Annotation = DataProxy->Data.GetPtr<FSmartObjectSlotAnnotation>())
			{
				VisContext.SlotIndex = Slot.GetIndex();
				VisContext.AnnotationIndex = DataProxy.GetIndex();
				VisContext.bIsSlotSelected = bIsSelected;
				VisContext.bIsAnnotationSelected = Selection.Contains(DataProxy->ID);
				
				Annotation->DrawVisualizationHUD(VisContext);
			}
		}
	}
}

}; // UE::SmartObject::Editor

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

	UE::SmartObject::Editor::Draw(*Definition, {}, OwnerLocalToWorld, *View, *PDI, *Component->GetWorld(), Component->GetOwner());
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

	UE::SmartObject::Editor::DrawCanvas(*Definition, {}, OwnerLocalToWorld, *View, *Canvas, *Component->GetWorld(), Component->GetOwner());
}

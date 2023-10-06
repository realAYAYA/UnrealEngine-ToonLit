// Copyright Epic Games, Inc. All Rights Reserved.

#include "Annotations/SmartObjectAnnotation_SlotUserCollision.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectVisualizationContext.h"
#include "SceneManagement.h" // FPrimitiveDrawInterface
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Annotations/SmartObjectSlotEntranceAnnotation.h"
#include "SmartObjectSettings.h"
#include "SmartObjectUserComponent.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategory.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectAnnotation_SlotUserCollision)

namespace UE::SmartObject::Annotations
{
	static constexpr FColor CapsuleColor(255, 255, 255, 192);
	static constexpr FColor CapsuleFloorColor(255, 32, 16, 64);
	static constexpr FColor CapsuleCollisionColor(255, 255, 255, 128);

} // UE::SmartObject::Annotations

#if WITH_EDITOR

void FSmartObjectAnnotation_SlotUserCollision::DrawVisualization(FSmartObjectVisualizationContext& VisContext) const
{
	constexpr float DepthBias = 2.0f;
	constexpr bool ScreenSpace = true;
	
	const FTransform SlotTransform = VisContext.Definition.GetSlotWorldTransform(VisContext.SlotIndex, VisContext.OwnerLocalToWorld);

	const USmartObjectSlotValidationFilter* PreviewValidationFilter = VisContext.PreviewValidationFilterClass.GetDefaultObject();
	check(PreviewValidationFilter);
	const FSmartObjectSlotValidationParams& ValidationParams = PreviewValidationFilter->GetEntryValidationParams();

	FSmartObjectUserCapsuleParams CapsuleValue = Capsule;
	if (bUseUserCapsuleSize)
	{
		if (!ValidationParams.GetPreviewUserCapsule(VisContext.World, CapsuleValue))
		{
			return;
		}
	}

	FLinearColor Color = UE::SmartObject::Annotations::CapsuleColor;
	if (VisContext.bIsAnnotationSelected)
	{
		Color = VisContext.SelectedColor;
	}

	TArray<FSmartObjectAnnotationCollider> Colliders;
	GetColliders(CapsuleValue, SlotTransform, Colliders);

	const FSmartObjectTraceParams& OverlapParameters = ValidationParams.GetTransitionTraceParameters();
	FCollisionQueryParams OverlapQueryParams(SCENE_QUERY_STAT(SmartObjectTrace), OverlapParameters.bTraceComplex);
	OverlapQueryParams.bIgnoreTouches = true;

	if (VisContext.PreviewActor)
	{
		OverlapQueryParams.AddIgnoredActor(VisContext.PreviewActor);
	}
	
	if (UE::SmartObject::Annotations::TestCollidersOverlap(VisContext.World, Colliders, OverlapParameters, OverlapQueryParams))
	{
		Color = UE::SmartObject::Annotations::CapsuleCollisionColor;
	}

	if (!Colliders.IsEmpty())
	{
		for (const FSmartObjectAnnotationCollider& Collider : Colliders)
		{
			if (Collider.CollisionShape.IsCapsule())
			{
				DrawWireCapsule(VisContext.PDI, Collider.Location, Collider.Rotation.GetAxisX(), Collider.Rotation.GetAxisY(), Collider.Rotation.GetAxisZ(),
					Color, Collider.CollisionShape.GetCapsuleRadius(),  Collider.CollisionShape.GetCapsuleHalfHeight(), 12, SDPG_World, 1.0f, DepthBias, ScreenSpace);
			}
			else if (Collider.CollisionShape.IsBox())
			{
				DrawOrientedWireBox(VisContext.PDI, Collider.Location, Collider.Rotation.GetAxisX(), Collider.Rotation.GetAxisY(), Collider.Rotation.GetAxisZ(),
					Collider.CollisionShape.GetExtent(), Color, SDPG_World, 1.0f, DepthBias, ScreenSpace); 
					
			}
			else if (Collider.CollisionShape.IsSphere())
			{
				DrawWireSphere(VisContext.PDI, Collider.Location, Color, Collider.CollisionShape.GetSphereRadius(), 12, SDPG_World, 1.0f, DepthBias, ScreenSpace);
			}
		}
	}

	// Ground level
	DrawCircle(VisContext.PDI, SlotTransform.GetLocation(), SlotTransform.GetUnitAxis(EAxis::X), SlotTransform.GetUnitAxis(EAxis::Y), UE::SmartObject::Annotations::CapsuleFloorColor, CapsuleValue.Radius, 12, SDPG_World, 1.0f, DepthBias, ScreenSpace);
}

void FSmartObjectAnnotation_SlotUserCollision::DrawVisualizationHUD(FSmartObjectVisualizationContext& VisContext) const
{
}

#endif // WITH_EDITOR

#if WITH_GAMEPLAY_DEBUGGER
void FSmartObjectAnnotation_SlotUserCollision::CollectDataForGameplayDebugger(FSmartObjectAnnotationGameplayDebugContext& DebugContext) const
{
	const UWorld* World = DebugContext.Category.GetWorldFromReplicator();
	if (!World)
	{
		return;
	}

	TSubclassOf<USmartObjectSlotValidationFilter> ValidationFilterClass = nullptr;
	if (DebugContext.DebugActor)
	{
		// If user actor is present, try to query some data automatically from interfaces and components. 
		if (!ValidationFilterClass.Get())
		{
			if (const USmartObjectUserComponent* UserComponent = DebugContext.DebugActor->GetComponentByClass<USmartObjectUserComponent>())
			{
				ValidationFilterClass = UserComponent->GetValidationFilter();
			}
		}
	}

	const USmartObjectSlotValidationFilter* ValidationFilter = ValidationFilterClass.Get() ? ValidationFilterClass.GetDefaultObject() : GetDefault<USmartObjectSlotValidationFilter>();
	check(ValidationFilter);
	const FSmartObjectSlotValidationParams& ValidationParams = ValidationFilter->GetEntryValidationParams(); 

	FSmartObjectUserCapsuleParams CapsuleValue = Capsule;
	if (bUseUserCapsuleSize)
	{
		if (!DebugContext.DebugActor
			|| !ValidationParams.GetUserCapsuleForActor(*DebugContext.DebugActor, CapsuleValue))
		{
			return;
		}
	}
	
	FLinearColor Color = UE::SmartObject::Annotations::CapsuleColor;

	// Draw capsule
	TArray<FSmartObjectAnnotationCollider> Colliders;
	GetColliders(CapsuleValue, DebugContext.SlotTransform, Colliders);

	const FSmartObjectTraceParams& OverlapParameters = ValidationParams.GetTransitionTraceParameters();
	FCollisionQueryParams OverlapQueryParams(SCENE_QUERY_STAT(SmartObjectTrace), OverlapParameters.bTraceComplex);
	OverlapQueryParams.bIgnoreTouches = true;

	if (DebugContext.SmartObjectOwnerActor)
	{
		OverlapQueryParams.AddIgnoredActor(DebugContext.SmartObjectOwnerActor);
	}
	if (DebugContext.DebugActor)
	{
		OverlapQueryParams.AddIgnoredActor(DebugContext.DebugActor);
	}
	if (UE::SmartObject::Annotations::TestCollidersOverlap(*World, Colliders, OverlapParameters, OverlapQueryParams))
	{
		Color = UE::SmartObject::Annotations::CapsuleCollisionColor;
	}

	if (!Colliders.IsEmpty())
	{
		for (const FSmartObjectAnnotationCollider& Collider : Colliders)
		{
			if (Collider.CollisionShape.IsCapsule())
			{
				DebugContext.Category.AddShape(FGameplayDebuggerShape::MakeCapsule(Collider.Location, Collider.Rotation.Rotator(), Collider.CollisionShape.GetCapsuleRadius(), Collider.CollisionShape.GetCapsuleHalfHeight(), Color.ToFColor(/*bSRGB*/true)));
			}
			else if (Collider.CollisionShape.IsBox())
			{
				DebugContext.Category.AddShape(FGameplayDebuggerShape::MakeBox(Collider.Location, Collider.Rotation.Rotator(), Collider.CollisionShape.GetExtent(), Color.ToFColor(/*bSRGB*/true)));
			}
			else if (Collider.CollisionShape.IsSphere())
			{
				DebugContext.Category.AddShape(FGameplayDebuggerShape::MakePoint(Collider.Location, Collider.CollisionShape.GetSphereRadius(), Color.ToFColor(/*bSRGB*/true)));
			}
		}
	}

	// Ground level
	DebugContext.Category.AddShape(FGameplayDebuggerShape::MakeCircle(DebugContext.SlotTransform.GetLocation(), DebugContext.SlotTransform.GetUnitAxis(EAxis::X), DebugContext.SlotTransform.GetUnitAxis(EAxis::Y), CapsuleValue.Radius, UE::SmartObject::Annotations::CapsuleFloorColor));
}
#endif // WITH_GAMEPLAY_DEBUGGER

void FSmartObjectAnnotation_SlotUserCollision::GetColliders(const FSmartObjectUserCapsuleParams& UserCapsule, const FTransform& SlotTransform, TArray<FSmartObjectAnnotationCollider>& OutColliders) const
{
	const FSmartObjectUserCapsuleParams& CapsuleValue = bUseUserCapsuleSize ? UserCapsule : Capsule;
	OutColliders.Add(CapsuleValue.GetAsCollider(SlotTransform.GetLocation(), SlotTransform.GetRotation()));
}

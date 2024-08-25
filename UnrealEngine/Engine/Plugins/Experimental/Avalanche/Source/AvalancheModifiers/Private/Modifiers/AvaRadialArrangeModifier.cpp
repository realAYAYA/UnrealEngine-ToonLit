// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaRadialArrangeModifier.h"

#include "AvaDefs.h"
#include "AvaModifiersActorUtils.h"
#include "GameFramework/Actor.h"
#include "Shared/AvaTransformModifierShared.h"
#include "Shared/AvaVisibilityModifierShared.h"

#define LOCTEXT_NAMESPACE "AvaRadialArrangeModifier"

#if WITH_EDITOR
void UAvaRadialArrangeModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	static const TSet<FName> PropertiesName =
	{
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, Count),
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, Rings),
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, InnerRadius),
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, OuterRadius),
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, StartAngle),
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, EndAngle),
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, Arrangement),
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, bStartFromOuterRadius),
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, bOrient),
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, OrientAxis),
		GET_MEMBER_NAME_CHECKED(UAvaRadialArrangeModifier, bFlipOrient)
	};

	if (PropertiesName.Contains(MemberName))
	{
		MarkModifierDirty();
	}
}
#endif // WITH_EDITOR

void UAvaRadialArrangeModifier::SetCount(const int32 NewCount)
{
	Count = NewCount;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::SetRings(const int32 NewRings)
{
	Rings = NewRings;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::SetInnerRadius(const float NewInnerRadius)
{
	InnerRadius = NewInnerRadius;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::SetOuterRadius(const float NewOuterRadius)
{
	OuterRadius = NewOuterRadius;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::SetStartAngle(const float NewStartAngle)
{
	StartAngle = NewStartAngle;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::SetEndAngle(const float NewEndAngle)
{
	EndAngle = NewEndAngle;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::SetArrangement(const EAvaRadialArrangement NewArrangement)
{
	Arrangement = NewArrangement;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::SetStartFromOuterRadius(const bool bMewStartFromOuterRadius)
{
	bStartFromOuterRadius = bMewStartFromOuterRadius;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::SetOrient(const bool bNewOrient)
{
	bOrient = bNewOrient;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::SetOrientAxis(const EAvaAxis NewOrientAxis)
{
	OrientAxis = NewOrientAxis;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::SetFlipOrient(const bool bNewFlipOrient)
{
	bFlipOrient = bNewFlipOrient;

	MarkModifierDirty();
}

void UAvaRadialArrangeModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("RadialArrange"));
	InMetadata.SetCategory(TEXT("Layout"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Positions child actors in a 2D radial format"));
#endif
}

void UAvaRadialArrangeModifier::Apply()
{
	AActor* ModifyActor = GetModifiedActor();

	// Early exit if the modify actor is NOT being isolated. The outliner will manage the visibility for the actor and it's children.
	if (FAvaModifiersActorUtils::IsActorNotIsolated(ModifyActor))
	{
		Next();
		return;
	}
	
	const FAvaSceneTreeUpdateModifierExtension* SceneExtension = GetExtension<FAvaSceneTreeUpdateModifierExtension>();
	if (!SceneExtension)
	{
		Fail(LOCTEXT("InvalidSceneExtension", "Scene extension could not be found"));
		return;
	}

	TArray<TWeakObjectPtr<AActor>> AttachedActors = SceneExtension->GetDirectChildrenActor(ModifyActor);
	const int32 AttachedActorCount = AttachedActors.Num();
	const int32 TotalSlotCount = Count == -1 ? AttachedActorCount : FMath::Min(AttachedActorCount, Count);

	// open distance in degrees where children will be placed
	const float AngleOpenDistance = EndAngle - StartAngle;
	const float RadiusDistance = Rings > 1 ? OuterRadius - InnerRadius : 0.0f;
	const float RadiusDistancePerRing = RadiusDistance / Rings;

	auto CalculateRelativeOffset = [this, RadiusDistancePerRing](float InAngleInDegrees, const int32 InRingIndex) -> FVector
	{
		InAngleInDegrees = FRotator::NormalizeAxis(InAngleInDegrees);

		double SlotSin = 0.0f;
		double SlotCos = 0.0f;
		FMath::SinCos(&SlotSin, &SlotCos, FMath::DegreesToRadians(InAngleInDegrees));

		const float RingStartOffset = RadiusDistancePerRing * InRingIndex;
		const float ChildRadius = InnerRadius + RingStartOffset;

		return FVector(0.0f, ChildRadius * SlotCos, ChildRadius * SlotSin);
	};

	UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(true);
	UAvaVisibilityModifierShared* VisibilityShared = GetShared<UAvaVisibilityModifierShared>(true);

	TSet<TWeakObjectPtr<AActor>> NewChildrenActorsWeak;
	for (int32 ChildIndex = 0; ChildIndex < AttachedActorCount; ++ChildIndex)
	{
		AActor* AttachedActor = AttachedActors[ChildIndex].Get();

		if (!AttachedActor)
		{
			continue;
		}

		{
			// Track all new children actors
			TArray<AActor*> ChildrenActors { AttachedActor };
			AttachedActor->GetAttachedActors(ChildrenActors, false, true);
			for (AActor* ChildActor : ChildrenActors)
			{
				NewChildrenActorsWeak.Add(ChildActor);
			}
		}
		
		// No need to handle nested children actor, only direct children, visibility will propagate
		if (AttachedActor->GetAttachParentActor() != ModifyActor)
		{
			continue;
		}
		
		// Track this actor visibility state
		const bool bIsVisible = ChildIndex < TotalSlotCount;
		VisibilityShared->SetActorVisibility(this, AttachedActor, !bIsVisible, true);

		const int32 ChildIndexToUse = ChildIndex;
		float RingAngleStep = 0.0f;
		int32 RingIndex = 0;
		int32 RingColumnIndex = 0;
		float SlotAngle = 0.0f;

		switch (Arrangement)
		{
			/**
			 * Each radial ring will contain the same number of elements.
			 * The space between elements in the outer rings will be greater than the inner rings.
			 */
			case EAvaRadialArrangement::Monospace:
			{
				const int32 ChildrenPerRing = FMath::Max(1, FMath::CeilToInt32((float)TotalSlotCount / Rings));

				RingAngleStep = ChildrenPerRing > 1 ? (AngleOpenDistance / (ChildrenPerRing - 1)) : 0.0f;

				RingColumnIndex = ChildIndexToUse % ChildrenPerRing;
				RingIndex = FMath::FloorToInt32((float)ChildIndexToUse / ChildrenPerRing);

				SlotAngle = StartAngle
					+ (RingAngleStep * RingColumnIndex)
					+ 90; // adding 90 degrees to make 0 degrees face up instead of right

				break;
			}
			/**
			 * All elements in all radial rings have the same spacing between them.
			 * The number of elements in the inner rings will be greater than the outer rings.
			 */
			 // @TODO: back engineer this Viz Artist arrangement mode
			case EAvaRadialArrangement::Equal:
			{
				//const float RingAngleStep = AngleOpenDistance / ChildrenPerRing;
				//RingAngleStep = (AngleOpenDistance * Rings) / TotalSlotCount;
				RingAngleStep = (AngleOpenDistance / TotalSlotCount) * Rings;

				const int32 ChildrenPerRing = FMath::Max(1, FMath::CeilToInt32((float)TotalSlotCount / Rings));

				RingColumnIndex = ChildIndexToUse % ChildrenPerRing;
				RingIndex = FMath::FloorToInt32((float)ChildIndexToUse / ChildrenPerRing);

				SlotAngle = StartAngle
					+ (RingAngleStep * RingColumnIndex)
					+ 90; // adding 90 degrees to make 0 degrees face up instead of right

				break;
			}
		}

		if (bStartFromOuterRadius)
		{
			RingIndex = Rings - (RingIndex - 1);
		}

		const FVector RelativeOffset = CalculateRelativeOffset(SlotAngle, RingIndex);

		// Track this actor layout state
		LayoutShared->SaveActorState(this, AttachedActor);
		AttachedActor->SetActorRelativeLocation(RelativeOffset);

		if (bOrient)
		{
			const FVector EyePosition = AttachedActor->GetActorLocation();
			const FVector TargetPosition = ModifyActor->GetActorLocation();
			const FRotator NewRotation = FAvaModifiersActorUtils::FindLookAtRotation(EyePosition, TargetPosition, OrientAxis, bFlipOrient);

			AttachedActor->SetActorRotation(NewRotation);
		}
	}
	
	// Untrack previous actors that are not attached anymore
	const TSet<TWeakObjectPtr<AActor>> UntrackActors = ChildrenActorsWeak.Difference(NewChildrenActorsWeak);
	LayoutShared->RestoreActorsState(this, UntrackActors);
	VisibilityShared->RestoreActorsState(this, UntrackActors);

	ChildrenActorsWeak = NewChildrenActorsWeak;

	Next();
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaGridArrangeModifier.h"

#include "AvaDefs.h"
#include "AvaModifiersActorUtils.h"
#include "GameFramework/Actor.h"
#include "Shared/AvaTransformModifierShared.h"
#include "Shared/AvaVisibilityModifierShared.h"

#define LOCTEXT_NAMESPACE "AvaGridArrangeModifier"

#if WITH_EDITOR
void UAvaGridArrangeModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	static const FName CountPropertyName = GET_MEMBER_NAME_CHECKED(UAvaGridArrangeModifier, Count);
	static const FName SpreadPropertyName = GET_MEMBER_NAME_CHECKED(UAvaGridArrangeModifier, Spread);
	static const FName StartCornerPropertyName = GET_MEMBER_NAME_CHECKED(UAvaGridArrangeModifier, StartCorner);
	static const FName StartDirectionPropertyName = GET_MEMBER_NAME_CHECKED(UAvaGridArrangeModifier, StartDirection);

	if (MemberName == CountPropertyName
		|| MemberName == SpreadPropertyName
		|| MemberName == StartCornerPropertyName
		|| MemberName == StartDirectionPropertyName)
	{
		MarkModifierDirty();
	}
}
#endif

void UAvaGridArrangeModifier::SetCount(const FIntPoint& InCount)
{
	const FIntPoint NewCount = InCount.ComponentMax(FIntPoint(1, 1));

	if (Count == NewCount)
	{
		return;
	}

	Count = NewCount;
	MarkModifierDirty();
}

void UAvaGridArrangeModifier::SetSpread(const FVector2D& NewSpread)
{
	if (Spread == NewSpread)
	{
		return;
	}

	Spread = NewSpread;
	MarkModifierDirty();
}

void UAvaGridArrangeModifier::SetStartCorner(EAvaCorner2D InCorner)
{
	if (StartCorner == InCorner)
	{
		return;
	}

	StartCorner = InCorner;
	MarkModifierDirty();
}

void UAvaGridArrangeModifier::SetStartDirection(EAvaGridArrangeDirection InDirection)
{
	if (StartDirection == InDirection)
	{
		return;
	}

	StartDirection = InDirection;
	MarkModifierDirty();
}

void UAvaGridArrangeModifier::GetArrangementLayout(EAvaCorner2D& OutStartCorner, EAvaGridArrangeDirection& OutStartDirection) const
{
	OutStartCorner = StartCorner;
	OutStartDirection = StartDirection;
}

void UAvaGridArrangeModifier::SetArrangementLayout(const EAvaCorner2D NewStartCorner, const EAvaGridArrangeDirection NewStartDirection)
{
	StartCorner = NewStartCorner;
	StartDirection = NewStartDirection;

	MarkModifierDirty();
}

void UAvaGridArrangeModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("GridArrange"));
	InMetadata.SetCategory(TEXT("Layout"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Positions child actors in a 2D grid format"));
#endif
}

void UAvaGridArrangeModifier::Apply()
{
	AActor* const ModifyActor = GetModifiedActor();

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

	if (Count.X < 1 || Count.Y < 1)
	{
		Fail(LOCTEXT("InvalidGridCount", "Count must be greater than 0"));
		return;
	}

	const TArray<TWeakObjectPtr<AActor>> AttachedActors = SceneExtension->GetDirectChildrenActor(ModifyActor);
	const int32 TotalSlotCount = Count.X * Count.Y;
	const int32 AttachedActorCount = AttachedActors.Num();

	auto GetGridX = [this](const int32 ChildIndex)
	{
		return StartDirection == EAvaGridArrangeDirection::Horizontal
			? ChildIndex % Count.X
			: ChildIndex / Count.X;
	};
	auto GetGridY = [this](const int32 ChildIndex)
	{
		return StartDirection == EAvaGridArrangeDirection::Horizontal
			? ChildIndex / Count.X
			: ChildIndex % Count.X;
	};

	auto GetReversedGridX = [this, GetGridX](const int32 ChildIndex) { return (Count.X - 1) - GetGridX(ChildIndex); };
	auto GetReversedGridY = [this, GetGridY](const int32 ChildIndex) { return (Count.Y - 1) - GetGridY(ChildIndex); };

	UAvaVisibilityModifierShared* VisibilityShared = GetShared<UAvaVisibilityModifierShared>(true);
	UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(true);

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

		FVector RelativeOffset = FVector::ZeroVector;
		switch (StartCorner)
		{
			case EAvaCorner2D::TopLeft:
				RelativeOffset.Y = GetGridX(ChildIndex);
				RelativeOffset.Z = GetGridY(ChildIndex);
			break;
			case EAvaCorner2D::TopRight:
				RelativeOffset.Y = -GetReversedGridX(ChildIndex);
				RelativeOffset.Z = GetGridY(ChildIndex);
			break;
			case EAvaCorner2D::BottomLeft:
				RelativeOffset.Y = GetGridX(ChildIndex);
				RelativeOffset.Z = -GetReversedGridY(ChildIndex);
			break;
			case EAvaCorner2D::BottomRight:
				RelativeOffset.Y = -GetReversedGridX(ChildIndex);
				RelativeOffset.Z = -GetReversedGridY(ChildIndex);
			break;
		}

		RelativeOffset.Y *= Spread.X;
		RelativeOffset.Z *= -Spread.Y;

		// Track this actor layout state
		LayoutShared->SaveActorState(this, AttachedActor);
		AttachedActor->SetActorRelativeLocation(RelativeOffset);
	}

	// Untrack previous actors that are not attached anymore
	const TSet<TWeakObjectPtr<AActor>> UntrackActors = ChildrenActorsWeak.Difference(NewChildrenActorsWeak);
	LayoutShared->RestoreActorsState(this, UntrackActors);
	VisibilityShared->RestoreActorsState(this, UntrackActors);

	ChildrenActorsWeak = NewChildrenActorsWeak;

	Next();
}

#undef LOCTEXT_NAMESPACE

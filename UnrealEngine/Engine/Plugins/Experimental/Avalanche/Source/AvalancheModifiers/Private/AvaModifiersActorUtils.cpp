// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaModifiersActorUtils.h"

#include "AvaDefs.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "AvaOutlinerUtils.h"
#include "IAvaOutliner.h"
#endif

FBox FAvaModifiersActorUtils::GetActorsBounds(const TSet<TWeakObjectPtr<AActor>>& InActors, const FTransform& InReferenceTransform, bool bInSkipHidden)
{
	FBox ActorsBounds = FBox(EForceInit::ForceInit);
	ActorsBounds.IsValid = 0;
	
	FVector OrientedVerts[8];
	
	FTransform AccumulatedTransform = InReferenceTransform;

	for (const TWeakObjectPtr<AActor>& ActorWeak : InActors)
	{
		const AActor* Actor = ActorWeak.Get();
		
		if (!Actor)
		{
			continue;
		}
		
		if (bInSkipHidden && !IsActorVisible(Actor))
		{
			continue;
		}

		FBox ActorBounds = FAvaModifiersActorUtils::GetActorBounds(Actor);

		if (ActorBounds.IsValid > 0)
		{
			FTransform ActorTransform = Actor->GetTransform();
			ActorTransform.SetScale3D(FVector::OneVector);

			const FOrientedBox OrientedBox = FAvaModifiersActorUtils::GetOrientedBox(ActorBounds, ActorTransform);

			OrientedBox.CalcVertices(OrientedVerts);

			for (const FVector& OrientedVert : OrientedVerts)
			{
				ActorsBounds += AccumulatedTransform.InverseTransformPositionNoScale(OrientedVert);
			}

			ActorsBounds.IsValid = 1;
		}
	}
	
	AccumulatedTransform.SetScale3D(FVector::OneVector);
	
	return ActorsBounds.TransformBy(AccumulatedTransform);
}

FBox FAvaModifiersActorUtils::GetActorsBounds(AActor* InActor, bool bInIncludeChildren, bool bInSkipHidden)
{
	if (!InActor)
	{
		return FBox();
	}
	
	TSet<TWeakObjectPtr<AActor>> AttachedModifyActors {InActor};

	if (bInIncludeChildren)
	{
		TArray<AActor*> AttachedActors;
		InActor->GetAttachedActors(AttachedActors, false, true);
		Algo::Transform(AttachedActors, AttachedModifyActors, [](AActor* InAttachedActor)
		{
			return InAttachedActor;
		});
	}
	
	return FAvaModifiersActorUtils::GetActorsBounds(AttachedModifyActors, InActor->GetActorTransform(), bInSkipHidden);
}

FBox FAvaModifiersActorUtils::GetActorBounds(const AActor* InActor)
{	
	FBox Box(ForceInit);
	Box.IsValid = 0;
	
	if (!InActor || !InActor->GetRootComponent())
	{
		return Box;
	}
	
	FTransform ActorToWorld = InActor->GetTransform();
	ActorToWorld.SetScale3D(FVector::OneVector);
	const FTransform WorldToActor = ActorToWorld.Inverse();

	InActor->ForEachComponent<UPrimitiveComponent>(true, [&WorldToActor, &Box](const UPrimitiveComponent* InPrimitiveComponent)
	{
		if (!IsValid(InPrimitiveComponent))
		{
			return;
		}
		
#if WITH_EDITOR
		// Ignore Visualization Components, but don't consider them as failed components.
		if (InPrimitiveComponent->IsVisualizationComponent())
		{
			return;
		}
#endif
		
		const FTransform ComponentToActor = InPrimitiveComponent->GetComponentTransform() * WorldToActor;
		const FBox ComponentBox = InPrimitiveComponent->CalcBounds(ComponentToActor).GetBox();

		Box += ComponentBox;
		Box.IsValid = 1;
	});

	return Box;
}

FOrientedBox FAvaModifiersActorUtils::GetOrientedBox(const FBox& InLocalBox, const FTransform& InWorldTransform)
{
	FOrientedBox OutOrientedBox;
	OutOrientedBox.Center = InWorldTransform.TransformPosition(InLocalBox.GetCenter());

	OutOrientedBox.AxisX = InWorldTransform.TransformVector(FVector::UnitX());
	OutOrientedBox.AxisY = InWorldTransform.TransformVector(FVector::UnitY());
	OutOrientedBox.AxisZ = InWorldTransform.TransformVector(FVector::UnitZ());

	OutOrientedBox.ExtentX = (InLocalBox.Max.X - InLocalBox.Min.X) / 2.f;
	OutOrientedBox.ExtentY = (InLocalBox.Max.Y - InLocalBox.Min.Y) / 2.f;
	OutOrientedBox.ExtentZ = (InLocalBox.Max.Z - InLocalBox.Min.Z) / 2.f;
	return OutOrientedBox;
}

FVector FAvaModifiersActorUtils::GetVectorAxis(int32 InAxis)
{
	FVector FollowedAxisVector = FVector::ZeroVector;
	
	if (EnumHasAnyFlags(static_cast<EAvaModifiersAxis>(InAxis), EAvaModifiersAxis::X))
	{
		FollowedAxisVector.X = 1;
	}
	
	if (EnumHasAnyFlags(static_cast<EAvaModifiersAxis>(InAxis), EAvaModifiersAxis::Y))
	{
		FollowedAxisVector.Y = 1;
	}
	
	if (EnumHasAnyFlags(static_cast<EAvaModifiersAxis>(InAxis), EAvaModifiersAxis::Z))
	{
		FollowedAxisVector.Z = 1;
	}

	return FollowedAxisVector;
}

bool FAvaModifiersActorUtils::IsAxisVectorEquals(const FVector& InVectorA, const FVector& InVectorB, int32 InCompareAxis)
{
	// Compare InVectorA and InVectorB but only component of InCompareAxis
	const FVector FollowedAxisVector = FAvaModifiersActorUtils::GetVectorAxis(InCompareAxis);
	if ((InVectorA * FollowedAxisVector).Equals(InVectorB * FollowedAxisVector, 0.1))
	{
		return true;
	}

	return false;
}

bool FAvaModifiersActorUtils::IsActorNotIsolated(const AActor* InActor)
{
	if (!InActor)
	{
		return false;
	}
	
#if WITH_EDITOR
	bool bIsIsolatingActors = false;
	TArray<TWeakObjectPtr<const AActor>> IsolatedActors;

	TSharedPtr<IAvaOutliner> AvaOutliner = FAvaOutlinerUtils::EditorGetOutliner(InActor->GetWorld());
	if (AvaOutliner.IsValid())
	{
		bIsIsolatingActors = FAvaOutlinerUtils::EditorActorIsolationInfo(AvaOutliner, IsolatedActors);
	}

	if (bIsIsolatingActors && !IsolatedActors.Contains(InActor))
	{
		return true;
	}
#endif

	return false;
}

FRotator FAvaModifiersActorUtils::FindLookAtRotation(const FVector& InEyePosition, const FVector& InTargetPosition, const EAvaAxis InAxis, const bool bInFlipAxis)
{
	auto LookAtDirection = [&InEyePosition, &InTargetPosition, bInFlipAxis]() -> FVector
	{
		FVector OutDirection = bInFlipAxis ? (InEyePosition - InTargetPosition) : (InTargetPosition - InEyePosition);

		OutDirection.Normalize();
		if (OutDirection.SizeSquared() < 0.5f)
		{
			// Assert possible if OutDirection is not normalized.
			OutDirection = FVector(1, 0, 0);
		}

		return OutDirection;
	};

	FMatrix NewRotation = FRotationMatrix::Identity;

	switch (InAxis)
	{
	case EAvaAxis::Horizontal:
		NewRotation = FRotationMatrix::MakeFromX(LookAtDirection());
		break;
	case EAvaAxis::Vertical:
		NewRotation = FRotationMatrix::MakeFromY(LookAtDirection());
		break;
	case EAvaAxis::Depth:
		NewRotation = FRotationMatrix::MakeFromZ(LookAtDirection());
		break;
	}

	return NewRotation.Rotator();
}

bool FAvaModifiersActorUtils::IsActorVisible(const AActor* InActor)
{
	if (!InActor)
	{
		return false;
	}

	const bool bIsActorVisible = !InActor->IsHidden();

#if WITH_EDITOR
	const bool bIsActorVisibleInEditor = !InActor->IsTemporarilyHiddenInEditor();
#else
	constexpr bool bIsActorVisibleInEditor = true;
#endif

	// If we don't have a root component, consider this true to skip it in final condition
	const bool bIsRootComponentVisible = !InActor->GetRootComponent() || InActor->GetRootComponent()->IsVisible();

	return bIsActorVisible && bIsActorVisibleInEditor && bIsRootComponentVisible;
}

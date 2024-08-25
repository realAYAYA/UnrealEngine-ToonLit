// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaLookAtModifier.h"

#include "AvaDefs.h"
#include "Extensions/AvaTransformUpdateModifierExtension.h"
#include "GameFramework/Actor.h"
#include "Misc/ITransaction.h"
#include "Shared/AvaTransformModifierShared.h"

#define LOCTEXT_NAMESPACE "AvaLookAtModifier"

void UAvaLookAtModifier::OnModifierAdded(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierAdded(InReason);

	AddExtension<FAvaTransformUpdateModifierExtension>(this);

	if (FAvaSceneTreeUpdateModifierExtension* SceneExtension = GetExtension<FAvaSceneTreeUpdateModifierExtension>())
	{
		SceneExtension->TrackSceneTree(0, &ReferenceActor);
	}
}

void UAvaLookAtModifier::OnModifierEnabled(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierEnabled(InReason);
	
	if (UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(true))
	{
		LayoutShared->SaveActorState(this, GetModifiedActor());
	}
}

void UAvaLookAtModifier::OnModifierDisabled(EActorModifierCoreDisableReason InReason)
{
	Super::OnModifierDisabled(InReason);

	if (UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(false))
	{
		LayoutShared->RestoreActorState(this, GetModifiedActor());
	}
}

void UAvaLookAtModifier::Apply()
{
	AActor* const ModifyActor = GetModifiedActor();

	const AActor* LookActor = ReferenceActor.ReferenceActorWeak.Get();
	if (!IsValid(LookActor))
	{
		Next();
		return;
	}

	auto LookAtDirection = [this, ModifyActor, LookActor]() -> FVector
	{
		FVector OutDirection = bFlipAxis
			? LookActor->GetActorLocation() - ModifyActor->GetActorLocation()
			: ModifyActor->GetActorLocation() - LookActor->GetActorLocation();

		OutDirection.Normalize();
		if (OutDirection.SizeSquared() < 0.5f)
		{
			// Assert possible if OutDirection is not normalized.
			OutDirection = FVector(1, 0, 0);
		}

		return OutDirection;
	};

	auto NormalOffset = [this, ModifyActor](const FQuat& InNewRotation) -> FQuat
	{
		FQuat OutNormal = bFlipAxis
			? InNewRotation - ModifyActor->GetActorRotation().Quaternion()
			: ModifyActor->GetActorRotation().Quaternion() - InNewRotation;

		OutNormal.Normalize();

		return OutNormal;
	};

	FMatrix NewRotation = FRotationMatrix::Identity;
		
	switch (Axis)
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

	ModifyActor->SetActorRotation(NewRotation.ToQuat());

	Next();
}

void UAvaLookAtModifier::OnModifiedActorTransformed()
{
	MarkModifierDirty();
}

void UAvaLookAtModifier::PostLoad()
{
	if (!bDeprecatedPropertiesMigrated)
	{
		ReferenceActor.ReferenceContainer = EAvaReferenceContainer::Other;
		ReferenceActor.ReferenceActorWeak = ReferenceActorWeak;
		ReferenceActor.bSkipHiddenActors = false;

		bDeprecatedPropertiesMigrated = true;
	}
	
	Super::PostLoad();
}

#if WITH_EDITOR
void UAvaLookAtModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();
	
	static const FName ReferenceActorPropertyName = GET_MEMBER_NAME_CHECKED(UAvaLookAtModifier, ReferenceActor);
	static const FName FlipAxisPropertyName = GET_MEMBER_NAME_CHECKED(UAvaLookAtModifier, bFlipAxis);
	static const FName AxisPropertyName = GET_MEMBER_NAME_CHECKED(UAvaLookAtModifier, Axis);
	
	if (MemberName == ReferenceActorPropertyName)
	{
		OnReferenceActorChanged();
	}
	else if (MemberName == AxisPropertyName
		|| MemberName == FlipAxisPropertyName)
	{
		MarkModifierDirty();
	}
}

void UAvaLookAtModifier::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	OnReferenceActorChanged();
	
	Super::PostTransacted(TransactionEvent);
}
#endif // WITH_EDITOR

void UAvaLookAtModifier::SetReferenceActor(const FAvaSceneTreeActor& InReferenceActor)
{
	if (ReferenceActor == InReferenceActor)
	{
		return;
	}
	
	ReferenceActor = InReferenceActor;
	OnReferenceActorChanged();
}

void UAvaLookAtModifier::OnReferenceActorChanged()
{
	if (ReferenceActor.ReferenceActorWeak.Get() == GetModifiedActor())
	{
		ReferenceActor.ReferenceActorWeak = nullptr;
	}

	if (const FAvaSceneTreeUpdateModifierExtension* SceneExtension = GetExtension<FAvaSceneTreeUpdateModifierExtension>())
	{
		SceneExtension->CheckTrackedActorUpdate(0);
	}
}

void UAvaLookAtModifier::SetAxis(const EAvaAxis NewAxis)
{
	Axis = NewAxis;

	MarkModifierDirty();
}

void UAvaLookAtModifier::SetFlipAxis(const bool bNewFlipAxis)
{
	bFlipAxis = bNewFlipAxis;

	MarkModifierDirty();
}

void UAvaLookAtModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("LookAt"));
	InMetadata.SetCategory(TEXT("Layout"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Rotates an actor to face another actor"));
#endif
}

void UAvaLookAtModifier::OnTransformUpdated(AActor* InActor, bool bInParentMoved)
{
	if (InActor && InActor == ReferenceActor.ReferenceActorWeak.Get())
	{
		MarkModifierDirty();
	}
}

void UAvaLookAtModifier::OnSceneTreeTrackedActorChanged(int32 InIdx, AActor* InPreviousActor, AActor* InNewActor)
{
	Super::OnSceneTreeTrackedActorChanged(InIdx, InPreviousActor, InNewActor);
	
	if (InNewActor == GetModifiedActor())
	{
		OnReferenceActorChanged();
		return;
	}

	// Untrack reference actor
	if (FAvaTransformUpdateModifierExtension* TransformExtension = GetExtension<FAvaTransformUpdateModifierExtension>())
	{
		TransformExtension->UntrackActor(InPreviousActor);
		TransformExtension->TrackActor(InNewActor, true);
	}

	MarkModifierDirty();
}

#undef LOCTEXT_NAMESPACE

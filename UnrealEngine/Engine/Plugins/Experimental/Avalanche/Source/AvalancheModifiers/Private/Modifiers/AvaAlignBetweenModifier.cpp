// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaAlignBetweenModifier.h"

#include "GameFramework/Actor.h"
#include "Misc/ITransaction.h"
#include "Shared/AvaTransformModifierShared.h"

#define LOCTEXT_NAMESPACE "AvaAlignBetweenModifier"

void UAvaAlignBetweenModifier::OnModifierAdded(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierAdded(InReason);

	AddExtension<FAvaTransformUpdateModifierExtension>(this);
}

void UAvaAlignBetweenModifier::OnModifierEnabled(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierEnabled(InReason);

	// Save actor layout state
	if (UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(true))
	{
		LayoutShared->SaveActorState(this, GetModifiedActor());
	}

	SetTransformExtensionReferenceActors();
}

void UAvaAlignBetweenModifier::OnModifierDisabled(EActorModifierCoreDisableReason InReason)
{
	Super::OnModifierDisabled(InReason);

	// Restore actor layout state
	if (UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(false))
	{
		LayoutShared->RestoreActorState(this, GetModifiedActor());
	}
}

void UAvaAlignBetweenModifier::Apply()
{
	AActor* const ModifyActor = GetModifiedActor();
	
	if (ReferenceActors.IsEmpty())
	{
		Next();
		return;
	}

	const TSet<FAvaAlignBetweenWeightedActor> WeightedActors = GetEnabledReferenceActors();

	float TotalWeight = 0.0f;
	for (const FAvaAlignBetweenWeightedActor& WeightedActor : WeightedActors)
	{
		if (WeightedActor.ActorWeak.IsValid())
		{
			TotalWeight += WeightedActor.Weight;	
		}
	}

	if (TotalWeight > 0.0f)
	{
		FVector AverageWeightedLocation = FVector::ZeroVector;
		
		for (const FAvaAlignBetweenWeightedActor& WeightedActor : WeightedActors)
		{
			if (const AActor* Actor = WeightedActor.ActorWeak.Get())
			{
				AverageWeightedLocation += Actor->GetActorLocation() * (WeightedActor.Weight / TotalWeight);
			}
		}
		
		ModifyActor->SetActorLocation(AverageWeightedLocation);
	}

	Next();
}

void UAvaAlignBetweenModifier::OnModifiedActorTransformed()
{
	Super::OnModifiedActorTransformed();

	MarkModifierDirty();
}

#if WITH_EDITOR
void UAvaAlignBetweenModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	static const FName ReferenceActorsPropertyName = GET_MEMBER_NAME_CHECKED(UAvaAlignBetweenModifier, ReferenceActors);
	
	if (MemberName == ReferenceActorsPropertyName)
	{
		OnReferenceActorsChanged();
	}
}

void UAvaAlignBetweenModifier::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	OnReferenceActorsChanged();
	
	Super::PostTransacted(TransactionEvent);
}
#endif // WITH_EDITOR

TSet<AActor*> UAvaAlignBetweenModifier::GetActors(const bool bEnabledOnly) const
{
	TSet<AActor*> OutActors;

	for (const FAvaAlignBetweenWeightedActor& WeightedActor : ReferenceActors)
	{
		if (!bEnabledOnly|| (bEnabledOnly && WeightedActor.bEnabled))
		{
			OutActors.Add(WeightedActor.ActorWeak.Get());
		}
	}

	return OutActors;
}

void UAvaAlignBetweenModifier::SetReferenceActors(const TSet<FAvaAlignBetweenWeightedActor>& NewReferenceActors)
{
	ReferenceActors = NewReferenceActors;
	OnReferenceActorsChanged();
}

void UAvaAlignBetweenModifier::AddReferenceActor(const FAvaAlignBetweenWeightedActor& ReferenceActor)
{
	const AActor* const ModifyActor = GetModifiedActor();
	if (!IsValid(ModifyActor))
	{
		return;
	}

	if (!ReferenceActor.ActorWeak.IsValid() || ReferenceActor.ActorWeak == ModifyActor)
	{
		return;
	}

	bool bAlreadyInSet = false;
	ReferenceActors.Add(ReferenceActor, &bAlreadyInSet);

	if (!bAlreadyInSet)
	{
		SetTransformExtensionReferenceActors();
		
		MarkModifierDirty();
	}
}

bool UAvaAlignBetweenModifier::RemoveReferenceActor(AActor* const Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	if (ReferenceActors.Remove(FAvaAlignBetweenWeightedActor(Actor)) > 0)
	{
		MarkModifierDirty();
		
		return true;
	}

	return false;
}

bool UAvaAlignBetweenModifier::FindReferenceActor(AActor* InActor, FAvaAlignBetweenWeightedActor& OutReferenceActor) const
{
	if (!IsValid(InActor))
	{
		return false;
	}

	if (const FAvaAlignBetweenWeightedActor* OutReference = ReferenceActors.Find(FAvaAlignBetweenWeightedActor(InActor)))
	{
		OutReferenceActor = *OutReference;
		return true;
	}

	return false;
}

void UAvaAlignBetweenModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("AlignBetween"));
	InMetadata.SetCategory(TEXT("Layout"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Positions an actor between a group of weighted actors"));
#endif
}

void UAvaAlignBetweenModifier::OnTransformUpdated(AActor* InActor, bool bInParentMoved)
{
	// Check actor is reference actor enabled
	FAvaAlignBetweenWeightedActor ReferenceActor;
	FindReferenceActor(InActor, ReferenceActor);

	if (ReferenceActor.IsValid() && ReferenceActor.bEnabled && ReferenceActor.Weight > 0)
	{
		MarkModifierDirty();
	}
}

void UAvaAlignBetweenModifier::SetTransformExtensionReferenceActors()
{
	if (FAvaTransformUpdateModifierExtension* TransformExtension = GetExtension<FAvaTransformUpdateModifierExtension>())
	{
		TSet<TWeakObjectPtr<AActor>> ExtensionActors;
		Algo::Transform(ReferenceActors, ExtensionActors, [](const FAvaAlignBetweenWeightedActor& InActor)->AActor*
		{
			return InActor.ActorWeak.Get();
		});
		TransformExtension->TrackActors(ExtensionActors, true);
	}
}

void UAvaAlignBetweenModifier::OnReferenceActorsChanged()
{
	// Make sure the modifying actor is not part of the array.
	AActor* const ModifyActor = GetModifiedActor();
	if (!IsValid(ModifyActor))
	{
		return;
	}
	
	ReferenceActors.Remove(FAvaAlignBetweenWeightedActor(ModifyActor));

	SetTransformExtensionReferenceActors();

	MarkModifierDirty();
}

TSet<FAvaAlignBetweenWeightedActor> UAvaAlignBetweenModifier::GetEnabledReferenceActors() const
{
	TSet<FAvaAlignBetweenWeightedActor> OutReferences;
	OutReferences.Reserve(ReferenceActors.Num());

	for (const FAvaAlignBetweenWeightedActor& WeightedActor : ReferenceActors)
	{
		if (WeightedActor.IsValid())
		{
			OutReferences.Add(WeightedActor);
		}
	}

	return OutReferences;
}

#undef LOCTEXT_NAMESPACE

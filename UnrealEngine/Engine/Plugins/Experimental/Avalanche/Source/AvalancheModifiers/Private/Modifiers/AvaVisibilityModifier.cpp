// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaVisibilityModifier.h"

#include "AvaModifiersActorUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/ITransaction.h"
#include "Shared/AvaVisibilityModifierShared.h"

#define LOCTEXT_NAMESPACE "AvaVisibilityModifier"

void UAvaVisibilityModifier::Apply()
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
	
	const TArray<TWeakObjectPtr<AActor>> AttachedActors = SceneExtension->GetDirectChildrenActor(ModifyActor);
	UAvaVisibilityModifierShared* VisibilityShared = GetShared<UAvaVisibilityModifierShared>(true);

	// Top most modifier in tree has priority over this one if it is hiding current one
	bool bIsNestedVisibilityModifier = false;
	if (const UAvaVisibilityModifier* VisibilityModifier = GetFirstModifierAbove(ModifyActor))
	{
		// This actor is not visible and hidden by root visibility modifier do not proceed
		if (VisibilityModifier->IsChildActorHidden(ModifyActor))
		{
			bIsNestedVisibilityModifier = true;
		}
	}
	
	TSet<TWeakObjectPtr<AActor>> NewChildrenActorsWeak;
	for (int32 ChildIndex = 0; ChildIndex < AttachedActors.Num(); ++ChildIndex)
	{
		AActor* AttachedActor = AttachedActors[ChildIndex].Get();

		if (!AttachedActor)
		{
			continue;
		}
		
		// No need to handle nested children actor, only direct children, visibility will propagate
		if (AttachedActor->GetAttachParentActor() != ModifyActor)
		{
			continue;
		}

		bool bHideActor = false;
		
		if (!bTreatAsRange)
		{
			bHideActor = ChildIndex == Index;
		}
		else
		{
			bHideActor = ChildIndex <= Index;
		}
		
		bHideActor = bInvertVisibility ? bHideActor : !bHideActor;
		DirectChildrenActorsWeak.Add(AttachedActor, bHideActor);
		
		TArray<AActor*> AttachedChildActors {AttachedActor};
		AttachedActor->GetAttachedActors(AttachedChildActors, false, true);
		for (AActor* AttachedChildActor : AttachedChildActors)
		{
			// If we are not hiding, then the top nearest modifier in the tree takes precedence
			if (!bHideActor)
			{
				UAvaVisibilityModifier* VisibilityModifier = GetFirstModifierAbove(AttachedChildActor);

				if (VisibilityModifier && VisibilityModifier != this)
				{
					VisibilityModifier->MarkModifierDirty();
					continue;
				}
			}
			
			if (!bIsNestedVisibilityModifier)
			{
				VisibilityShared->SetActorVisibility(this, AttachedChildActor, bHideActor, false);
			}
			
			NewChildrenActorsWeak.Add(AttachedActor);
		}
	}

	// Untrack previous actors that are not attached anymore
	VisibilityShared->RestoreActorsState(this, ChildrenActorsWeak.Difference(NewChildrenActorsWeak));

	ChildrenActorsWeak = NewChildrenActorsWeak;

	Next();
}

#if WITH_EDITOR
void UAvaVisibilityModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetMemberPropertyName();

	static const FName IndexPropertyName = GET_MEMBER_NAME_CHECKED(UAvaVisibilityModifier, Index);
	static const FName InvertVisibilityPropertyName = GET_MEMBER_NAME_CHECKED(UAvaVisibilityModifier, bInvertVisibility);
	static const FName TreatAsRangePropertyName = GET_MEMBER_NAME_CHECKED(UAvaVisibilityModifier, bTreatAsRange);
	
	if (PropertyName == IndexPropertyName
		|| PropertyName == TreatAsRangePropertyName
		|| PropertyName == InvertVisibilityPropertyName)
	{
		MarkModifierDirty();
	}
}
#endif // WITH_EDITOR

void UAvaVisibilityModifier::SetTreatAsRange(const bool bInTreatAsRange)
{
	if (bTreatAsRange == bInTreatAsRange)
	{
		return;
	}

	bTreatAsRange = bInTreatAsRange;
	MarkModifierDirty();
}

void UAvaVisibilityModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("Visibility"));
	InMetadata.SetCategory(TEXT("Rendering"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Controls the visibility of a range of child actors by index"));
#endif
}

void UAvaVisibilityModifier::OnRenderStateUpdated(AActor* InActor, UActorComponent* InComponent)
{
	Super::OnRenderStateUpdated(InActor, InComponent);

	const AActor* ActorModified = GetModifiedActor();

	if (!IsValid(ActorModified)
		|| !InActor->IsAttachedTo(ActorModified))
	{
		return;
	}
	
	MarkModifierDirty();
}

void UAvaVisibilityModifier::SetInvertVisibility(const bool bNewInvertVisibility)
{
	bInvertVisibility = bNewInvertVisibility;

	MarkModifierDirty();
}

void UAvaVisibilityModifier::SetIndex(int32 InIndex)
{
	if (Index == InIndex)
	{
		return;
	}

	if (Index < 0)
	{
		return;
	}

	Index = InIndex;
	MarkModifierDirty();
}

bool UAvaVisibilityModifier::IsChildActorHidden(AActor* InActor) const
{
	const AActor* ActorModified = GetModifiedActor();

	if (InActor->IsAttachedTo(ActorModified))
	{
		while (InActor->GetAttachParentActor() != ActorModified)
		{
			InActor = InActor->GetAttachParentActor();
		}

		if (!DirectChildrenActorsWeak.Contains(InActor))
		{
			return false;
		}

		return *DirectChildrenActorsWeak.Find(InActor);
	}
	
	return false;
}

UAvaVisibilityModifier* UAvaVisibilityModifier::GetFirstModifierAbove(AActor* InActor)
{
	UAvaVisibilityModifierShared* VisibilityShared = GetShared<UAvaVisibilityModifierShared>(false);
	UAvaVisibilityModifier* FirstModifierAbove = nullptr;
	
	if (!InActor || !VisibilityShared)
	{
		return FirstModifierAbove;
	}
	
	if (FAvaVisibilitySharedActorState* ActorState = VisibilityShared->FindActorState(InActor))
	{
		for (const FAvaVisibilitySharedModifierState& ModifierState : ActorState->ModifierStates)
		{
			UAvaBaseModifier* Modifier = ModifierState.ModifierWeak.Get();
			if (!Modifier || Modifier->GetModifiedActor() != InActor->GetAttachParentActor())
			{
				continue;
			}

			if (UAvaVisibilityModifier* VisibilityModifier = Cast<UAvaVisibilityModifier>(Modifier))
			{
				FirstModifierAbove = VisibilityModifier;
				break;
			}
		}
	}

	if (!FirstModifierAbove)
	{
		FirstModifierAbove = GetFirstModifierAbove(InActor->GetAttachParentActor());
	}

	return FirstModifierAbove;
}

UAvaVisibilityModifier* UAvaVisibilityModifier::GetLastModifierAbove(AActor* InActor)
{
	UAvaVisibilityModifier* LastModifierAbove = nullptr;

	while (UAvaVisibilityModifier* VisibilityModifier = GetFirstModifierAbove(InActor))
	{
		LastModifierAbove = VisibilityModifier;
		InActor = LastModifierAbove->GetModifiedActor();
	}

	return LastModifierAbove;
}

AActor* UAvaVisibilityModifier::GetDirectChildren(AActor* InParentActor, AActor* InChildActor)
{
	if (!InParentActor || !InChildActor)
	{
		return nullptr;
	}
	
	if (InChildActor && InChildActor->GetAttachParentActor() == InParentActor)
	{
		return InChildActor;
	}
	
	return GetDirectChildren(InParentActor, InChildActor->GetAttachParentActor());
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Shared/AvaTransformModifierShared.h"

#include "Modifiers/AvaBaseModifier.h"
#include "GameFramework/Actor.h"

void FAvaTransformSharedModifierState::Save(const AActor* InActor)
{
	if (const UAvaBaseModifier* Modifier = ModifierWeak.Get())
	{
		if (InActor)
		{
			ActorTransform = InActor->GetActorTransform();
		}
	}
}

void FAvaTransformSharedModifierState::Restore(AActor* InActor, EAvaTransformSharedModifier InRestoreState) const
{
	if (const UAvaBaseModifier* Modifier = ModifierWeak.Get())
	{
		if (InActor)
		{
			FTransform RestoreTransform = ActorTransform;
			const FTransform& CurrentActorTransform = InActor->GetActorTransform();
			
			if (!EnumHasAnyFlags(InRestoreState, EAvaTransformSharedModifier::Location))
			{
				RestoreTransform.SetLocation(CurrentActorTransform.GetLocation());
			}

			if (!EnumHasAnyFlags(InRestoreState, EAvaTransformSharedModifier::Rotation))
			{
				RestoreTransform.SetRotation(CurrentActorTransform.GetRotation());
			}

			if (!EnumHasAnyFlags(InRestoreState, EAvaTransformSharedModifier::Scale))
			{
				RestoreTransform.SetScale3D(CurrentActorTransform.GetScale3D());
			}

			if (!CurrentActorTransform.Equals(RestoreTransform))
			{
				InActor->SetActorTransform(RestoreTransform);
			}
		}
	}
}

void FAvaTransformSharedActorState::Save()
{
	if (const AActor* Actor = ActorWeak.Get())
	{
		ActorTransform = Actor->GetActorTransform();
	}
}

void FAvaTransformSharedActorState::Restore(EAvaTransformSharedModifier InRestoreState) const
{
	if (AActor* Actor = ActorWeak.Get())
	{
		FTransform RestoreTransform = ActorTransform;
		const FTransform& CurrentActorTransform = Actor->GetActorTransform();
			
		if (!EnumHasAnyFlags(InRestoreState, EAvaTransformSharedModifier::Location))
		{
			RestoreTransform.SetLocation(CurrentActorTransform.GetLocation());
		}

		if (!EnumHasAnyFlags(InRestoreState, EAvaTransformSharedModifier::Rotation))
		{
			RestoreTransform.SetRotation(CurrentActorTransform.GetRotation());
		}

		if (!EnumHasAnyFlags(InRestoreState, EAvaTransformSharedModifier::Scale))
		{
			RestoreTransform.SetScale3D(CurrentActorTransform.GetScale3D());
		}

		if (!CurrentActorTransform.Equals(RestoreTransform))
		{
			Actor->SetActorTransform(RestoreTransform);
		}
	}
}

void UAvaTransformModifierShared::SaveActorState(UAvaBaseModifier* InModifierContext, AActor* InActor)
{
	if (!IsValid(InActor))
	{
		return;
	}

	FAvaTransformSharedActorState& ActorState = ActorStates.FindOrAdd(FAvaTransformSharedActorState(InActor));

	if (ActorState.ModifierStates.IsEmpty())
	{
		ActorState.Save();
	}

	bool bAlreadyInSet = false;
	FAvaTransformSharedModifierState& ModifierState = ActorState.ModifierStates.FindOrAdd(FAvaTransformSharedModifierState(InModifierContext), &bAlreadyInSet);

	if (!bAlreadyInSet)
	{
		ModifierState.Save(InActor);
		ActorState.ModifierStates.Add(ModifierState);
	}
}

void UAvaTransformModifierShared::RestoreActorState(UAvaBaseModifier* InModifierContext, AActor* InActor, EAvaTransformSharedModifier InRestoreTransform)
{
	if (!IsValid(InActor))
	{
		return;
	}

	FAvaTransformSharedActorState* ActorState = ActorStates.Find(FAvaTransformSharedActorState(InActor));
	if (!ActorState)
	{
		return;
	}

	const FAvaTransformSharedModifierState* ActorModifierState = ActorState->ModifierStates.Find(FAvaTransformSharedModifierState(InModifierContext));
	if (!ActorModifierState)
	{
		return;
	}

	// restore modifier state and remove it
	ActorModifierState->Restore(InActor, InRestoreTransform);
	ActorState->ModifierStates.Remove(*ActorModifierState);

	// Restore original actor state and remove it
	if (ActorState->ModifierStates.IsEmpty())
	{
		ActorState->Restore(InRestoreTransform);
		ActorStates.Remove(*ActorState);
	}
}

FAvaTransformSharedActorState* UAvaTransformModifierShared::FindActorState(AActor* InActor)
{
	if (!IsValid(InActor))
	{
		return nullptr;
	}

	return ActorStates.Find(FAvaTransformSharedActorState(InActor));
}

TSet<FAvaTransformSharedActorState*> UAvaTransformModifierShared::FindActorsState(UAvaBaseModifier* InModifierContext)
{
	TSet<FAvaTransformSharedActorState*> ModifierActorStates;
	
	for (FAvaTransformSharedActorState& ActorState : ActorStates)
	{
		if (ActorState.ModifierStates.Contains(FAvaTransformSharedModifierState(InModifierContext)))
		{
			ModifierActorStates.Add(&ActorState);
		}
	}
	
	return ModifierActorStates;
}

void UAvaTransformModifierShared::RestoreActorsState(UAvaBaseModifier* InModifierContext, const TSet<AActor*>* InActors, EAvaTransformSharedModifier InRestoreTransform)
{
	const FAvaTransformSharedModifierState SearchModifierState(InModifierContext);
	TSet<AActor*> LinkedModifierActors;
	TSet<UActorModifierCoreBase*> LinkedActorModifiers;
	
	for (const FAvaTransformSharedActorState& ActorState : ActorStates)
	{
		AActor* Actor = ActorState.ActorWeak.Get();
		if (!Actor)
		{
			continue;
		}

		if (!ActorState.ModifierStates.Contains(SearchModifierState))
		{
			continue;
		}

		if (InActors && !InActors->Contains(Actor))
		{
			continue;
		}
		
		// Collect actors affected by modifier
		LinkedModifierActors.Add(Actor);

		// Collect linked actor modifiers
		for (const FAvaTransformSharedModifierState& ModifierState : ActorState.ModifierStates)
		{
			if (UActorModifierCoreBase* Modifier = ModifierState.ModifierWeak.Get())
			{
				LinkedActorModifiers.Add(Modifier);
			}
		}
	}

	// Locking state to prevent from updating when restoring state
	// When destroyed : Unlocking state of modifier
	FActorModifierCoreScopedLock ModifiersLock(LinkedActorModifiers);

	// Restore actor state
	for (AActor* Actor : LinkedModifierActors)
	{
		RestoreActorState(InModifierContext, Actor, InRestoreTransform);
	}
}

void UAvaTransformModifierShared::RestoreActorsState(UAvaBaseModifier* InModifierContext, const TSet<TWeakObjectPtr<AActor>>& InActors, EAvaTransformSharedModifier InRestoreTransform)
{
	TSet<AActor*> Actors;
	Algo::Transform(InActors, Actors, [](const TWeakObjectPtr<AActor>& InActor)->AActor*{ return InActor.Get(); });

	RestoreActorsState(InModifierContext, &Actors, InRestoreTransform);
}

bool UAvaTransformModifierShared::IsActorStateSaved(UAvaBaseModifier* InModifierContext, AActor* InActor)
{
	if (const FAvaTransformSharedActorState* ActorState = FindActorState(InActor))
	{
		return ActorState->ModifierStates.Contains(FAvaTransformSharedModifierState(InModifierContext));
	}
	
	return false;
}

bool UAvaTransformModifierShared::IsActorsStateSaved(UAvaBaseModifier* InModifierContext)
{
	const FAvaTransformSharedModifierState ModifierState(InModifierContext);
	
	for (const FAvaTransformSharedActorState& ActorState : ActorStates)
	{
		if (ActorState.ModifierStates.Contains(ModifierState))
		{
			return true;
		}
	}
	
	return false;
}

void UAvaTransformModifierShared::PostLoad()
{
	Super::PostLoad();

	// Remove invalid items when loading
	for (FAvaTransformSharedActorState& ActorState : ActorStates)
	{
		ActorState.ModifierStates.Remove(FAvaTransformSharedModifierState(nullptr));
	}
}

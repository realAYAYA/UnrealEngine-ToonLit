// Copyright Epic Games, Inc. All Rights Reserved.

#include "Shared/AvaVisibilityModifierShared.h"

void FAvaVisibilitySharedModifierState::Save(const AActor* InActor)
{
	if (const UAvaBaseModifier* Modifier = ModifierWeak.Get())
	{
		if (InActor)
		{
#if WITH_EDITOR
			bActorHiddenInEditor = InActor->IsTemporarilyHiddenInEditor();
#endif
			bActorHiddenInGame = InActor->IsHidden();
		}
	}
}

void FAvaVisibilitySharedModifierState::Restore(AActor* InActor) const
{
	if (const UAvaBaseModifier* Modifier = ModifierWeak.Get())
	{
		if (InActor)
		{
#if WITH_EDITOR
			InActor->SetIsTemporarilyHiddenInEditor(bActorHiddenInEditor);
#endif
			InActor->SetActorHiddenInGame(bActorHiddenInGame);
		}
	}
}

void FAvaVisibilitySharedActorState::Save()
{
	if (const AActor* Actor = ActorWeak.Get())
	{
#if WITH_EDITOR
		bActorHiddenInEditor = Actor->IsTemporarilyHiddenInEditor();
#endif
		bActorHiddenInGame = Actor->IsHidden();
	}
}

void FAvaVisibilitySharedActorState::Restore() const
{
	if (AActor* Actor = ActorWeak.Get())
	{
#if WITH_EDITOR
		Actor->SetIsTemporarilyHiddenInEditor(bActorHiddenInEditor);
#endif
		Actor->SetActorHiddenInGame(bActorHiddenInGame);
	}
}

void UAvaVisibilityModifierShared::SaveActorState(UAvaBaseModifier* InModifierContext, AActor* InActor)
{
	if (!IsValid(InActor))
	{
		return;
	}

	FAvaVisibilitySharedActorState& ActorState = ActorStates.FindOrAdd(FAvaVisibilitySharedActorState(InActor));

	if (ActorState.ModifierStates.IsEmpty())
	{
		ActorState.Save();
	}
	
	FAvaVisibilitySharedModifierState ModifierState(InModifierContext);
	if (!ActorState.ModifierStates.Contains(ModifierState))
	{
		ModifierState.Save(InActor);
		ActorState.ModifierStates.Add(InModifierContext);
	}
}

void UAvaVisibilityModifierShared::RestoreActorState(UAvaBaseModifier* InModifierContext, AActor* InActor)
{
	if (!IsValid(InActor))
	{
		return;
	}

	if (FAvaVisibilitySharedActorState* ActorState = ActorStates.Find(FAvaVisibilitySharedActorState(InActor)))
	{
		if (const FAvaVisibilitySharedModifierState* ActorModifierState = ActorState->ModifierStates.Find(FAvaVisibilitySharedModifierState(InModifierContext)))
		{
			// restore modifier state and remove it
			ActorModifierState->Restore(InActor);
			ActorState->ModifierStates.Remove(*ActorModifierState);

			// Restore original actor state and remove it
			if (ActorState->ModifierStates.IsEmpty())
			{
				ActorState->Restore();
				ActorStates.Remove(*ActorState);
			}
		}
	}
}

FAvaVisibilitySharedActorState* UAvaVisibilityModifierShared::FindActorState(AActor* InActor)
{
	if (!IsValid(InActor))
	{
		return nullptr;
	}

	return ActorStates.Find(FAvaVisibilitySharedActorState(InActor));
}

void UAvaVisibilityModifierShared::SetActorVisibility(UAvaBaseModifier* InModifierContext, AActor* InActor, bool bInHidden, bool bInRecurse, EAvaVisibilityActor InActorVisibility)
{
	if (!IsValid(InActor))
	{
		return;
	}

	TArray<AActor*> Actors { InActor };

	if (bInRecurse)
	{
		constexpr bool bResetArray = false;
		constexpr bool bRecursivelyIncludeAttachedActors = true;
		InActor->GetAttachedActors(Actors, bResetArray, bRecursivelyIncludeAttachedActors);
	}

	SetActorsVisibility(InModifierContext, Actors, bInHidden, InActorVisibility);
}

void UAvaVisibilityModifierShared::SetActorsVisibility(UAvaBaseModifier* InModifierContext, TArray<AActor*> InActors, bool bInHidden, EAvaVisibilityActor InActorVisibility)
{
	for (AActor* Actor : InActors)
	{
		if (!Actor)
		{
			continue;
		}
		
		SaveActorState(InModifierContext, Actor);

#if WITH_EDITOR
		if (EnumHasAnyFlags(InActorVisibility, EAvaVisibilityActor::Editor))
		{
			if (Actor->IsTemporarilyHiddenInEditor() != bInHidden)
			{
				Actor->SetIsTemporarilyHiddenInEditor(bInHidden);
			}
		}
#endif
		
		if (EnumHasAnyFlags(InActorVisibility, EAvaVisibilityActor::Game))
		{
			if (Actor->IsHidden() != bInHidden)
			{
				Actor->SetActorHiddenInGame(bInHidden);
			}
		}
	}
}

void UAvaVisibilityModifierShared::RestoreActorsState(UAvaBaseModifier* InModifierContext, const TSet<AActor*>* InActors)
{
	const FAvaVisibilitySharedModifierState SearchModifierState(InModifierContext);
	TSet<AActor*> LinkedModifierActors;
	TSet<UActorModifierCoreBase*> LinkedActorModifiers;
	
	for (const FAvaVisibilitySharedActorState& ActorState : ActorStates)
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
		for (const FAvaVisibilitySharedModifierState& ModifierState : ActorState.ModifierStates)
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
		RestoreActorState(InModifierContext, Actor);
	}
}

void UAvaVisibilityModifierShared::RestoreActorsState(UAvaBaseModifier* InModifierContext, const TSet<TWeakObjectPtr<AActor>>& InActors)
{
	TSet<AActor*> Actors;
	Algo::Transform(InActors, Actors, [](const TWeakObjectPtr<AActor>& InActor)->AActor*{ return InActor.Get(); });

	RestoreActorsState(InModifierContext, &Actors);
}

bool UAvaVisibilityModifierShared::IsActorStateSaved(UAvaBaseModifier* InModifierContext, AActor* InActor)
{
	if (const FAvaVisibilitySharedActorState* ActorState = FindActorState(InActor))
	{
		return ActorState->ModifierStates.Contains(FAvaVisibilitySharedModifierState(InModifierContext));
	}
	
	return false;
}

bool UAvaVisibilityModifierShared::IsActorsStateSaved(UAvaBaseModifier* InModifierContext)
{
	const FAvaVisibilitySharedModifierState ModifierState(InModifierContext);
	
	for (const FAvaVisibilitySharedActorState& ActorState : ActorStates)
	{
		if (ActorState.ModifierStates.Contains(ModifierState))
		{
			return true;
		}
	}
	
	return false;
}

void UAvaVisibilityModifierShared::PostLoad()
{
	Super::PostLoad();

	// Remove invalid items when loading
	for (FAvaVisibilitySharedActorState& ActorState : ActorStates)
	{
		ActorState.ModifierStates.Remove(FAvaVisibilitySharedModifierState(nullptr));
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Extensions/AvaRenderStateUpdateModifierExtension.h"

#include "AvaModifiersActorUtils.h"
#include "Containers/Ticker.h"
#include "Modifiers/ActorModifierCoreBase.h"

FAvaRenderStateUpdateModifierExtension::FAvaRenderStateUpdateModifierExtension(IAvaRenderStateUpdateHandler* InExtensionHandler)
	: ExtensionHandlerWeak(InExtensionHandler)
{
	check(InExtensionHandler);
}

void FAvaRenderStateUpdateModifierExtension::TrackActorVisibility(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	if (IsActorVisibilityTracked(InActor))
	{
		return;
	}

	TrackedActorsVisibility.Add(InActor, FAvaModifiersActorUtils::IsActorVisible(InActor));
}

void FAvaRenderStateUpdateModifierExtension::UntrackActorVisibility(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	TrackedActorsVisibility.Remove(InActor);
}

bool FAvaRenderStateUpdateModifierExtension::IsActorVisibilityTracked(AActor* InActor) const
{
	return InActor && TrackedActorsVisibility.Contains(InActor);
}

void FAvaRenderStateUpdateModifierExtension::SetTrackedActorsVisibility(const TSet<TWeakObjectPtr<AActor>>& InActors)
{
	// Remove unwanted actors, keep value for tracked ones
	for (TMap<TWeakObjectPtr<AActor>, bool>::TIterator It(TrackedActorsVisibility); It; ++It)
	{
		if (!InActors.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}

	// Track value of wanted actors, will not overwrite already tracked one
	for (const TWeakObjectPtr<AActor>& Actor : InActors)
	{
		TrackActorVisibility(Actor.Get());
	}
}

void FAvaRenderStateUpdateModifierExtension::SetTrackedActorVisibility(AActor* InActor, bool bInIncludeChildren)
{
	if (!InActor)
	{
		return;
	}

	TSet<TWeakObjectPtr<AActor>> TrackActors {InActor};

	if (bInIncludeChildren)
	{
		TArray<AActor*> AttachedActors;
		InActor->GetAttachedActors(AttachedActors, false, true);
		Algo::Transform(AttachedActors, TrackActors, [](AActor* InAttachedActor)
		{
			return InAttachedActor;
		});
	}

	SetTrackedActorsVisibility(TrackActors);
}

void FAvaRenderStateUpdateModifierExtension::OnExtensionEnabled(EActorModifierCoreEnableReason InReason)
{
	BindDelegate();
}

void FAvaRenderStateUpdateModifierExtension::OnExtensionDisabled(EActorModifierCoreDisableReason InReason)
{
	UnbindDelegate();
}

void FAvaRenderStateUpdateModifierExtension::OnRenderStateDirty(UActorComponent& InComponent)
{
	const AActor* ModifierActor = GetModifierActor();
	AActor* ActorDirty = InComponent.GetOwner();

	if (!ActorDirty || !ModifierActor)
	{
		return;
	}

	const UActorModifierCoreBase* Modifier = GetModifier();
	if (!Modifier || !Modifier->IsModifierEnabled())
	{
		return;
	}

	if (ActorDirty->GetLevel() != ModifierActor->GetLevel())
	{
		return;
	}

	if (IAvaRenderStateUpdateHandler* HandlerInterface = ExtensionHandlerWeak.Get())
	{
		HandlerInterface->OnRenderStateUpdated(ActorDirty, &InComponent);
	}

	if (!TrackedActorsVisibility.Contains(ActorDirty))
	{
		return;
	}

	// Execute on next tick otherwise visibility data might not be up to date
	TWeakObjectPtr<AActor> ActorDirtyWeak(ActorDirty);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSPLambda(this, [this, ActorDirtyWeak](float InDeltaSeconds)->bool
	{
		AActor* ActorDirty = ActorDirtyWeak.Get();

		if (!ActorDirty)
		{
			return false;
		}

		bool* bOldVisibility = TrackedActorsVisibility.Find(ActorDirty);

		if (!bOldVisibility)
		{
			return false;
		}

		const bool bNewVisibility = FAvaModifiersActorUtils::IsActorVisible(ActorDirty);
		bool& bOldVisibilityRef = *bOldVisibility;

		const bool bVisibilityChanged = bOldVisibilityRef != bNewVisibility;

		// Set new visibility
		bOldVisibilityRef = bNewVisibility;

		IAvaRenderStateUpdateHandler* HandlerInterface = ExtensionHandlerWeak.Get();
		if (bVisibilityChanged && HandlerInterface)
		{
			HandlerInterface->OnActorVisibilityChanged(ActorDirty);
		}

		return false;
	}));
}

void FAvaRenderStateUpdateModifierExtension::BindDelegate()
{
	UActorComponent::MarkRenderStateDirtyEvent.RemoveAll(this);
	UActorComponent::MarkRenderStateDirtyEvent.AddSP(this, &FAvaRenderStateUpdateModifierExtension::OnRenderStateDirty);
}

void FAvaRenderStateUpdateModifierExtension::UnbindDelegate()
{
	UActorComponent::MarkRenderStateDirtyEvent.RemoveAll(this);
}

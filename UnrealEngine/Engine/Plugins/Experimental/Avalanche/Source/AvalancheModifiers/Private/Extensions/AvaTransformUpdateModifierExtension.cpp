// Copyright Epic Games, Inc. All Rights Reserved.

#include "Extensions/AvaTransformUpdateModifierExtension.h"

#include "Modifiers/ActorModifierCoreBase.h"

FAvaTransformUpdateModifierExtension::FAvaTransformUpdateModifierExtension(IAvaTransformUpdateHandler* InExtensionInterface)
	: ExtensionHandlerWeak(InExtensionInterface)
{
	check(InExtensionInterface);
}

void FAvaTransformUpdateModifierExtension::TrackActor(AActor* InActor, bool bInReset)
{
	if (!InActor || !InActor->GetRootComponent())
	{
		return;
	}

	if (bInReset)
	{
		const TSet<TWeakObjectPtr<AActor>> NewSet {InActor};
		UntrackActors(TrackedActors.Difference(NewSet));
	}
	
	if (TrackedActors.Contains(InActor))
	{
		return;
	}

	if (IsExtensionEnabled())
	{
		BindDelegate(InActor);
	}

	TrackedActors.Add(InActor);
}

void FAvaTransformUpdateModifierExtension::UntrackActor(AActor* InActor)
{
	if (!InActor || !InActor->GetRootComponent())
	{
		return;
	}
	
	if (!TrackedActors.Contains(InActor))
	{
		return;
	}

	UnbindDelegate(InActor);

	TrackedActors.Remove(InActor);
}

void FAvaTransformUpdateModifierExtension::TrackActors(const TSet<TWeakObjectPtr<AActor>>& InActors, bool bInReset)
{
	if (bInReset)
	{
		UntrackActors(TrackedActors.Difference(InActors));	
	}
	
	for (const TWeakObjectPtr<AActor>& InActor : InActors)
	{
		TrackActor(InActor.Get(), false);
	}
}

void FAvaTransformUpdateModifierExtension::UntrackActors(const TSet<TWeakObjectPtr<AActor>>& InActors)
{
	for (const TWeakObjectPtr<AActor>& InActor : InActors)
	{
		UntrackActor(InActor.Get());
	}
}

void FAvaTransformUpdateModifierExtension::OnExtensionEnabled(EActorModifierCoreEnableReason InReason)
{
	for (TWeakObjectPtr<AActor>& TrackedActor : TrackedActors)
	{
		BindDelegate(TrackedActor.Get());
	}
}

void FAvaTransformUpdateModifierExtension::OnExtensionDisabled(EActorModifierCoreDisableReason InReason)
{
	for (TWeakObjectPtr<AActor>& TrackedActor : TrackedActors)
	{
		UnbindDelegate(TrackedActor.Get());
	}
}

void FAvaTransformUpdateModifierExtension::OnTransformUpdated(USceneComponent* InComponent, EUpdateTransformFlags InFlags, ETeleportType InType)
{
	if (!InComponent)
	{
		return;
	}

	AActor* ActorTransformed = InComponent->GetOwner();
	if (!ActorTransformed)
	{
		return;
	}

	const UActorModifierCoreBase* Modifier = GetModifier();
	if (!Modifier || !Modifier->IsModifierEnabled())
	{
		return;
	}
	
	if (IAvaTransformUpdateHandler* HandlerInterface = ExtensionHandlerWeak.Get())
	{
		HandlerInterface->OnTransformUpdated(ActorTransformed, InFlags == EUpdateTransformFlags::PropagateFromParent);
	}
}

void FAvaTransformUpdateModifierExtension::BindDelegate(const AActor* InActor)
{
	if (!InActor)
	{
		return;
	}
	
	if (USceneComponent* SceneComponent = InActor->GetRootComponent())
	{
		SceneComponent->TransformUpdated.RemoveAll(this);
		SceneComponent->TransformUpdated.AddSP(this, &FAvaTransformUpdateModifierExtension::OnTransformUpdated);
	}
}

void FAvaTransformUpdateModifierExtension::UnbindDelegate(const AActor* InActor)
{
	if (!InActor)
	{
		return;
	}
	
	if (USceneComponent* SceneComponent = InActor->GetRootComponent())
	{
		SceneComponent->TransformUpdated.RemoveAll(this);
	}
}

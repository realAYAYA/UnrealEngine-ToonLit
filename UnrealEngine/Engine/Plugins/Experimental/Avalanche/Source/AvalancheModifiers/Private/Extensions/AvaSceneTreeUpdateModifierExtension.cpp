// Copyright Epic Games, Inc. All Rights Reserved.

#include "Extensions/AvaSceneTreeUpdateModifierExtension.h"
#include "AvaActorUtils.h"
#include "AvaSceneTree.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "IAvaSceneInterface.h"

#if WITH_EDITOR
#include "AvaOutlinerSubsystem.h"
#include "AvaOutlinerUtils.h"
#include "Engine/World.h"
#include "IAvaOutliner.h"
#endif

FAvaSceneTreeUpdateModifierExtension::FAvaSceneTreeUpdateModifierExtension(IAvaSceneTreeUpdateHandler* InExtensionHandler)
	: ExtensionHandlerWeak(InExtensionHandler)
{
	check(InExtensionHandler);
}

void FAvaSceneTreeUpdateModifierExtension::TrackSceneTree(int32 InTrackedActorIdx, FAvaSceneTreeActor* InTrackedActor)
{
	if (!InTrackedActor)
	{
		return;
	}

	InTrackedActor->LocalActorWeak = GetModifierActor();
	InTrackedActor->ReferenceActorsWeak.Empty();
	InTrackedActor->ReferenceActorChildrenWeak.Empty();
	InTrackedActor->ReferenceActorParentsWeak.Empty();
	InTrackedActor->ReferenceActorDirectChildrenWeak.Empty();

	TrackedActors.Add(InTrackedActorIdx, InTrackedActor);

	CheckTrackedActorUpdate(InTrackedActorIdx);
}

void FAvaSceneTreeUpdateModifierExtension::UntrackSceneTree(int32 InTrackedActorIdx)
{
	if (!TrackedActors.Contains(InTrackedActorIdx))
	{
		return;
	}

	TrackedActors.Remove(InTrackedActorIdx);
}

FAvaSceneTreeActor* FAvaSceneTreeUpdateModifierExtension::GetTrackedActor(int32 InTrackedActorIdx) const
{
	if (FAvaSceneTreeActor* const* TrackedActor = TrackedActors.Find(InTrackedActorIdx))
	{
		return *TrackedActor;
	}

	return nullptr;
}

void FAvaSceneTreeUpdateModifierExtension::CheckTrackedActorsUpdate() const
{
	// Container could change while in range iteration
	TArray<int32> TrackedKeys;
	TrackedActors.GenerateKeyArray(TrackedKeys);

	for (const int32 Key : TrackedKeys)
	{
		CheckTrackedActorUpdate(Key);
	}
}

void FAvaSceneTreeUpdateModifierExtension::OnExtensionEnabled(EActorModifierCoreEnableReason InReason)
{
	const UWorld* World = GetModifierWorld();
	if (!IsValid(World))
	{
		return;
	}

	// When actor are destroyed in world
	World->RemoveOnActorDestroyededHandler(WorldActorDestroyedDelegate);
	WorldActorDestroyedDelegate = World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateSP(this, &FAvaSceneTreeUpdateModifierExtension::OnWorldActorDestroyed));

	// Used to detect visibility changes in siblings
	USceneComponent::MarkRenderStateDirtyEvent.RemoveAll(this);
	USceneComponent::MarkRenderStateDirtyEvent.AddSP(this, &FAvaSceneTreeUpdateModifierExtension::OnRenderStateDirty);

#if WITH_EDITOR
	if (UAvaOutlinerSubsystem* const OutlinerSubsystem = World->GetSubsystem<UAvaOutlinerSubsystem>())
	{
		UAvaOutlinerSubsystem::FActorHierarchyChanged& ActorHierarchyChanged = OutlinerSubsystem->OnActorHierarchyChanged();

		ActorHierarchyChanged.RemoveAll(this);
		ActorHierarchyChanged.AddSP(this, &FAvaSceneTreeUpdateModifierExtension::OnActorHierarchyChanged);

		if (const TSharedPtr<IAvaOutliner> Outliner = OutlinerSubsystem->GetOutliner())
		{
			IAvaOutliner::FOnOutlinerLoaded& OnOutlinerLoaded = Outliner->GetOnOutlinerLoaded();
			OnOutlinerLoaded.RemoveAll(this);
			OnOutlinerLoaded.AddSP(this, &FAvaSceneTreeUpdateModifierExtension::OnOutlinerLoaded);
		}
	}
#endif

	CheckTrackedActorsUpdate();
}

void FAvaSceneTreeUpdateModifierExtension::OnExtensionDisabled(EActorModifierCoreDisableReason InReason)
{
	USceneComponent::MarkRenderStateDirtyEvent.RemoveAll(this);

	const UWorld* World = GetModifierWorld();
	if (!IsValid(World))
	{
		return;
	}

	World->RemoveOnActorDestroyededHandler(WorldActorDestroyedDelegate);

#if WITH_EDITOR
	if (UAvaOutlinerSubsystem* const OutlinerSubsystem = World->GetSubsystem<UAvaOutlinerSubsystem>())
	{
		OutlinerSubsystem->OnActorHierarchyChanged().RemoveAll(this);
		if (const TSharedPtr<IAvaOutliner> Outliner = OutlinerSubsystem->GetOutliner())
		{
			Outliner->GetOnOutlinerLoaded().RemoveAll(this);
		}
	}
#endif
}

#if WITH_EDITOR
void FAvaSceneTreeUpdateModifierExtension::OnOutlinerLoaded()
{
	CheckTrackedActorsUpdate();
}

void FAvaSceneTreeUpdateModifierExtension::OnActorHierarchyChanged(AActor* InActor, const AActor* InParentActor, EAvaOutlinerHierarchyChangeType InChangeType)
{
	if (!InActor)
	{
		return;
	}

	// Container could change while in range iteration
	TArray<int32> TrackedKeys;
	TrackedActors.GenerateKeyArray(TrackedKeys);

	for (const int32 Key : TrackedKeys)
	{
		const FAvaSceneTreeActor* TrackedActor = GetTrackedActor(Key);
		if (!TrackedActor || !TrackedActor->LocalActorWeak.IsValid())
		{
			continue;
		}

		CheckTrackedActorUpdate(Key);

		if (TrackedActor)
		{
			const AActor* ReferenceActor = TrackedActor->ReferenceActorWeak.Get();
			IAvaSceneTreeUpdateHandler* HandlerInterface = ExtensionHandlerWeak.Get();

			if (HandlerInterface
				&& IsValid(ReferenceActor)
				&& TrackedActor->ReferenceContainer == EAvaReferenceContainer::Other
				&& (InActor == ReferenceActor || InActor->IsAttachedTo(ReferenceActor)))
			{
				HandlerInterface->OnSceneTreeTrackedActorRearranged(Key, InActor);
			}
		}
	}
}
#endif

void FAvaSceneTreeUpdateModifierExtension::OnRenderStateDirty(UActorComponent& InComponent)
{
	AActor* OwningActor = InComponent.GetOwner();
	if (!IsValid(OwningActor))
	{
		return;
	}

	if (GetModifierWorld() != OwningActor->GetWorld())
	{
		return;
	}

	// Container could change while in range iteration
	TArray<int32> TrackedKeys;
	TrackedActors.GenerateKeyArray(TrackedKeys);

	for (const int32 Key : TrackedKeys)
	{
		const FAvaSceneTreeActor* TrackedActor = GetTrackedActor(Key);
		if (!TrackedActor || !TrackedActor->LocalActorWeak.IsValid())
		{
			continue;
		}

		const AActor* ReferenceActor = TrackedActor->ReferenceActorWeak.Get();

		const bool bIsReferenceActor = ReferenceActor == OwningActor || TrackedActor->ReferenceActorsWeak.Contains(OwningActor);
		const bool bIsReferenceActorChild = ReferenceActor && OwningActor->IsAttachedTo(ReferenceActor);
		const bool bIsReferenceActorSibling = ReferenceActor && OwningActor->GetAttachParentActor() == ReferenceActor->GetAttachParentActor();
		const bool bIsReferenceActorParent = ReferenceActor && ReferenceActor->IsAttachedTo(OwningActor);

		if (!bIsReferenceActor && !bIsReferenceActorChild && !bIsReferenceActorSibling && !bIsReferenceActorParent)
		{
			continue;
		}

		CheckTrackedActorUpdate(Key);
	}
}

void FAvaSceneTreeUpdateModifierExtension::OnWorldActorDestroyed(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	// Delay check from one tick to make sure actor is no longer attached
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSPLambda(this, [this](float InDeltaSeconds)->bool
	{
		CheckTrackedActorsUpdate();
		return false;
	}));
}

bool FAvaSceneTreeUpdateModifierExtension::IsSameActorArray(const TArray<TWeakObjectPtr<AActor>>& InPreviousActorWeak, const TArray<TWeakObjectPtr<AActor>>& InNewActorWeak) const
{
	if (InPreviousActorWeak.Num() != InNewActorWeak.Num())
	{
		return false;
	}

	for (int32 Idx = 0; Idx < InPreviousActorWeak.Num(); Idx++)
	{
		if (InPreviousActorWeak[Idx].Get() != InNewActorWeak[Idx].Get())
		{
			return false;
		}
	}

	return true;
}

TArray<TWeakObjectPtr<AActor>> FAvaSceneTreeUpdateModifierExtension::GetReferenceActors(const FAvaSceneTreeActor* InTrackedActor) const
{
	TArray<TWeakObjectPtr<AActor>> ReferenceActors;

	if (!InTrackedActor)
	{
		return ReferenceActors;
	}

	AActor* LocalActor = InTrackedActor->LocalActorWeak.Get();
	if (!IsValid(LocalActor))
	{
		return ReferenceActors;
	}

	if (InTrackedActor->ReferenceContainer == EAvaReferenceContainer::Other)
	{
		if (AActor* ReferenceActor = InTrackedActor->ReferenceActorWeak.Get())
		{
			ReferenceActors.Add(ReferenceActor);
		}

		return ReferenceActors;
	}

	AActor* ContextActor = LocalActor;
	while (AActor* NewReferenceActor = FAvaActorUtils::ActorFromReferenceContainer(ContextActor, InTrackedActor->ReferenceContainer, false))
	{
		// Take only siblings
		if (NewReferenceActor->GetAttachParentActor() != LocalActor->GetAttachParentActor())
		{
			break;
		}

		ReferenceActors.Add(NewReferenceActor);
		ContextActor = NewReferenceActor;

		if (InTrackedActor->bSkipHiddenActors
				&& (NewReferenceActor->IsHidden()
#if WITH_EDITOR
				|| NewReferenceActor->IsTemporarilyHiddenInEditor()
#endif
				))
		{
			continue;
		}

		break;
	}

	return ReferenceActors;
}

TSet<TWeakObjectPtr<AActor>> FAvaSceneTreeUpdateModifierExtension::GetChildrenActorsRecursive(const AActor* InActor) const
{
	TSet<TWeakObjectPtr<AActor>> ChildrenWeak;

	if (InActor)
	{
		TArray<AActor*> ReferenceAttachedActors;
		InActor->GetAttachedActors(ReferenceAttachedActors, false, true);
		Algo::Transform(ReferenceAttachedActors, ChildrenWeak, [](AActor* InAttachedActor)->TWeakObjectPtr<AActor>
		{
			return InAttachedActor;
		});
	}

	return ChildrenWeak;
}

TArray<TWeakObjectPtr<AActor>> FAvaSceneTreeUpdateModifierExtension::GetDirectChildrenActor(AActor* InActor) const
{
	TArray<TWeakObjectPtr<AActor>> DirectChildrenWeak;

	if (!IsValid(InActor))
	{
		return DirectChildrenWeak;
	}

	bool bIsOutlinerAttachedActors = false;
	TArray<AActor*> DirectChildren;

#if WITH_EDITOR
	if (const UWorld* const World = InActor->GetWorld())
	{
		if (const UAvaOutlinerSubsystem* const OutlinerSubsystem = World->GetSubsystem<UAvaOutlinerSubsystem>())
		{
			if (const TSharedPtr<IAvaOutliner> AvaOutliner = OutlinerSubsystem->GetOutliner())
			{
				DirectChildren = FAvaOutlinerUtils::EditorOutlinerChildActors(AvaOutliner, InActor);
				bIsOutlinerAttachedActors = true;
			}
		}
	}
#endif

	if (!bIsOutlinerAttachedActors)
	{
		if (const IAvaSceneInterface* SceneInterface = FAvaActorUtils::GetSceneInterfaceFromActor(InActor))
		{
			const FAvaSceneTree& SceneTree = SceneInterface->GetSceneTree();
			DirectChildren = SceneTree.GetChildActors(InActor);
			bIsOutlinerAttachedActors = true;
		}
	}

	// Fallback for unsupported world
	if (!bIsOutlinerAttachedActors)
	{
		 InActor->GetAttachedActors(DirectChildren, false, false);
	}

	Algo::Transform(DirectChildren, DirectChildrenWeak, [](AActor* InActor)->TWeakObjectPtr<AActor>
	{
		return InActor;
	});

	return DirectChildrenWeak;
}

TArray<TWeakObjectPtr<AActor>> FAvaSceneTreeUpdateModifierExtension::GetParentActors(const AActor* InActor) const
{
	TArray<TWeakObjectPtr<AActor>> ActorParentsWeak;

	while (InActor && InActor->GetAttachParentActor())
	{
		AActor* ParentActor = InActor->GetAttachParentActor();
		ActorParentsWeak.Add(ParentActor);
		InActor = ParentActor;
	}

	return ActorParentsWeak;
}

void FAvaSceneTreeUpdateModifierExtension::CheckTrackedActorUpdate(int32 InIdx) const
{
	if (!IsExtensionEnabled())
	{
		return;
	}

	FAvaSceneTreeActor* TrackedActor = GetTrackedActor(InIdx);
	if (!TrackedActor)
	{
		return;
	}

	AActor* LocalActor = TrackedActor->LocalActorWeak.IsValid()
		? TrackedActor->LocalActorWeak.Get()
		: GetModifierActor();

	if (!LocalActor)
	{
		return;
	}

	// Reapply in case we overwrite the whole struct outside
	TrackedActor->LocalActorWeak = LocalActor;

	// Gather previous reference actor before clearing array
	AActor* PreviousReferenceActor = !TrackedActor->ReferenceActorsWeak.IsEmpty()
		? TrackedActor->ReferenceActorsWeak.Last().Get()
		: nullptr;

	TrackedActor->ReferenceActorsWeak.Empty();

	AActor* NewReferenceActor = nullptr;

	// Track siblings actors in case their visibility changes
	for (const TWeakObjectPtr<AActor>& NewReferenceActorWeak : GetReferenceActors(TrackedActor))
	{
		AActor* ReferenceActor = NewReferenceActorWeak.Get();
		if (!ReferenceActor)
		{
			continue;
		}

		TrackedActor->ReferenceActorsWeak.Add(ReferenceActor);
		NewReferenceActor = ReferenceActor;
	}

	TrackedActor->ReferenceActorWeak = NewReferenceActor;

	// Gather children of reference actor
	const TSet<TWeakObjectPtr<AActor>> PreviousReferenceActorChildrenWeak = TrackedActor->ReferenceActorChildrenWeak;
	TrackedActor->ReferenceActorChildrenWeak = GetChildrenActorsRecursive(NewReferenceActor);

	// Gather direct children ordered of reference actor
	const TArray<TWeakObjectPtr<AActor>> PreviousDirectChildrenWeak = TrackedActor->ReferenceActorDirectChildrenWeak;
	TrackedActor->ReferenceActorDirectChildrenWeak = GetDirectChildrenActor(NewReferenceActor);

	// Gather parents of reference actor
	const TArray<TWeakObjectPtr<AActor>> PreviousParentActorsWeak = TrackedActor->ReferenceActorParentsWeak;
	TrackedActor->ReferenceActorParentsWeak = GetParentActors(NewReferenceActor);

	const bool bReferenceActorChanged = NewReferenceActor != PreviousReferenceActor;
	const bool bReferenceActorChildrenChanged = !(TrackedActor->ReferenceActorChildrenWeak.Num() == PreviousReferenceActorChildrenWeak.Num()
			&& TrackedActor->ReferenceActorChildrenWeak.Difference(PreviousReferenceActorChildrenWeak).IsEmpty());
	const bool bReferenceActorDirectChildrenChanged = !IsSameActorArray(PreviousDirectChildrenWeak, TrackedActor->ReferenceActorDirectChildrenWeak);
	const bool bReferenceActorParentChanged = !IsSameActorArray(PreviousParentActorsWeak, TrackedActor->ReferenceActorParentsWeak);

	if (IAvaSceneTreeUpdateHandler* HandlerInterface = ExtensionHandlerWeak.Get())
	{
		// Fire event when reference actor changed
		if (bReferenceActorChanged)
		{
			HandlerInterface->OnSceneTreeTrackedActorChanged(InIdx, PreviousReferenceActor, NewReferenceActor);
		}

		// Fire event when children actors changed
		if (bReferenceActorChildrenChanged)
		{
			HandlerInterface->OnSceneTreeTrackedActorChildrenChanged(InIdx, PreviousReferenceActorChildrenWeak, TrackedActor->ReferenceActorChildrenWeak);
		}

		// Fire event when direct children actors changed (even order)
		if (bReferenceActorDirectChildrenChanged)
		{
			HandlerInterface->OnSceneTreeTrackedActorDirectChildrenChanged(InIdx, PreviousDirectChildrenWeak, TrackedActor->ReferenceActorDirectChildrenWeak);
		}

		if (bReferenceActorParentChanged)
		{
			HandlerInterface->OnSceneTreeTrackedActorParentChanged(InIdx, PreviousParentActorsWeak, TrackedActor->ReferenceActorParentsWeak);
		}
	}
}

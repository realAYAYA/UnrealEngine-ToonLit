// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaArrangeBaseModifier.h"

#include "Extensions/AvaTransformUpdateModifierExtension.h"
#include "GameFramework/Actor.h"
#include "Misc/ITransaction.h"
#include "Shared/AvaTransformModifierShared.h"
#include "Shared/AvaVisibilityModifierShared.h"

void UAvaArrangeBaseModifier::OnModifierAdded(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierAdded(InReason);

	AddExtension<FAvaRenderStateUpdateModifierExtension>(this);
	AddExtension<FAvaTransformUpdateModifierExtension>(this);

	if (FAvaSceneTreeUpdateModifierExtension* SceneExtension = GetExtension<FAvaSceneTreeUpdateModifierExtension>())
	{
		ReferenceActor.ReferenceContainer = EAvaReferenceContainer::Other;
		ReferenceActor.ReferenceActorWeak = GetModifiedActor();
		ReferenceActor.bSkipHiddenActors = false;
		
		SceneExtension->TrackSceneTree(0, &ReferenceActor);
	}
}

void UAvaArrangeBaseModifier::OnModifierDisabled(EActorModifierCoreDisableReason InReason)
{
	Super::OnModifierDisabled(InReason);

	if (UAvaTransformModifierShared* LayoutShared = GetShared<UAvaTransformModifierShared>(false))
	{
		LayoutShared->RestoreActorsState(this);
	}

	if (UAvaVisibilityModifierShared* VisibilityShared = GetShared<UAvaVisibilityModifierShared>(false))
	{
		VisibilityShared->RestoreActorsState(this);
	}
}

void UAvaArrangeBaseModifier::OnModifiedActorTransformed()
{
	Super::OnModifiedActorTransformed();

	MarkModifierDirty();
}

void UAvaArrangeBaseModifier::OnSceneTreeTrackedActorChildrenChanged(int32 InIdx,
	const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors,
	const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors)
{
	Super::OnSceneTreeTrackedActorChildrenChanged(InIdx, InPreviousChildrenActors, InNewChildrenActors);

	const AActor* const ModifyActor = GetModifiedActor();
	if (!IsValid(ModifyActor))
	{
		return;
	}

	MarkModifierDirty();
}

void UAvaArrangeBaseModifier::OnSceneTreeTrackedActorDirectChildrenChanged(int32 InIdx,
	const TArray<TWeakObjectPtr<AActor>>& InPreviousChildrenActors,
	const TArray<TWeakObjectPtr<AActor>>& InNewChildrenActors)
{
	Super::OnSceneTreeTrackedActorDirectChildrenChanged(InIdx, InPreviousChildrenActors, InNewChildrenActors);

	const AActor* const ModifyActor = GetModifiedActor();
	if (!IsValid(ModifyActor))
	{
		return;
	}

	MarkModifierDirty();
}

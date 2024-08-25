// Copyright Epic Games, Inc. All Rights Reserved.

#include "Shared/AvaTranslucentPriorityModifierShared.h"
#include "AvaActorUtils.h"
#include "AvaSceneItem.h"
#include "AvaSceneTree.h"
#include "AvaSceneTreeNode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Framework/AvaGameInstance.h"
#include "IAvaSceneInterface.h"

#if WITH_EDITOR
#include "AvaOutlinerUtils.h"
#include "IAvaOutliner.h"
#include "Item/AvaOutlinerItemUtils.h"
#endif

void UAvaTranslucentPriorityModifierShared::SetComponentsState(UAvaTranslucentPriorityModifier* InModifierContext, const TSet<TWeakObjectPtr<UPrimitiveComponent>>& InComponents)
{
	if (!IsValid(InModifierContext))
	{
		return;
	}

	// Remove old component states
	for (TSet<FAvaTranslucentPriorityModifierComponentState>::TIterator It(ComponentStates); It; ++It)
	{
		UPrimitiveComponent* PrimitiveComponent = It->PrimitiveComponentWeak.Get();

		if (PrimitiveComponent && InComponents.Contains(PrimitiveComponent))
		{
			It->ModifierWeak = InModifierContext;
			continue;
		}

		const UAvaTranslucentPriorityModifier* Modifier = It->ModifierWeak.Get();
		if (!PrimitiveComponent || !Modifier || Modifier == InModifierContext)
		{
			It.RemoveCurrent();
		}
	}

	// Add missing components
	for (const TWeakObjectPtr<UPrimitiveComponent>& PrimitiveComponentWeak : InComponents)
	{
		if (UPrimitiveComponent* PrimitiveComponent = PrimitiveComponentWeak.Get())
		{
			SaveComponentState(InModifierContext, PrimitiveComponent, true);
		}
	}
}

void UAvaTranslucentPriorityModifierShared::SaveComponentState(UAvaTranslucentPriorityModifier* InModifierContext, UPrimitiveComponent* InComponent, bool bInOverrideContext)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	FAvaTranslucentPriorityModifierComponentState& ComponentState = ComponentStates.FindOrAdd(FAvaTranslucentPriorityModifierComponentState(InComponent));

	if (!ComponentState.ModifierWeak.IsValid())
	{
		ComponentState.Save();
		bInOverrideContext = true;
	}

	if (bInOverrideContext)
	{
		ComponentState.ModifierWeak = InModifierContext;
	}
}

void UAvaTranslucentPriorityModifierShared::RestoreComponentState(const UAvaTranslucentPriorityModifier* InModifierContext, UPrimitiveComponent* InComponent, bool bInClearState)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	if (const FAvaTranslucentPriorityModifierComponentState* ComponentState = ComponentStates.Find(FAvaTranslucentPriorityModifierComponentState(InComponent)))
	{
		if (ComponentState->ModifierWeak == InModifierContext)
		{
			ComponentState->Restore();

			if (bInClearState)
			{
				ComponentStates.Remove(*ComponentState);

				// This is needed in case other parent translucent modifier take ownership of the component
				UActorComponent::MarkRenderStateDirtyEvent.Broadcast(*InComponent);
			}
		}
	}
}

void UAvaTranslucentPriorityModifierShared::RestoreComponentsState(const UAvaTranslucentPriorityModifier* InModifierContext, bool bInClearState)
{
	if (!InModifierContext)
	{
		return;
	}

	for (TSet<FAvaTranslucentPriorityModifierComponentState>::TIterator It(ComponentStates); It; ++It)
	{
		if (InModifierContext == It->ModifierWeak)
		{
			RestoreComponentState(It->ModifierWeak.Get(), It->PrimitiveComponentWeak.Get(), bInClearState);
		}
	}
}

void UAvaTranslucentPriorityModifierShared::RestoreComponentsState(const UAvaTranslucentPriorityModifier* InModifierContext, const TSet<TWeakObjectPtr<UPrimitiveComponent>>& InComponents, bool bInClearState)
{
	if (!InModifierContext)
	{
		return;
	}

	for (const TWeakObjectPtr<UPrimitiveComponent>& Component : InComponents)
	{
		RestoreComponentState(InModifierContext, Component.Get(), bInClearState);
	}
}

UAvaTranslucentPriorityModifier* UAvaTranslucentPriorityModifierShared::FindModifierContext(UPrimitiveComponent* InComponent) const
{
	UAvaTranslucentPriorityModifier* ModifierContext = nullptr;

	if (!InComponent)
	{
		return ModifierContext;
	}

	if (const FAvaTranslucentPriorityModifierComponentState* ComponentState = ComponentStates.Find(FAvaTranslucentPriorityModifierComponentState(InComponent)))
	{
		ModifierContext = ComponentState->ModifierWeak.Get();
	}

	return ModifierContext;
}

TArray<const FAvaTranslucentPriorityModifierComponentState*> UAvaTranslucentPriorityModifierShared::GetSortedComponentStates(UAvaTranslucentPriorityModifier* InModifierContext) const
{
	TArray<const FAvaTranslucentPriorityModifierComponentState*> SortedComponentStates;

	if (!InModifierContext)
	{
		return SortedComponentStates;
	}

	SortedComponentStates.Reserve(ComponentStates.Num());

	if (InModifierContext->GetMode() == EAvaTranslucentPriorityModifierMode::Manual)
	{
		// Only gather components the modifier is handling
		for (const FAvaTranslucentPriorityModifierComponentState& ComponentState : ComponentStates)
		{
			const UAvaTranslucentPriorityModifier* ModifierContext = ComponentState.ModifierWeak.Get();
			const UPrimitiveComponent* PrimitiveComponent = ComponentState.PrimitiveComponentWeak.Get();

			if (!ModifierContext || !PrimitiveComponent)
			{
				continue;
			}

			if (ModifierContext == InModifierContext)
			{
				SortedComponentStates.Add(&ComponentState);
			}
		}
	}
	else if (InModifierContext->GetMode() == EAvaTranslucentPriorityModifierMode::AutoCameraDistance)
	{
		// Gather components with same camera context
		const ACameraActor* CameraActor = InModifierContext->GetCameraActorWeak().Get();

		if (!CameraActor || CameraActor->GetWorld() != InModifierContext->GetWorld())
		{
			return SortedComponentStates;
		}

		for (const FAvaTranslucentPriorityModifierComponentState& ComponentState : ComponentStates)
		{
			const UAvaTranslucentPriorityModifier* ModifierContext = ComponentState.ModifierWeak.Get();
			const UPrimitiveComponent* PrimitiveComponent = ComponentState.PrimitiveComponentWeak.Get();

			if (!ModifierContext || !PrimitiveComponent)
			{
				continue;
			}

			if (ModifierContext->GetMode() == InModifierContext->GetMode()
				&& ModifierContext->CameraActorWeak == CameraActor)
			{
				SortedComponentStates.Add(&ComponentState);
			}
		}

		// Sort current modifiers by distance X to a specific camera
		SortedComponentStates.StableSort([CameraActor](const FAvaTranslucentPriorityModifierComponentState& InComponentA, const FAvaTranslucentPriorityModifierComponentState& InComponentB)->bool
		{
			const FVector AForwardComponentLocation = CameraActor->GetActorForwardVector() * InComponentA.GetComponentLocation();
			const float ADist = FVector::Distance(AForwardComponentLocation, CameraActor->GetActorLocation());

			const FVector BForwardComponentLocation = CameraActor->GetActorForwardVector() * InComponentB.GetComponentLocation();
			const float BDist = FVector::Distance(BForwardComponentLocation, CameraActor->GetActorLocation());

			return ADist > BDist;
		});
	}
	else if (InModifierContext->GetMode() == EAvaTranslucentPriorityModifierMode::AutoOutlinerTree)
	{
		const AActor* ModifiedActor = InModifierContext->GetModifiedActor();
		UWorld* const World = ModifiedActor->GetTypedOuter<UWorld>();

		for (const FAvaTranslucentPriorityModifierComponentState& ComponentState : ComponentStates)
		{
			const UAvaTranslucentPriorityModifier* ModifierContext = ComponentState.ModifierWeak.Get();
			const UPrimitiveComponent* PrimitiveComponent = ComponentState.PrimitiveComponentWeak.Get();

			if (!ModifierContext || !PrimitiveComponent)
			{
				continue;
			}

			const AActor* OwningActor = ModifierContext->GetModifiedActor();
			if (!OwningActor)
			{
				continue;
			}

			if (ModifierContext->GetMode() == InModifierContext->GetMode()
				&& OwningActor->GetTypedOuter<UWorld>() == World)
			{
				SortedComponentStates.Add(&ComponentState);
			}
		}

		// Sort current modifiers by outliner hierarchy
#if WITH_EDITOR
		// Use the outliner first if available for sorting
		TSharedPtr<IAvaOutliner> AvaOutliner = FAvaOutlinerUtils::EditorGetOutliner(World);
		if (AvaOutliner.IsValid())
		{
			SortedComponentStates.StableSort([AvaOutliner](const FAvaTranslucentPriorityModifierComponentState& InComponentA, const FAvaTranslucentPriorityModifierComponentState& InComponentB)->bool
			{
				const FAvaOutlinerItemPtr OutlinerItemA = AvaOutliner->FindItem(InComponentA.GetOwningActor());
				const FAvaOutlinerItemPtr OutlinerItemB = AvaOutliner->FindItem(InComponentB.GetOwningActor());

				return UE::AvaOutliner::CompareOutlinerItemOrder(OutlinerItemB, OutlinerItemA);
			});
		}
		else
		{
#endif
			// Fallback on scene tree sorting
			if (const IAvaSceneInterface* SceneInterface = FAvaActorUtils::GetSceneInterfaceFromActor(ModifiedActor))
			{
				const FAvaSceneTree& SceneTree = SceneInterface->GetSceneTree();

				SortedComponentStates.StableSort([&SceneTree, World](const FAvaTranslucentPriorityModifierComponentState& InComponentA, const FAvaTranslucentPriorityModifierComponentState& InComponentB)->bool
				{
					const FAvaSceneTreeNode* SceneItemA = SceneTree.FindTreeNode(FAvaSceneItem(InComponentA.GetOwningActor(), World));
					const FAvaSceneTreeNode* SceneItemB = SceneTree.FindTreeNode(FAvaSceneItem(InComponentB.GetOwningActor(), World));

					return FAvaSceneTree::CompareTreeItemOrder(SceneItemB, SceneItemA);
				});
			}
#if WITH_EDITOR
		}
#endif
	}

	return SortedComponentStates;
}

void UAvaTranslucentPriorityModifierShared::SetSortPriorityOffset(int32 InOffset)
{
	if (SortPriorityOffset == InOffset)
	{
		return;
	}

	SortPriorityOffset = InOffset;
	OnLevelGlobalsChangedDelegate.Broadcast();
}

void UAvaTranslucentPriorityModifierShared::SetSortPriorityStep(int32 InStep)
{
	if (SortPriorityStep == InStep)
	{
		return;
	}

	SortPriorityStep = InStep;
	OnLevelGlobalsChangedDelegate.Broadcast();
}

void UAvaTranslucentPriorityModifierShared::PostLoad()
{
	Super::PostLoad();

	// Clean up invalid components
	for (TSet<FAvaTranslucentPriorityModifierComponentState>::TIterator It(ComponentStates); It; ++It)
	{
		if (!It->PrimitiveComponentWeak.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void FAvaTranslucentPriorityModifierComponentState::Save()
{
	if (const UPrimitiveComponent* Component = PrimitiveComponentWeak.Get())
	{
		SortPriority = Component->TranslucencySortPriority;
	}
}

void FAvaTranslucentPriorityModifierComponentState::Restore() const
{
	if (UPrimitiveComponent* Component = PrimitiveComponentWeak.Get())
	{
		Component->SetTranslucentSortPriority(SortPriority);
	}
}

AActor* FAvaTranslucentPriorityModifierComponentState::GetOwningActor() const
{
	if (const UPrimitiveComponent* Component = PrimitiveComponentWeak.Get())
	{
		return Component->GetOwner();
	}

	return nullptr;
}

FVector FAvaTranslucentPriorityModifierComponentState::GetComponentLocation() const
{
	if (const UPrimitiveComponent* Component = PrimitiveComponentWeak.Get())
	{
		return Component->GetComponentLocation();
	}

	return FVector::ZeroVector;
}

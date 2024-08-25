// Copyright Epic Games, Inc. All Rights Reserved.

#include "Outliner/AvaPropertyAnimatorEditorOutlinerProxy.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Components/PropertyAnimatorCoreComponent.h"
#include "IAvaOutliner.h"
#include "Item/AvaOutlinerActor.h"
#include "Outliner/AvaPropertyAnimatorEditorOutliner.h"
#include "Selection/AvaOutlinerScopedSelection.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "AvaPropertyAnimatorEditorOutlinerProxy"

FAvaPropertyAnimatorEditorOutlinerProxy::FAvaPropertyAnimatorEditorOutlinerProxy(IAvaOutliner& InOutliner, const FAvaOutlinerItemPtr& InParentItem)
	: Super(InOutliner, InParentItem)
{
	ItemIcon = FSlateIconFinder::FindIconForClass(UPropertyAnimatorCoreComponent::StaticClass());
}

UPropertyAnimatorCoreComponent* FAvaPropertyAnimatorEditorOutlinerProxy::GetPropertyAnimatorComponent() const
{
	const FAvaOutlinerItemPtr Parent = GetParent();

	if (!Parent.IsValid())
	{
		return nullptr;
	}

	const FAvaOutlinerActor* const ActorItem = Parent->CastTo<FAvaOutlinerActor>();

	if (!ActorItem)
	{
		return nullptr;
	}

	const AActor* Actor = ActorItem->GetActor();

	if (!Actor)
	{
		return nullptr;
	}

	return Actor->FindComponentByClass<UPropertyAnimatorCoreComponent>();
}

void FAvaPropertyAnimatorEditorOutlinerProxy::OnItemRegistered()
{
	Super::OnItemRegistered();
	BindDelegates();
}

void FAvaPropertyAnimatorEditorOutlinerProxy::OnItemUnregistered()
{
	Super::OnItemUnregistered();
	UnbindDelegates();
}

void FAvaPropertyAnimatorEditorOutlinerProxy::Select(FAvaOutlinerScopedSelection& InSelection) const
{
	if (UPropertyAnimatorCoreComponent* const PropertyAnimatorComponent = GetPropertyAnimatorComponent())
	{
		InSelection.Select(PropertyAnimatorComponent);
	}
}

FText FAvaPropertyAnimatorEditorOutlinerProxy::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Animators");
}

FSlateIcon FAvaPropertyAnimatorEditorOutlinerProxy::GetIcon() const
{
	return ItemIcon;
}

FText FAvaPropertyAnimatorEditorOutlinerProxy::GetIconTooltipText() const
{
	return LOCTEXT("Tooltip", "Shows all the animators found in the property animator component of an actor");
}

void FAvaPropertyAnimatorEditorOutlinerProxy::GetProxiedItems(const TSharedRef<IAvaOutlinerItem>& InParent
	, TArray<FAvaOutlinerItemPtr>& OutChildren, bool bInRecursive)
{
	if (const UPropertyAnimatorCoreComponent* const PropertyAnimatorComponent = GetPropertyAnimatorComponent())
	{
		for (UPropertyAnimatorCoreBase* const PropertyAnimator : PropertyAnimatorComponent->GetAnimators())
		{
			if (!PropertyAnimator)
			{
				continue;
			}

			const FAvaOutlinerItemPtr AnimatorItem = Outliner.FindOrAdd<FAvaPropertyAnimatorEditorOutliner>(PropertyAnimator);
			AnimatorItem->SetParent(SharedThis(this));

			OutChildren.Add(AnimatorItem);

			if (bInRecursive)
			{
				AnimatorItem->FindChildren(OutChildren, bInRecursive);
			}
		}
	}
}

void FAvaPropertyAnimatorEditorOutlinerProxy::BindDelegates()
{
	UnbindDelegates();
	UPropertyAnimatorCoreBase::OnAnimatorCreatedDelegate.AddSP(this, &FAvaPropertyAnimatorEditorOutlinerProxy::OnPropertyAnimatorUpdated);
	UPropertyAnimatorCoreBase::OnAnimatorRemovedDelegate.AddSP(this, &FAvaPropertyAnimatorEditorOutlinerProxy::OnPropertyAnimatorUpdated);
	UPropertyAnimatorCoreBase::OnAnimatorRenamedDelegate.AddSP(this, &FAvaPropertyAnimatorEditorOutlinerProxy::OnPropertyAnimatorUpdated);
}

void FAvaPropertyAnimatorEditorOutlinerProxy::UnbindDelegates()
{
	UPropertyAnimatorCoreBase::OnAnimatorCreatedDelegate.RemoveAll(this);
	UPropertyAnimatorCoreBase::OnAnimatorRemovedDelegate.RemoveAll(this);
	UPropertyAnimatorCoreBase::OnAnimatorRenamedDelegate.RemoveAll(this);
}

void FAvaPropertyAnimatorEditorOutlinerProxy::OnPropertyAnimatorUpdated(UPropertyAnimatorCoreBase* InAnimator)
{
	const UPropertyAnimatorCoreComponent* PropertyAnimatorComponent = GetPropertyAnimatorComponent();

	if (IsValid(InAnimator) && IsValid(PropertyAnimatorComponent))
	{
		const UPropertyAnimatorCoreComponent* UpdatedComponent = InAnimator->GetTypedOuter<UPropertyAnimatorCoreComponent>();

		if (UpdatedComponent == PropertyAnimatorComponent)
		{
			RefreshChildren();
			Outliner.RequestRefresh();
		}
	}
}

#undef LOCTEXT_NAMESPACE

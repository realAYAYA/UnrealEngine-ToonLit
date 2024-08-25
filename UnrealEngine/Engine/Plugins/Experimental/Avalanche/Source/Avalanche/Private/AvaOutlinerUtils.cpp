// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaOutlinerUtils.h"

#if WITH_EDITOR

#include "AvaOutlinerSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "IAvaOutliner.h"
#include "Item/AvaOutlinerActor.h"

TSharedPtr<IAvaOutliner> FAvaOutlinerUtils::EditorGetOutliner(const UWorld* const InWorld)
{
	check(IsValid(InWorld));

	UAvaOutlinerSubsystem* const OutlinerSubsystem = InWorld->GetSubsystem<UAvaOutlinerSubsystem>();
	if (!IsValid(OutlinerSubsystem))
	{
		return nullptr;
	}

	return OutlinerSubsystem->GetOutliner();
}

bool FAvaOutlinerUtils::EditorOutlinerItemsToActors(const TArray<FAvaOutlinerItemPtr>& InItems, TArray<AActor*>& OutActors)
{
	const int32 ItemCount = InItems.Num();

	OutActors.Empty();

	for (int32 i = 0; i < ItemCount; ++i)
	{
		if (!InItems[i].IsValid())
		{
			// don't allow any invalid items for this function
			OutActors.Empty();
			return false;
		}

		const FAvaOutlinerActor* const ActorItem = InItems[i]->CastTo<FAvaOutlinerActor>();
		if (ActorItem)
		{
			AActor* const ChildActor = ActorItem->GetActor();
			if (!IsValid(ChildActor))
			{
				OutActors.Empty();
				return false;
			}

			OutActors.Add(ChildActor);
		}
	}

	return true;
}

TArray<AActor*> FAvaOutlinerUtils::EditorOutlinerChildActors(TSharedPtr<IAvaOutliner> InOutliner, AActor* const InParentActor)
{
	if (!InOutliner.IsValid())
	{
		return {};
	}

	// Return all root level actors if no parent is specified.
	if (!IsValid(InParentActor))
	{
		TArray<AActor*> OutActors;
		if (EditorOutlinerItemsToActors(InOutliner->GetTreeRoot()->GetChildrenMutable(), OutActors))
		{
			return OutActors;
		}
		return {};
	}

	FAvaOutlinerItemPtr OutlinerItem = InOutliner->FindItem(InParentActor);
	if (OutlinerItem.IsValid())
	{
		TArray<AActor*> OutActors;
		if (EditorOutlinerItemsToActors(OutlinerItem->GetChildrenMutable(), OutActors))
		{
			return OutActors;
		}
	}

	return {};
}

bool FAvaOutlinerUtils::EditorActorIsolationInfo(TSharedPtr<IAvaOutliner> InOutliner, TArray<TWeakObjectPtr<const AActor>>& OutIsolatedActors)
{
	// todo
	OutIsolatedActors.Empty();
	return false;
}

#endif // WITH_EDITOR

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Types/UIFWidgetTree.h"
#include "UIFLog.h"
#include "UIFPlayerComponent.h"
#include "UIFWidget.h"

#include "Engine/ActorChannel.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"


/**
 *
 */
FUIFrameworkWidgetTreeEntry::FUIFrameworkWidgetTreeEntry(UUIFrameworkWidget* InParent, UUIFrameworkWidget* InChild)
	: Parent(InParent)
	, Child(InChild)
	, ParentId(InParent ? InParent->GetWidgetId() : FUIFrameworkWidgetId::MakeRoot())
	, ChildId(InChild->GetWidgetId())
{

}

bool FUIFrameworkWidgetTreeEntry::IsParentValid() const
{
	return (Parent && Parent->GetWidgetId() == ParentId) || ParentId.IsRoot();
}

bool FUIFrameworkWidgetTreeEntry::IsChildValid() const
{
	return Child && Child->GetWidgetId() == ChildId;
}

/**
 *
 */
void FUIFrameworkWidgetTree::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		FUIFrameworkWidgetTreeEntry& Entry = Entries[Index];
		if (Entry.Child)
		{
			OwnerComponent->LocalWidgetRemovedFromTree(Entry);

			// Was it remove but added by something else
			{
				int32 Count = 0;
				for (const FUIFrameworkWidgetTreeEntry& Other : Entries)
				{
					if (Other.Child == Entry.Child)
					{
						++Count;
						if (Count > 2)
						{
							break;
						}
					}
				}
				if (Count <= 1)
				{
					Entry.Child->LocalDestroyUMGWidget();
					WidgetByIdMap.Remove(Entry.Child->GetWidgetId());
				}
			}
		}
	}
}

void FUIFrameworkWidgetTree::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		FUIFrameworkWidgetTreeEntry& Entry = Entries[Index];
		if (Entry.ParentId.IsValid() && Entry.ChildId.IsValid())
		{
			OwnerComponent->LocalWidgetWasAddedToTree(Entry);
			WidgetByIdMap.FindOrAdd(Entry.ChildId) = Entry.Child;
			if (!Entry.ParentId.IsRoot())
			{
				WidgetByIdMap.FindOrAdd(Entry.ParentId) = Entry.Parent;
			}
		}
	}
}

void FUIFrameworkWidgetTree::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		// Note these events should only be called when the widget was not constructed and are now constructed.
		FUIFrameworkWidgetTreeEntry& Entry = Entries[Index];
		if (Entry.ParentId.IsValid() && Entry.ChildId.IsValid())
		{
			OwnerComponent->LocalWidgetWasAddedToTree(Entry);
			WidgetByIdMap.FindOrAdd(Entry.ChildId) = Entry.Child;
			if (!Entry.ParentId.IsRoot())
			{
				WidgetByIdMap.FindOrAdd(Entry.ParentId) = Entry.Parent;
			}
		}
	}
}

bool FUIFrameworkWidgetTree::ReplicateSubWidgets(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = false;
#if DO_CHECK
	TSet<UUIFrameworkWidget*> AllChildren;
#endif
	for (FUIFrameworkWidgetTreeEntry& Entry : Entries)
	{
		UUIFrameworkWidget* Widget = Entry.Child;
		if (IsValid(Widget))
		{
#if DO_CHECK
			bool bAlreadyInSet = false;
			AllChildren.Add(Widget, &bAlreadyInSet);
			ensureMsgf(bAlreadyInSet == false, TEXT("The widget has more than one parent."));
#endif

			bWroteSomething |= Channel->ReplicateSubobject(Widget, *Bunch, *RepFlags);
		}
	}
	return bWroteSomething;
}

void FUIFrameworkWidgetTree::AddRoot(UUIFrameworkWidget* Widget)
{
	check(Widget);
	AddChildInternal(nullptr, Widget);
}

void FUIFrameworkWidgetTree::AddWidget(UUIFrameworkWidget* Parent, UUIFrameworkWidget* Child)
{
	check(Parent);
	check(Child);
	AddChildInternal(Parent, Child);
}

void FUIFrameworkWidgetTree::AddChildInternal(UUIFrameworkWidget* Parent, UUIFrameworkWidget* Child)
{
	check(OwnerComponent);
	int32 PreviousEntryIndex = Entries.IndexOfByPredicate([Child](const FUIFrameworkWidgetTreeEntry& Other){ return Other.Child == Child; });
	if (PreviousEntryIndex != INDEX_NONE)
	{
		const FUIFrameworkWidgetTreeEntry& PreviousEntry = Entries[PreviousEntryIndex];
		if (PreviousEntry.Parent != Parent)
		{
			// New Parent for an existing child.
			Entries.RemoveAtSwap(PreviousEntryIndex);
			FUIFrameworkWidgetTreeEntry& NewEntry = Entries.Emplace_GetRef(Parent, Child);
			MarkItemDirty(NewEntry);
		}
		else
		{
			UE_LOG(LogUIFramework, Warning, TEXT("A widget was added to the WidgetTree but it's already added."));
		}
	}
	else
	{
		FUIFrameworkWidgetTreeEntry& NewEntry = Entries.Emplace_GetRef(Parent, Child);
		MarkItemDirty(NewEntry);
		WidgetByIdMap.FindOrAdd(Child->GetWidgetId()) = Child;

		if (OwnerComponent->GetOwner()->IsUsingRegisteredSubObjectList())
		{
			OwnerComponent->GetOwner()->AddReplicatedSubObject(Child);
		}
		AddChildRecursiveInternal(Child);
	}
}

void FUIFrameworkWidgetTree::AddChildRecursiveInternal(UUIFrameworkWidget* Widget)
{
	FUIFrameworkWidgetTree* Self = this;
	Widget->AuthorityForEachChildren([Self, Widget](UUIFrameworkWidget* ChildWidget)
		{
			if (ChildWidget != nullptr)
			{
				FUIFrameworkWidgetTreeEntry& NewEntry = Self->Entries.Emplace_GetRef(Widget, ChildWidget);
				Self->MarkItemDirty(NewEntry);
				Self->WidgetByIdMap.FindOrAdd(ChildWidget->GetWidgetId()) = ChildWidget;
				if (Self->OwnerComponent->GetOwner()->IsUsingRegisteredSubObjectList())
				{
					Self->OwnerComponent->GetOwner()->AddReplicatedSubObject(ChildWidget);
				}
				Self->AddChildRecursiveInternal(ChildWidget);
			}
		});
}

void FUIFrameworkWidgetTree::RemoveWidget(UUIFrameworkWidget* Widget)
{
	check(Widget);
	bool bDirtyArray = false;
	{
		FMemMark Mark(FMemStack::Get());
		TArray<UUIFrameworkWidget*, TInlineAllocator<60, TMemStackAllocator<>>> WidgetsToRemove;
		WidgetsToRemove.Add(Widget);

		while (WidgetsToRemove.Num() > 0)
		{
			UUIFrameworkWidget* CurrentWidgetToRemove = WidgetsToRemove.Last();
			WidgetByIdMap.Remove(CurrentWidgetToRemove->GetWidgetId());
			OwnerComponent->GetOwner()->RemoveReplicatedSubObject(CurrentWidgetToRemove);

			WidgetsToRemove.RemoveAt(WidgetsToRemove.Num()-1);
			for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
			{
				if (Entries[Index].Parent == CurrentWidgetToRemove)
				{
					bDirtyArray = true;
					if (Entries[Index].Child)
					{
						WidgetsToRemove.Add(Entries[Index].Child);
					}

					Entries.RemoveAtSwap(Index);
				}
				else if (Entries[Index].Child == CurrentWidgetToRemove)
				{
					bDirtyArray = true;
					Entries.RemoveAtSwap(Index);
				}
			}
		}
	}

	if (bDirtyArray)
	{
		MarkArrayDirty();
	}
}

FUIFrameworkWidgetTreeEntry* FUIFrameworkWidgetTree::GetEntryByReplicationId(int32 ReplicationId)
{
	if (const int32* Index = ItemMap.Find(ReplicationId))
	{
		return &Entries[*Index];
	}
	return nullptr;
}

const FUIFrameworkWidgetTreeEntry* FUIFrameworkWidgetTree::GetEntryByReplicationId(int32 ReplicationId) const
{
	return const_cast<FUIFrameworkWidgetTree*>(this)->GetEntryByReplicationId(ReplicationId);
}

UUIFrameworkWidget* FUIFrameworkWidgetTree::FindWidgetById(FUIFrameworkWidgetId WidgetId)
{
	const TWeakObjectPtr<UUIFrameworkWidget>* Found = WidgetByIdMap.Find(WidgetId);
	return Found ? Found->Get() : nullptr;
}

const UUIFrameworkWidget* FUIFrameworkWidgetTree::FindWidgetById(FUIFrameworkWidgetId WidgetId) const
{
	return const_cast<FUIFrameworkWidgetTree*>(this)->FindWidgetById(WidgetId);
}
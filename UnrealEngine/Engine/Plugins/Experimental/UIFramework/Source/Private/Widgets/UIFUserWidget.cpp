// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/UIFUserWidget.h"
#include "Types/UIFWidgetTree.h"
#include "UIFLog.h"
#include "UIFModule.h"

#include "Blueprint/UserWidget.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UIFUserWidget)


/**
 *
 */
void FUIFrameworkUserWidgetSlotList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	check(Owner);
	for (int32 Index : ChangedIndices)
	{
		FUIFrameworkUserWidgetSlot& Slot = Slots[Index];
		if (Slot.LocalIsAquiredWidgetValid())
		{
			// Remove and add the widget again... 
			// that may not work if they are on top of each other... The order may matter if the zorder is the same :( 
			Owner->LocalAddChild(Slot.GetWidgetId());
		}
		//else it will be remove and the new widget will be added by the WidgetTree replication.
	}
}

bool FUIFrameworkUserWidgetSlotList::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	return FFastArraySerializer::FastArrayDeltaSerialize<FUIFrameworkUserWidgetSlot, FUIFrameworkUserWidgetSlotList>(Slots, DeltaParms, *this);
}

void FUIFrameworkUserWidgetSlotList::AddEntry(FUIFrameworkUserWidgetSlot Entry)
{
	FUIFrameworkUserWidgetSlot& NewEntry = Slots.Add_GetRef(MoveTemp(Entry));
	MarkItemDirty(NewEntry);
}

bool FUIFrameworkUserWidgetSlotList::RemoveEntry(UUIFrameworkWidget* Widget)
{
	check(Widget);
	const int32 Index = Slots.IndexOfByPredicate([Widget](const FUIFrameworkUserWidgetSlot& Entry) { return Entry.AuthorityGetWidget() == Widget; });
	if (Index != INDEX_NONE)
	{
		Slots.RemoveAt(Index);
		MarkArrayDirty();
	}
	return Index != INDEX_NONE;
}

FUIFrameworkUserWidgetSlot* FUIFrameworkUserWidgetSlotList::FindEntry(FUIFrameworkWidgetId WidgetId)
{
	return Slots.FindByPredicate([WidgetId](const FUIFrameworkUserWidgetSlot& Entry) { return Entry.GetWidgetId() == WidgetId; });
}

void FUIFrameworkUserWidgetSlotList::ForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func)
{
	for (FUIFrameworkUserWidgetSlot& Slot : Slots)
	{
		if (UUIFrameworkWidget* ChildWidget = Slot.AuthorityGetWidget())
		{
			Func(ChildWidget);
		}
	}
}

/**
 *
 */
UUIFrameworkUserWidget::UUIFrameworkUserWidget()
	: ReplicatedSlotList(this)
{
}

void UUIFrameworkUserWidget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ReplicatedSlotList, Params);
}

void UUIFrameworkUserWidget::LocalOnUMGWidgetCreated()
{
	Super::LocalOnUMGWidgetCreated();
	//todo add viewmodel
}

bool UUIFrameworkUserWidget::LocalIsReplicationReady() const
{
	return Super::LocalIsReplicationReady() && !WidgetClass.IsNull();
}

void UUIFrameworkUserWidget::SetWidgetClass(TSoftClassPtr<UWidget> InWidgetClass)
{
	WidgetClass = InWidgetClass;
	MARK_PROPERTY_DIRTY_FROM_NAME(UUIFrameworkWidget, WidgetClass, this);
}

void UUIFrameworkUserWidget::SetNamedSlot(FName SlotName, UUIFrameworkWidget* Widget)
{
	if (Widget == nullptr || SlotName.IsNone())
	{
		FFrame::KismetExecutionMessage(TEXT("The widget is invalid. It can't be added."), ELogVerbosity::Warning, "InvalidWidgetToAdd");
	}
	else
	{
		// Reset the widget to make sure the id is set and it may have been duplicated during the attach
		FUIFrameworkUserWidgetSlot Entry;
		Entry.SlotName = SlotName;
		Entry.AuthoritySetWidget(Widget);
		Entry.AuthoritySetWidget(FUIFrameworkModule::AuthorityAttachWidget(this, Entry.AuthorityGetWidget()));
		AddEntry(Entry);
	}
}


void UUIFrameworkUserWidget::AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func)
{
	Super::AuthorityForEachChildren(Func);

	ReplicatedSlotList.ForEachChildren(Func);
}

void UUIFrameworkUserWidget::AuthorityRemoveChild(UUIFrameworkWidget* Widget)
{
	Super::AuthorityRemoveChild(Widget);
	RemoveEntry(Widget);
}

void UUIFrameworkUserWidget::LocalAddChild(FUIFrameworkWidgetId ChildId)
{
	bool bIsAdded = false;
	if (FUIFrameworkUserWidgetSlot* NamedSlotEntry = FindEntry(ChildId))
	{
		if (FUIFrameworkWidgetTree* WidgetTree = GetWidgetTree())
		{
			if (UUIFrameworkWidget* ChildWidget = WidgetTree->FindWidgetById(ChildId))
			{
				UWidget* ChildUMGWidget = ChildWidget->LocalGetUMGWidget();
				if (ensure(ChildUMGWidget))
				{
					NamedSlotEntry->LocalAquireWidget();
					if (UUserWidget* LocalUMGUserWidget = Cast<UUserWidget>(LocalGetUMGWidget()))
					{
						LocalUMGUserWidget->SetContentForSlot(NamedSlotEntry->SlotName, ChildUMGWidget);
					}
					else
					{
						UE_LOG(LogUIFramework, Log, TEXT("Can't set the NamedSlot on widget '%s' because it is not a UserWidget."), *ChildUMGWidget->GetName());
						Super::LocalAddChild(ChildId);
					}
				}
				else
				{
					UE_LOG(LogUIFramework, Error, TEXT("The widget '%" INT64_FMT "' is invalid."), ChildId.GetKey());
					Super::LocalAddChild(ChildId);
				}
			}
			else
			{
				UE_LOG(LogUIFramework, Log, TEXT("The widget '%" INT64_FMT "' doesn't exist in the WidgetTree."), ChildId.GetKey());
				Super::LocalAddChild(ChildId);
			}
		}
		else
		{
			UE_LOG(LogUIFramework, Log, TEXT("The widget '%" INT64_FMT "' doesn't exist in the WidgetTree."), ChildId.GetKey());
			Super::LocalAddChild(ChildId);
		}
	}
	else
	{
		UE_LOG(LogUIFramework, Verbose, TEXT("The widget '%" INT64_FMT "' was not found in the Canvas Slots."), ChildId.GetKey());
		Super::LocalAddChild(ChildId);
	}
}

void UUIFrameworkUserWidget::AddEntry(FUIFrameworkUserWidgetSlot Entry)
{
	ReplicatedSlotList.AddEntry(Entry);
}

bool UUIFrameworkUserWidget::RemoveEntry(UUIFrameworkWidget* Widget)
{
	return ReplicatedSlotList.RemoveEntry(Widget);
}

FUIFrameworkUserWidgetSlot* UUIFrameworkUserWidget::FindEntry(FUIFrameworkWidgetId WidgetId)
{
	return ReplicatedSlotList.FindEntry(WidgetId);
}

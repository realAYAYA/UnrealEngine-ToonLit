// Copyright Epic Games, Inc. All Rights Reserved.

#include "UIFPlayerComponent.h"
#include "Misc/MemStack.h"
#include "UIFLog.h"
#include "Templates/NonNullPointer.h"
#include "UIFModule.h"
#include "Types/UIFWidgetOwner.h"
#include "UIFLocalSettings.h"
#include "UIFPresenter.h"
#include "UIFWidget.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UIFPlayerComponent)


/**
 *
 */

void FUIFrameworkGameLayerSlotList::PreReplicatedRemove(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	check(Owner);
	for (int32 Index : ChangedIndices)
	{
		FUIFrameworkGameLayerSlot& Slot = Entries[Index];
		if (Slot.LocalIsAquiredWidgetValid())
		{
			if (Owner->Presenter)
			{
				Owner->Presenter->RemoveFromViewport(Slot.GetWidgetId());
			}
		}
	}
}

void FUIFrameworkGameLayerSlotList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	check(Owner);
	for (int32 Index : ChangedIndices)
	{
		FUIFrameworkGameLayerSlot& Slot = Entries[Index];
		if (Slot.LocalIsAquiredWidgetValid())
		{
			Owner->LocalAddChild(Slot.GetWidgetId());
		}
	}
}

void FUIFrameworkGameLayerSlotList::AddEntry(FUIFrameworkGameLayerSlot Entry)
{
	FUIFrameworkGameLayerSlot& NewEntry = Entries.Add_GetRef(MoveTemp(Entry));
	MarkItemDirty(NewEntry);
}

bool FUIFrameworkGameLayerSlotList::RemoveEntry(UUIFrameworkWidget* Widget)
{
	check(Widget);
	const int32 Index = Entries.IndexOfByPredicate([Widget](const FUIFrameworkGameLayerSlot& Entry){ return Entry.AuthorityGetWidget() == Widget; });
	if (Index != INDEX_NONE)
	{
		Entries.RemoveAt(Index);
		MarkArrayDirty();
	}
	return Index != INDEX_NONE;
}

FUIFrameworkGameLayerSlot* FUIFrameworkGameLayerSlotList::FindEntry(FUIFrameworkWidgetId WidgetId)
{
	return Entries.FindByPredicate([WidgetId](const FUIFrameworkGameLayerSlot& Entry) { return Entry.GetWidgetId() == WidgetId; });
}

const FUIFrameworkGameLayerSlot* FUIFrameworkGameLayerSlotList::FindEntry(FUIFrameworkWidgetId WidgetId) const
{
	return Entries.FindByPredicate([WidgetId](const FUIFrameworkGameLayerSlot& Entry) { return Entry.GetWidgetId() == WidgetId; });
}


/**
 *
 */
UUIFrameworkPlayerComponent::UUIFrameworkPlayerComponent()
	: RootList(this)
	, WidgetTree(GetOwner(), this)
{
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_DuringPhysics;

	if (!IsTemplate() && GetPlayerController() && GetPlayerController()->IsLocalPlayerController())
	{
		Presenter = static_cast<UUIFrameworkPresenter*>(CreateDefaultSubobject(TEXT("Presenter"), UUIFrameworkPresenter::StaticClass(), FUIFrameworkModule::GetPresenterClass().Get(), true, true));
	}
}

void UUIFrameworkPlayerComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (GetOwner()->HasAuthority())
	{
		WidgetTree.AuthorityAddAllWidgetsFromActorChannel();
	}
	else
	{
		GetDefault<UUIFrameworkLocalSettings>()->LoadResources();
	}
}

void UUIFrameworkPlayerComponent::UninitializeComponent()
{
	// On local, remove all UWidget.
	if (GetOwner()->HasAuthority())
	{
		WidgetTree.AuthorityRemoveAllWidgetsFromActorChannel();
	}
	else
	{
		for (FUIFrameworkGameLayerSlot& Entry : RootList.Entries)
		{
			if (UUIFrameworkWidget* ChildWidget = GetWidgetTree().FindWidgetById(Entry.GetWidgetId()))
			{
				ChildWidget->LocalDestroyUMGWidget();
			}
		}
	}

	Super::UninitializeComponent();
}

void UUIFrameworkPlayerComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, RootList, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, WidgetTree, Params);
}

bool UUIFrameworkPlayerComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	if (!bReplicateUsingRegisteredSubObjectList)
	{
		bWroteSomething |= WidgetTree.ReplicateSubWidgets(Channel, Bunch, RepFlags);
	}
	return bWroteSomething;
}

void UUIFrameworkPlayerComponent::AddWidget(FUIFrameworkGameLayerSlot InEntry)
{
	APlayerController* LocalOwner = GetPlayerController();
	check(LocalOwner->HasAuthority());

	if (InEntry.AuthorityGetWidget() == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("The widget is invalid. It can't be added."), ELogVerbosity::Warning, "InvalidWidgetToAdd");
	}
	else
	{
		// Reset the widget to make sure the id is set and it may have been duplicated during the attach
		InEntry.AuthoritySetWidget(FUIFrameworkModule::AuthorityAttachWidget(this, InEntry.AuthorityGetWidget()));
		RootList.AddEntry(InEntry);
	}
}

void UUIFrameworkPlayerComponent::RemoveWidget(UUIFrameworkWidget* Widget)
{
	APlayerController* LocalOwner = GetPlayerController();
	check(LocalOwner->HasAuthority());

	if (Widget == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("The widget is invalid. It can't be removed."), ELogVerbosity::Warning, "InvalidWidgetToRemove");
	}
	else
	{
		if (Widget->GetWidgetTreeOwner() != this)
		{
			FFrame::KismetExecutionMessage(TEXT("The widget was not added on this player. It can't be removed on this player."), ELogVerbosity::Warning, "InvalidPlayerParentOnRemovedWidget");
		}
		else
		{
			if (RootList.RemoveEntry(Widget))
			{
				FUIFrameworkModule::AuthorityDetachWidgetFromParent(Widget);
			}
		}
	}
}

void UUIFrameworkPlayerComponent::AuthorityRemoveChild(UUIFrameworkWidget* Widget)
{
	check(Widget);
	RootList.RemoveEntry(Widget);
}

FOnPendingReplicationProcessed& UUIFrameworkPlayerComponent::GetOnPendingReplicationProcessed()
{
	return OnPendingReplicationProcessed;
}

FUIFrameworkWidgetTree& UUIFrameworkPlayerComponent::GetWidgetTree()
{
	return WidgetTree;
}

FUIFrameworkWidgetOwner UUIFrameworkPlayerComponent::GetWidgetOwner() const
{
	FUIFrameworkWidgetOwner Owner;
	Owner.PlayerController = GetPlayerController();
	return Owner;
}

void UUIFrameworkPlayerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	APlayerController* LocalOwner = GetPlayerController();
	check(!LocalOwner->HasAuthority());

	if (ClassesToLoad.Num() == 0 && NetReplicationPending.Num() == 0)
	{
		// Create and add all the pending widgets
		TGuardValue<bool> TmpGuard = {bAddingWidget, true};
		for (int32 ReplicationId : AddPending)
		{
			if (FUIFrameworkWidgetTreeEntry* Entry = WidgetTree.LocalGetEntryByReplicationId(ReplicationId))
			{
				if (ensure(Entry->IsParentValid() && Entry->IsChildValid()))
				{
					if (Entry->ParentId.IsRoot())
					{
						LocalAddChild(Entry->ChildId);
					}
					else
					{
						Entry->Parent->LocalAddChild(Entry->ChildId);
					}
				}
			}
			else
			{
				UE_LOG(LogUIFramework, Verbose, TEXT("A widget would was added but couldn't be found anymore."));
			}
		}

		NetReplicationPending.Empty();
		AddPending.Empty();
		ClassesToLoad.Empty();

		OnPendingReplicationProcessed.Broadcast();

		PrimaryComponentTick.SetTickFunctionEnable(false);
	}
	else
	{
		FMemMark Mark(FMemStack::Get());
		TArray<int32, TMemStackAllocator<>> TempNetReplicationPending;
		TempNetReplicationPending.Reserve(NetReplicationPending.Num());
		for (int32 ReplicationId : NetReplicationPending)
		{
			TempNetReplicationPending.Add(ReplicationId);
		}
		NetReplicationPending.Empty(false);

		for (int32 ReplicationId : TempNetReplicationPending)
		{
			if (const FUIFrameworkWidgetTreeEntry* Entry = WidgetTree.LocalGetEntryByReplicationId(ReplicationId))
			{
				LocalWidgetWasAddedToTree(*Entry);
			}
		}
	}
}

void UUIFrameworkPlayerComponent::LocalWidgetWasAddedToTree(const FUIFrameworkWidgetTreeEntry& Entry)
{
	APlayerController* LocalOwner = GetPlayerController();
	check(!LocalOwner->HasAuthority());

	if (Entry.Child && Entry.Child->LocalIsReplicationReady())
	{
		TSoftClassPtr<UWidget> WidgetClass = Entry.Child->GetUMGWidgetClass();
		if (WidgetClass.IsValid())
		{
			if (Entry.IsParentValid() && Entry.IsChildValid())
			{
				Entry.Child->LocalCreateUMGWidget(this);
				AddPending.Add(Entry.ReplicationID);
				NetReplicationPending.Remove(Entry.ReplicationID);
			}
			else
			{
				NetReplicationPending.Add(Entry.ReplicationID);
			}
		}
		else if (!WidgetClass.IsNull() && WidgetClass.IsPending())
		{
			if (FWidgetClassToLoad* FoundWidgetClassToLoad = ClassesToLoad.Find(WidgetClass))
			{
				FoundWidgetClassToLoad->EntryReplicationIds.AddUnique(Entry.ReplicationID);
			}
			else
			{
				// The class needs to be loaded
				TWeakObjectPtr<ThisClass> WeakSelf = this;
				TSharedPtr<FStreamableHandle> StreamableHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
					WidgetClass.ToSoftObjectPath()
					, [WeakSelf, WidgetClass]() mutable
					{
						if (ThisClass* StrongSelf = WeakSelf.Get())
						{
							StrongSelf->LocalOnClassLoaded(WidgetClass);
						}
					}
				, FStreamableManager::AsyncLoadHighPriority, false, false, TEXT("UIWidget Widget Class"));

				FWidgetClassToLoad NewItem;
				NewItem.EntryReplicationIds.Add(Entry.ReplicationID);
				NewItem.StreamableHandle = MoveTemp(StreamableHandle);
				ClassesToLoad.Add(WidgetClass, MoveTemp(NewItem));
			}
		}
		else
		{
			ensureMsgf(false, TEXT("The widget '%s' doesn't have it's WidgetClass property set."), *Entry.Child->GetClass()->GetName());
		}
	}
	else
	{
		NetReplicationPending.Add(Entry.ReplicationID);
	}

	PrimaryComponentTick.SetTickFunctionEnable(NetReplicationPending.Num() > 0 || AddPending.Num() > 0 || ClassesToLoad.Num() > 0);
}

void UUIFrameworkPlayerComponent::LocalWidgetRemovedFromTree(const FUIFrameworkWidgetTreeEntry& Entry)
{
	APlayerController* LocalOwner = GetPlayerController();
	check(!LocalOwner->HasAuthority());
	check(!bAddingWidget);

	NetReplicationPending.Remove(Entry.ReplicationID);
	AddPending.Remove(Entry.ReplicationID);

	PrimaryComponentTick.SetTickFunctionEnable(NetReplicationPending.Num() > 0 || AddPending.Num() > 0 || ClassesToLoad.Num() > 0);
}

void UUIFrameworkPlayerComponent::LocalRemoveWidgetRootFromTree(const UUIFrameworkWidget* Widget)
{
	check(Widget);
	ServerRemoveWidgetRootFromTree(Widget->GetWidgetId());
}

void UUIFrameworkPlayerComponent::ServerRemoveWidgetRootFromTree_Implementation(FUIFrameworkWidgetId WidgetId)
{
	if (UUIFrameworkWidget* Widget = GetWidgetTree().FindWidgetById(WidgetId))
	{
		GetWidgetTree().AuthorityRemoveWidgetAndChildren(Widget);
	}
}

void UUIFrameworkPlayerComponent::LocalOnClassLoaded(TSoftClassPtr<UWidget> WidgetClass)
{
	FWidgetClassToLoad FoundWidgetClassToLoad;
	if (ClassesToLoad.RemoveAndCopyValue(WidgetClass, FoundWidgetClassToLoad))
	{
		if (WidgetClass.Get())
		{
			for (int32 ReplicationId : FoundWidgetClassToLoad.EntryReplicationIds)
			{
				if (const FUIFrameworkWidgetTreeEntry* Entry = WidgetTree.LocalGetEntryByReplicationId(ReplicationId))
				{
					if (Entry->IsParentValid() && Entry->IsChildValid())
					{
						Entry->Child->LocalCreateUMGWidget(this);
						AddPending.Add(ReplicationId);
						NetReplicationPending.Remove(ReplicationId);
					}
					else
					{
						NetReplicationPending.Add(ReplicationId);
					}
				}
				else
				{
					UE_LOG(LogUIFramework, Log, TEXT("A widget with class %s was removed."), *WidgetClass.Get()->GetName());
				}
			}
		}
		else
		{
			UE_LOG(LogUIFramework, Error, TEXT("Failed to load widget class %s."), *WidgetClass.ToString());
			ensure(false);
		}
	}
	else
	{
		UE_LOG(LogUIFramework, Log, TEXT("A load request for class %s was not found but could had be removed."), *WidgetClass.Get()->GetName());
	}

	PrimaryComponentTick.SetTickFunctionEnable(NetReplicationPending.Num() > 0 || AddPending.Num() > 0 || ClassesToLoad.Num() > 0);
}

void UUIFrameworkPlayerComponent::LocalAddChild(FUIFrameworkWidgetId WidgetId)
{
	UUIFrameworkWidget* Widget = GetWidgetTree().FindWidgetById(WidgetId);
	if (FUIFrameworkGameLayerSlot* LayerEntry = RootList.FindEntry(WidgetId))
	{
		if (Widget)
		{
			UWidget* UMGWidget = Widget->LocalGetUMGWidget();
			if (ensure(UMGWidget))
			{
				UMGWidget->RemoveFromParent();
				LayerEntry->LocalAquireWidget();
				check(Presenter);
				Presenter->AddToViewport(UMGWidget, *LayerEntry);
			}
		}
		else
		{
			UE_LOG(LogUIFramework, Log, TEXT("The widget '%" INT64_FMT "' doesn't exist in the WidgetTree."), WidgetId.GetKey());
		}
	}
	else
	{
		if (Widget && Widget->LocalGetUMGWidget())
		{
			Widget->LocalGetUMGWidget()->RemoveFromParent();
		}
		UE_LOG(LogUIFramework, Verbose, TEXT("The widget '%" INT64_FMT "' was not found in the RootList."), WidgetId.GetKey());
	}
}

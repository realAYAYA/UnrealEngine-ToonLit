// Copyright Epic Games, Inc. All Rights Reserved.

#include "UIFPlayerComponent.h"
#include "UIFLog.h"
#include "UIFWidget.h"
#include "Blueprint/GameViewportSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/ActorChannel.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"


/**
 *
 */
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


/**
 *
 */
UUIFrameworkPlayerComponent::UUIFrameworkPlayerComponent()
	: RootList(this)
	, WidgetTree(this)
{
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_DuringPhysics;
}

void UUIFrameworkPlayerComponent::UninitializeComponent()
{
	// On local, remove all UWidget.
	if (!GetOwner()->HasAuthority())
	{
		for (FUIFrameworkGameLayerSlot& Entry : RootList.Entries)
		{
			if (UUIFrameworkWidget* ChildWidget = GetWidgetTree().FindWidgetById(Entry.GetWidgetId()))
			{
				ChildWidget->LocalDestroyUMGWidget();
			}
		}
	}
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
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	WroteSomething |= WidgetTree.ReplicateSubWidgets(Channel, Bunch, RepFlags);
	return WroteSomething;
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
		UUIFrameworkPlayerComponent* PreviousOwner = InEntry.AuthorityGetWidget()->GetPlayerComponent();
		if (PreviousOwner != nullptr && PreviousOwner != this)
		{
			FFrame::KismetExecutionMessage(TEXT("The widget was created for another player. It can't be added."), ELogVerbosity::Warning, "InvalidPlayerParent");
		}
		else
		{
			InEntry.AuthoritySetWidget(InEntry.AuthorityGetWidget()); // to make sure the id is set
			InEntry.AuthorityGetWidget()->AuthoritySetParent(this, FUIFrameworkParentWidget(this));
			RootList.AddEntry(InEntry);
		}
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
		UUIFrameworkPlayerComponent* PreviousOwner = Widget->GetPlayerComponent();
		if (PreviousOwner != this)
		{
			FFrame::KismetExecutionMessage(TEXT("The widget was created for another player. It can't be removed on this player."), ELogVerbosity::Warning, "InvalidPlayerParentOnRemovedWidget");
		}
		else
		{
			RootList.RemoveEntry(Widget);
			Widget->AuthoritySetParent(this, FUIFrameworkParentWidget());
		}
	}
}

void UUIFrameworkPlayerComponent::AuthorityRemoveChild(UUIFrameworkWidget* Widget)
{
	check(Widget);
	RootList.RemoveEntry(Widget);
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
			if (FUIFrameworkWidgetTreeEntry* Entry = WidgetTree.GetEntryByReplicationId(ReplicationId))
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
			if (const FUIFrameworkWidgetTreeEntry* Entry = WidgetTree.GetEntryByReplicationId(ReplicationId))
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

	if (Entry.Child)
	{
		TSoftClassPtr<UWidget> WidgetClass = Entry.Child->GetUMGWidgetClass();
		if (WidgetClass.Get() != nullptr)
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

void UUIFrameworkPlayerComponent::LocalOnClassLoaded(TSoftClassPtr<UWidget> WidgetClass)
{
	FWidgetClassToLoad FoundWidgetClassToLoad;
	if (ClassesToLoad.RemoveAndCopyValue(WidgetClass, FoundWidgetClassToLoad))
	{
		if (WidgetClass.Get())
		{
			for (int32 ReplicationId : FoundWidgetClassToLoad.EntryReplicationIds)
			{
				if (const FUIFrameworkWidgetTreeEntry* Entry = WidgetTree.GetEntryByReplicationId(ReplicationId))
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
			ensureMsgf(false, TEXT("Load request failed"));
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
				if (UGameViewportSubsystem* Subsystem = UGameViewportSubsystem::Get(GetWorld()))
				{
					LayerEntry->LocalAquireWidget();

					FGameViewportWidgetSlot GameViewportWidgetSlot;
					GameViewportWidgetSlot.ZOrder = LayerEntry->ZOrder;
					if (LayerEntry->Type == EUIFrameworkGameLayerType::Viewport)
					{
						Subsystem->AddWidget(UMGWidget, GameViewportWidgetSlot);
					}
					else
					{
						APlayerController* LocalOwner = GetPlayerController();
						check(LocalOwner);
						Subsystem->AddWidgetForPlayer(UMGWidget, LocalOwner->GetLocalPlayer(), GameViewportWidgetSlot);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogUIFramework, Log, TEXT("The widget '%" INT64_FMT "' doesn't exist in the WidgetTree."), WidgetId.GetKey());
			if (Widget && Widget->LocalGetUMGWidget())
			{
				Widget->LocalGetUMGWidget()->RemoveFromParent();
			}
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

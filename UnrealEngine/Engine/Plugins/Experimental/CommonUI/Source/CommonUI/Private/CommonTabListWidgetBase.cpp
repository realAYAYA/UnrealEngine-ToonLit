// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonTabListWidgetBase.h"
#include "CommonUIPrivate.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonUISubsystemBase.h"
#include "CommonUIUtils.h"
#include "Containers/Ticker.h"
#include "Groups/CommonButtonGroupBase.h"
#include "ICommonUIModule.h"
#include "Input/CommonUIInputTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonTabListWidgetBase)

UCommonTabListWidgetBase::UCommonTabListWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAutoListenForInput(false)
	, bDeferRebuildingTabList(false)
	, TabButtonGroup(nullptr)
	, bIsListeningForInput(false)
	, RegisteredTabsByID()
	, ActiveTabID(NAME_None)
	, bIsRebuildingList(false)
	, bPendingRebuild(false)
{
}
 
void UCommonTabListWidgetBase::SetLinkedSwitcher(UCommonAnimatedSwitcher* CommonSwitcher)
{
	if (LinkedSwitcher.Get() != CommonSwitcher)
	{
		HandlePreLinkedSwitcherChanged();
		LinkedSwitcher = CommonSwitcher;
		HandlePostLinkedSwitcherChanged();
	}
}

UCommonAnimatedSwitcher* UCommonTabListWidgetBase::GetLinkedSwitcher() const
{
	return LinkedSwitcher.Get();
}

bool UCommonTabListWidgetBase::RegisterTab(FName TabNameID, TSubclassOf<UCommonButtonBase> ButtonWidgetType, UWidget* ContentWidget, const int32 TabIndex /*= INDEX_NONE*/)
{
	bool bAreParametersValid = true;

	// Early out on redundant tab registration.
	if (!ensure(!RegisteredTabsByID.Contains(TabNameID)))
	{
		bAreParametersValid = false;
	}

	// Early out on invalid tab button type.
	if (!ensure(ButtonWidgetType))
	{
		bAreParametersValid = false;
	}
	
	// NOTE: Adding the button to the group may change it's selection, which raises an event we listen to,
	// which can only properly be handled if we already know that this button is associated with a registered tab.
	if (!ensure(TabButtonGroup))
	{
		bAreParametersValid = false;
	}

	if (!bAreParametersValid)
	{
		return false;
	}

	UCommonButtonBase* const NewTabButton = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonWidgetType);
	if (!ensureMsgf(NewTabButton, TEXT("Failed to create tab button. Aborting tab registration.")))
	{
		return false;
	}

	const int32 NumRegisteredTabs = RegisteredTabsByID.Num();
	const int32 NewTabIndex = (TabIndex == INDEX_NONE) ? NumRegisteredTabs : FMath::Clamp(TabIndex, 0, NumRegisteredTabs);

	// If the new tab is being inserted before the end of the list, we need to rebuild the tab list.
	const bool bRequiresRebuild = (NewTabIndex < NumRegisteredTabs);

	if (bRequiresRebuild)
	{
		for (TPair<FName, FCommonRegisteredTabInfo>& Pair : RegisteredTabsByID)
		{
			if (NewTabIndex <= Pair.Value.TabIndex)
			{
				// Increment this tab's index as we are inserting the new tab before it.
				Pair.Value.TabIndex++;
			}
		}
	}

	// Tab book-keeping.
	FCommonRegisteredTabInfo NewTabInfo;
	NewTabInfo.TabIndex = NewTabIndex;
	NewTabInfo.TabButton = NewTabButton;
	NewTabInfo.ContentInstance = ContentWidget;
	RegisteredTabsByID.Add(TabNameID, NewTabInfo);

	// Enforce the "contract" that tab buttons require - single-selectability, but not toggleability.
	NewTabButton->SetIsSelectable(true);
	NewTabButton->SetIsToggleable(false);

	TabButtonGroup->AddWidget(NewTabButton);
	HandleTabCreation(TabNameID, NewTabInfo.TabButton);
	
	OnTabButtonCreation.Broadcast(TabNameID, NewTabInfo.TabButton);
	
	if (bRequiresRebuild)
	{
		if (bDeferRebuildingTabList)
		{
			if (!bPendingRebuild)
			{
				bPendingRebuild = true;

				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCommonTabListWidgetBase::DeferredRebuildTabList));
			}
		}
		else
		{
			RebuildTabList();
		}
	}

	return true;
}

bool UCommonTabListWidgetBase::RemoveTab(FName TabNameID)
{
	FCommonRegisteredTabInfo* const TabInfo = RegisteredTabsByID.Find(TabNameID);
	if (!TabInfo)
	{
		return false;
	}

	UCommonButtonBase* const TabButton = TabInfo->TabButton;
	if (TabButton)
	{
		TabButtonGroup->RemoveWidget(TabButton);
		TabButton->RemoveFromParent();
	}
	RegisteredTabsByID.Remove(TabNameID);

	// Callbacks
	HandleTabRemoval(TabNameID, TabButton);
	OnTabButtonRemoval.Broadcast(TabNameID, TabButton);

	return true;
}

void UCommonTabListWidgetBase::RemoveAllTabs()
{
	for (TMap<FName, FCommonRegisteredTabInfo>::TIterator Iter(RegisteredTabsByID); Iter; ++Iter)
	{
		RemoveTab(Iter->Key);
	}
}

int32 UCommonTabListWidgetBase::GetTabCount() const
{
	// TODO Should we ensure all the tabs in the list are valid?
	return RegisteredTabsByID.Num();
}

void UCommonTabListWidgetBase::SetListeningForInput(bool bShouldListen)
{
	if (bShouldListen && !TabButtonGroup)
	{
		// If there's no tab button group, it means we haven't been constructed and we shouldn't listen to anything
		return;
	}

	if (GetUISubsystem() == nullptr)
	{
		// Shutting down
		return;
	}

	if (bShouldListen != bIsListeningForInput)
	{
		bIsListeningForInput = bShouldListen;
		UpdateBindings();
	}
}

void UCommonTabListWidgetBase::UpdateBindings()
{
	// New input system binding flow
	if (bIsListeningForInput)
	{
		NextTabActionHandle = RegisterUIActionBinding(FBindUIActionArgs(NextTabInputActionData, false, FSimpleDelegate::CreateUObject(this, &UCommonTabListWidgetBase::HandleNextTabAction)));
		PrevTabActionHandle = RegisterUIActionBinding(FBindUIActionArgs(PreviousTabInputActionData, false, FSimpleDelegate::CreateUObject(this, &UCommonTabListWidgetBase::HandlePreviousTabAction)));
	}
	else
	{
		NextTabActionHandle.Unregister();
		PrevTabActionHandle.Unregister();
	}
}

bool UCommonTabListWidgetBase::IsRebuildingList() const
{
	return bIsRebuildingList;
}

bool UCommonTabListWidgetBase::SelectTabByID(FName TabNameID, bool bSuppressClickFeedback)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Key == TabNameID && ensure(TabPair.Value.TabButton))
		{
			TabPair.Value.TabButton->SetIsSelected(true, !bSuppressClickFeedback);
			return true;
		}
	}

	return false;
}

FName UCommonTabListWidgetBase::GetSelectedTabId() const
{
	FName FoundId = NAME_None;

	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Value.TabButton != nullptr && TabPair.Value.TabButton->GetSelected())
		{
			FoundId = TabPair.Key;
			break;
		}
	}

	return FoundId;
}

FName UCommonTabListWidgetBase::GetTabIdAtIndex(int32 Index) const
{
	FName FoundId = NAME_None;

	if (ensure(Index < RegisteredTabsByID.Num()))
	{
		for (auto& TabPair : RegisteredTabsByID)
		{
			if (TabPair.Value.TabIndex == Index)
			{
				FoundId = TabPair.Key;
				break;
			}
		}
	}

	return FoundId;
}

void UCommonTabListWidgetBase::SetTabVisibility(FName TabNameID, ESlateVisibility NewVisibility)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Key == TabNameID && ensure(TabPair.Value.TabButton))
		{
			TabPair.Value.TabButton->SetVisibility(NewVisibility);
			
			if (NewVisibility == ESlateVisibility::Collapsed || NewVisibility == ESlateVisibility::Hidden)
			{
				TabPair.Value.TabButton->SetIsInteractionEnabled(false);
			}
			else
			{
				TabPair.Value.TabButton->SetIsInteractionEnabled(true);
			}
			
			break;
		}
	}
}

void UCommonTabListWidgetBase::SetTabEnabled(FName TabNameID, bool bEnable)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Key == TabNameID && ensure(TabPair.Value.TabButton))
		{
			if (bEnable)
			{
				TabPair.Value.TabButton->SetIsEnabled(true);
			}
			else
			{
				TabPair.Value.TabButton->SetIsEnabled(false);
			}

			break;
		}
	}
}

void UCommonTabListWidgetBase::SetTabInteractionEnabled(FName TabNameID, bool bEnable)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Key == TabNameID && ensure(TabPair.Value.TabButton))
		{
			if (bEnable)
			{
				TabPair.Value.TabButton->SetIsInteractionEnabled(true);
			}
			else
			{
				TabPair.Value.TabButton->SetIsInteractionEnabled(false);
			}

			break;
		}
	}
}

void UCommonTabListWidgetBase::DisableTabWithReason(FName TabNameID, const FText& Reason)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Key == TabNameID && ensure(TabPair.Value.TabButton))
		{
			TabPair.Value.TabButton->DisableButtonWithReason(Reason);
			break;
		}
	}
}

UCommonButtonBase* UCommonTabListWidgetBase::GetTabButtonBaseByID(FName TabNameID)
{
	if (FCommonRegisteredTabInfo* TabInfo = RegisteredTabsByID.Find(TabNameID))
	{
		return TabInfo->TabButton;
	}

	return nullptr;
}

void UCommonTabListWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Create the button group once up-front
	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true);
	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UCommonTabListWidgetBase::HandleTabButtonSelected);
}

void UCommonTabListWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (bAutoListenForInput)
	{
		SetListeningForInput(true);
	}
}

void UCommonTabListWidgetBase::NativeDestruct()
{
	Super::NativeDestruct();

	SetListeningForInput(false);

	ActiveTabID = NAME_None;
	RemoveAllTabs();
	if (TabButtonGroup)
	{
		TabButtonGroup->RemoveAll();
	}
}

void UCommonTabListWidgetBase::HandlePreLinkedSwitcherChanged()
{
	HandlePreLinkedSwitcherChanged_BP();
}

void UCommonTabListWidgetBase::HandlePostLinkedSwitcherChanged()
{
	HandlePostLinkedSwitcherChanged_BP();
}

void UCommonTabListWidgetBase::HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
}

void UCommonTabListWidgetBase::HandleTabRemoval_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
}

const TMap<FName, FCommonRegisteredTabInfo>& UCommonTabListWidgetBase::GetRegisteredTabsByID() const
{
	return RegisteredTabsByID;
}

void UCommonTabListWidgetBase::HandleTabButtonSelected(UCommonButtonBase* SelectedTabButton, int32 ButtonIndex)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		FCommonRegisteredTabInfo& TabInfo = TabPair.Value;
			
		if (TabInfo.TabButton == SelectedTabButton)
		{
			ActiveTabID = TabPair.Key;

			if (TabInfo.ContentInstance || LinkedSwitcher.IsValid())
			{
				if (ensureMsgf(TabInfo.ContentInstance, TEXT("A CommonTabListWidget tab button lacks a tab content widget to set its linked switcher to.")) &&
					ensureMsgf(LinkedSwitcher.IsValid(), TEXT("A CommonTabListWidgetBase.has a registered tab with a content widget to switch to, but has no linked activatable widget switcher. Did you forget to call SetLinkedSwitcher to establish the association?")))
				{
					// There's already an instance of the widget to display, so go for it
					LinkedSwitcher->SetActiveWidget(TabInfo.ContentInstance);
				}
			}

			OnTabSelected.Broadcast(TabPair.Key);
		}
	}
}

void UCommonTabListWidgetBase::HandleNextTabInputAction(bool& bPassThrough)
{
	HandleNextTabAction();
}

void UCommonTabListWidgetBase::HandleNextTabAction()
{
	if (ensure(TabButtonGroup))
	{
		TabButtonGroup->SelectNextButton();
	}
}

void UCommonTabListWidgetBase::HandlePreviousTabInputAction(bool& bPassThrough)
{
	HandlePreviousTabAction();
}

void UCommonTabListWidgetBase::HandlePreviousTabAction()
{
	if (ensure(TabButtonGroup))
	{
		TabButtonGroup->SelectPreviousButton();
	}
}

bool UCommonTabListWidgetBase::DeferredRebuildTabList(float DeltaTime)
{
	bPendingRebuild = false;
	RebuildTabList();
	return false;
}

void UCommonTabListWidgetBase::RebuildTabList()
{
	bIsRebuildingList = true;

	// Copy the registered tabs (as we are about to clear them) and sort by TabIndex.
	TMap<FName, FCommonRegisteredTabInfo> SortedRegisteredTabsByID = RegisteredTabsByID;
	SortedRegisteredTabsByID.ValueSort([](const FCommonRegisteredTabInfo& TabInfoA, const FCommonRegisteredTabInfo& TabInfoB)
		{
			return (TabInfoA.TabIndex < TabInfoB.TabIndex);
		});

	// Keep track of the current ActiveTabID so we can restore it after the list is rebuilt.
	const FName CurrentActiveTabID = ActiveTabID;

	// Disable selection required temporarily so we can deselect everything, rebuild the list, then select the tab we want.
	TabButtonGroup->SetSelectionRequired(false);
	TabButtonGroup->DeselectAll();
	RemoveAllTabs();

	RegisteredTabsByID = SortedRegisteredTabsByID;

	for (TPair<FName, FCommonRegisteredTabInfo>& Pair : RegisteredTabsByID)
	{
		TabButtonGroup->AddWidget(Pair.Value.TabButton);
		HandleTabCreation(Pair.Key, Pair.Value.TabButton);
	}

	bIsRebuildingList = false;

	constexpr bool bSuppressClickFeedback = true;
	SelectTabByID(CurrentActiveTabID, bSuppressClickFeedback);

	TabButtonGroup->SetSelectionRequired(true);
	OnTabListRebuilt.Broadcast();
}


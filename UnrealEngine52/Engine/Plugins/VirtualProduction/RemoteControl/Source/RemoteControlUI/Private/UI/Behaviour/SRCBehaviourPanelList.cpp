﻿// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCBehaviourPanelList.h"

#include "Behaviour/Builtin/Bind/RCBehaviourBind.h"
#include "Behaviour/Builtin/Conditional/RCBehaviourConditional.h"
#include "Behaviour/Builtin/Path/RCSetAssetByPathBehaviour.h"
#include "Controller/RCController.h"
#include "RCBehaviourModel.h"
#include "RemoteControlPreset.h"
#include "SRCBehaviourPanel.h"
#include "SlateOptMacros.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/Action/SRCActionPanel.h"
#include "UI/Behaviour/Builtin/Bind/RCBehaviourBindModel.h"
#include "UI/Behaviour/Builtin/Conditional/RCBehaviourConditionalModel.h"
#include "UI/Behaviour/Builtin/Path/RCBehaviourSetAssetByPathModel.h"
#include "UI/Controller/RCControllerModel.h"
#include "UI/Behaviour/Builtin/RangeMap/RCBehaviourRangeMapModel.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRemoteControlPanel.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "RemoteControlPanelBehavioursList"

namespace FRemoteControlBehaviourColumns
{
	const FName Behaviours = TEXT("Behaviors");
}

void SRCBehaviourPanelList::Construct(const FArguments& InArgs, TSharedRef<SRCBehaviourPanel> InBehaviourPanel, TSharedPtr<FRCControllerModel> InControllerItem, const TSharedRef<SRemoteControlPanel> InRemoteControlPanel)
{
	SRCLogicPanelListBase::Construct(SRCLogicPanelListBase::FArguments(), InBehaviourPanel, InRemoteControlPanel);
	
	BehaviourPanelWeakPtr = InBehaviourPanel;
	ControllerItemWeakPtr = InControllerItem;
	
	RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.MinorPanel");

	ListView = SNew(SListView<TSharedPtr<FRCBehaviourModel>>)
		.ListItemsSource(&BehaviourItems)
		.OnSelectionChanged(this, &SRCBehaviourPanelList::OnTreeSelectionChanged)
		.OnGenerateRow(this, &SRCBehaviourPanelList::OnGenerateWidgetForList)
		.ListViewStyle(&RCPanelStyle->TableViewStyle)
		.OnContextMenuOpening(this, &SRCLogicPanelListBase::GetContextMenuWidget)
		.HeaderRow(
			SNew(SHeaderRow)
			.Style(&RCPanelStyle->HeaderRowStyle)

			+ SHeaderRow::Column(FRemoteControlBehaviourColumns::Behaviours)
			.DefaultLabel(LOCTEXT("RCBehaviourColumnHeader", "Behaviors"))
			.FillWidth(1.f)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)
		);
	
	ChildSlot
	[
		ListView.ToSharedRef()
	];

	// Add delegates
	const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = InBehaviourPanel->GetRemoteControlPanel();
	check(RemoteControlPanel)
	RemoteControlPanel->OnBehaviourAdded.AddSP(this, &SRCBehaviourPanelList::OnBehaviourAdded);
	RemoteControlPanel->OnEmptyBehaviours.AddSP(this, &SRCBehaviourPanelList::OnEmptyBehaviours);

	if (URCController* Controller = Cast<URCController>(InControllerItem->GetVirtualProperty()))
	{
		Controller->OnBehaviourListModified.AddSP(this, &SRCBehaviourPanelList::OnBehaviourListModified);
	}

	Reset();
}

bool SRCBehaviourPanelList::IsEmpty() const
{
	return BehaviourItems.IsEmpty();
}

int32 SRCBehaviourPanelList::Num() const
{
	return BehaviourItems.Num();
}

int32 SRCBehaviourPanelList::NumSelectedLogicItems() const
{
	return ListView->GetNumItemsSelected();
}

void SRCBehaviourPanelList::AddNewLogicItem(UObject* InLogicItem)
{
	AddBehaviourToList(Cast<URCBehaviour>(InLogicItem));

	RequestRefresh();
}

void SRCBehaviourPanelList::AddSpecialContextMenuOptions(FMenuBuilder& MenuBuilder)
{
	if (TSharedPtr<FRCBehaviourModel> BehaviourItem = GetSelectedBehaviourItem())
	{
		const bool bIsEnabled = BehaviourItem->IsBehaviourEnabled();

		FUIAction Action(FExecuteAction::CreateSP(this, &SRCBehaviourPanelList::SetIsBehaviourEnabled, !bIsEnabled));

		FText Label, Tooltip;
		if (bIsEnabled)
		{
			Label = LOCTEXT("LabelDisableBehaviour", "Disable");
			Tooltip = LOCTEXT("TooltipDisableBehaviour", "Disables the current behaviour. Actions will not be processed for this behaviour when the Controller value changes");
		}
		else
		{
			Label = LOCTEXT("LabelEnableBehaviour", "Enable");
			Tooltip = LOCTEXT("TooltipEnableBehaviour", "Enables the current behaviour. Restores functioning of Actions associated with this behaviour.");
		}

		MenuBuilder.AddMenuEntry(Label, Tooltip, FSlateIcon(), Action);
	}
}

void SRCBehaviourPanelList::SetIsBehaviourEnabled(const bool bIsEnabled)
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		if (TSharedPtr<SRCActionPanel> ActionPanel = RemoteControlPanel->GetLogicActionPanel())
		{
			ActionPanel->SetIsBehaviourEnabled(bIsEnabled);
		}
	}
}

void SRCBehaviourPanelList::AddBehaviourToList(URCBehaviour* InBehaviour)
{
	if (!BehaviourPanelWeakPtr.IsValid())
	{
		return;
	}

	const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = BehaviourPanelWeakPtr.Pin()->GetRemoteControlPanel();

	if (URCBehaviourConditional* ConditionalBehaviour = Cast<URCBehaviourConditional>(InBehaviour))
	{
		BehaviourItems.Add(MakeShared<FRCBehaviourConditionalModel>(ConditionalBehaviour));
	}
	else if (URCSetAssetByPathBehaviour* SetAssetByPathBehaviour = Cast<URCSetAssetByPathBehaviour>(InBehaviour))
	{
		BehaviourItems.Add(MakeShared<FRCSetAssetByPathBehaviourModel>(SetAssetByPathBehaviour));
	}
	else if (URCBehaviourBind* BindBehaviour = Cast<URCBehaviourBind>(InBehaviour))
	{
		BehaviourItems.Add(MakeShared<FRCBehaviourBindModel>(BindBehaviour, RemoteControlPanel));
	}
	else if (URCRangeMapBehaviour* RangeMapBehaviour = Cast<URCRangeMapBehaviour>(InBehaviour))
	{
		BehaviourItems.Add(MakeShared<FRCRangeMapBehaviourModel>(RangeMapBehaviour));
	}
	else
	{
		BehaviourItems.Add(MakeShared<FRCBehaviourModel>(InBehaviour, RemoteControlPanel));
	}
}



void SRCBehaviourPanelList::Reset()
{
	BehaviourItems.Empty();

	if (TSharedPtr<FRCControllerModel> ControllerItem = ControllerItemWeakPtr.Pin())
	{
		if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
		{
			for (URCBehaviour* Behaviour : Controller->Behaviours)
			{
				AddBehaviourToList(Behaviour);
			}
		}
	}

	ListView->RebuildList();
}

TSharedRef<ITableRow> SRCBehaviourPanelList::OnGenerateWidgetForList(TSharedPtr<FRCBehaviourModel> InItem,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		.Style(&RCPanelStyle->TableRowStyle)
		[
			InItem->GetWidget()
		];
}

void SRCBehaviourPanelList::OnTreeSelectionChanged(TSharedPtr<FRCBehaviourModel> InItem, ESelectInfo::Type)
{
	if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = BehaviourPanelWeakPtr.Pin()->GetRemoteControlPanel())
	{
		if (InItem != SelectedBehaviourItemWeakPtr.Pin())
		{
			SelectedBehaviourItemWeakPtr = InItem;

			RemoteControlPanel->OnBehaviourSelectionChanged.Broadcast(InItem);
		}

		if (const TSharedPtr<FRCControllerModel> ControllerItem = ControllerItemWeakPtr.Pin())
		{
			ControllerItem->UpdateSelectedBehaviourModel(InItem);
		}
	}
}

void SRCBehaviourPanelList::OnBehaviourAdded(const URCBehaviour* InBehaviour)
{
	Reset();
}

void SRCBehaviourPanelList::OnEmptyBehaviours()
{
	if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = BehaviourPanelWeakPtr.Pin()->GetRemoteControlPanel())
	{
		RemoteControlPanel->OnBehaviourSelectionChanged.Broadcast(nullptr);	
	}
	
	Reset();
}

void SRCBehaviourPanelList::BroadcastOnItemRemoved()
{
	if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = BehaviourPanelWeakPtr.Pin()->GetRemoteControlPanel())
	{
		RemoteControlPanel->OnBehaviourSelectionChanged.Broadcast(nullptr);
	}
}

URemoteControlPreset* SRCBehaviourPanelList::GetPreset()
{
	if (BehaviourPanelWeakPtr.IsValid())
	{
		return BehaviourPanelWeakPtr.Pin()->GetPreset();
	}

	return nullptr;
}

int32 SRCBehaviourPanelList::RemoveModel(const TSharedPtr<FRCLogicModeBase> InModel)
{
	if (const TSharedPtr<FRCControllerModel> ControllerModel = ControllerItemWeakPtr.Pin())
	{
		if (URCController* Controller = Cast<URCController>(ControllerModel->GetVirtualProperty()))
		{
			if(const TSharedPtr<FRCBehaviourModel> SelectedBehaviour = StaticCastSharedPtr<FRCBehaviourModel>(InModel))
			{
				FScopedTransaction Transaction(LOCTEXT("RemoveBehaviour", "Remove Behaviour"));
				Controller->Modify();

				// Remove Model from Data Container
				const int32 RemoveCount = Controller->RemoveBehaviour(SelectedBehaviour->GetBehaviour());

				return RemoveCount;
			}
		}
	}

	return 0;
}

void SRCBehaviourPanelList::OnBehaviourListModified()
{
	Reset();
}

bool SRCBehaviourPanelList::IsListFocused() const
{
	return ListView->HasAnyUserFocus().IsSet() || ContextMenuWidgetCached.IsValid();;
}

void SRCBehaviourPanelList::DeleteSelectedPanelItem()
{
	DeleteItemFromLogicPanel<FRCBehaviourModel>(BehaviourItems, ListView->GetSelectedItems());
}

void SRCBehaviourPanelList::RequestRefresh()
{
	ListView->RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE
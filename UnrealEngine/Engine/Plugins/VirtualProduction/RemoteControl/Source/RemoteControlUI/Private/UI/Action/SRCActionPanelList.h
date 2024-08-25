// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Action/RCAction.h"
#include "Action/RCActionContainer.h"
#include "Action/RCFunctionAction.h"
#include "Action/RCPropertyAction.h"
#include "RCActionModel.h"
#include "RemoteControlPreset.h"
#include "SlateOptMacros.h"
#include "SRCActionPanel.h"
#include "SDropTarget.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/Action/Bind/RCActionBindModel.h"
#include "UI/Action/Conditional/RCActionConditionalModel.h"
#include "UI/BaseLogicUI/SRCLogicPanelListBase.h"
#include "UI/Behaviour/RCBehaviourModel.h"
#include "UI/RCUIHelpers.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRCPanelExposedField.h"
#include "UI/SRCPanelFieldGroup.h"
#include "UI/SRemoteControlPanel.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "SRCActionPanelList"

class FRCActionModel;

/*
* ~ SRCActionPanelList ~
*
* UI Widget for Actions List
* Used as part of the RC Logic Actions Panel.
*/
template <typename ActionType>
class REMOTECONTROLUI_API SRCActionPanelList : public SRCLogicPanelListBase
{
public:
	SLATE_BEGIN_ARGS(SRCActionPanelList<ActionType>)
		{
		}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<SRCActionPanel> InActionPanel, TSharedPtr<FRCBehaviourModel> InBehaviourItem)
	{
		const TSharedPtr<SRemoteControlPanel>& RemoteControlPanel = InActionPanel->GetRemoteControlPanel();
		check(RemoteControlPanel);

		SRCLogicPanelListBase::Construct(SRCLogicPanelListBase::FArguments(), InActionPanel, RemoteControlPanel.ToSharedRef());

		ActionPanelWeakPtr = InActionPanel;
		BehaviourItemWeakPtr = InBehaviourItem;

		RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.MinorPanel");

		ListView = SNew(SListView<TSharedPtr<ActionType>>)
			.ListItemsSource(&ActionItems)
			.SelectionMode(ESelectionMode::Multi)
			.HeaderRow(ActionType::GetHeaderRow())
			.OnGenerateRow(this, &SRCActionPanelList::OnGenerateWidgetForList)
			.OnSelectionChanged(this, &SRCActionPanelList::OnSelectionChanged)
			.OnContextMenuOpening(this, &SRCLogicPanelListBase::GetContextMenuWidget);

		ChildSlot
			[
				SNew(SDropTarget)
				.VerticalImage(FRemoteControlPanelStyle::Get()->GetBrush("RemoteControlPanel.VerticalDash"))
				.HorizontalImage(FRemoteControlPanelStyle::Get()->GetBrush("RemoteControlPanel.HorizontalDash"))
				.OnDropped_Lambda([this](const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent) { return SRCActionPanelList::OnExposedFieldDrop(InDragDropEvent.GetOperation()); })
				.OnAllowDrop(this, &SRCActionPanelList::OnAllowDrop)
				.OnIsRecognized(this, &SRCActionPanelList::OnAllowDrop)
				[
					ListView.ToSharedRef()
				]
			];

		// Add delegates
		RemoteControlPanel->OnActionAdded.AddSP(this, &SRCActionPanelList::OnActionAdded);
		RemoteControlPanel->OnEmptyActions.AddSP(this, &SRCActionPanelList::OnEmptyActions);

		if (URCBehaviour* Behaviour = InBehaviourItem->GetBehaviour())
		{
			if (Behaviour->ActionContainer)
			{
				Behaviour->ActionContainer->OnActionsListModified.AddSP(this, &SRCActionPanelList::OnActionsListModified);
			}
		}

		Reset();

		if (URemoteControlPreset* Preset = RemoteControlPanel->GetPreset())
		{
			Preset->GetPropertyIdRegistry()->OnPropertyIdActionNeedsRefresh().AddSP(this, &SRCActionPanelList::Refresh);
		}
	}

	virtual ~SRCActionPanelList()
	{
		if (ActionPanelWeakPtr.IsValid())
		{
			if (URemoteControlPreset* Preset = ActionPanelWeakPtr.Pin()->GetPreset())
			{
				Preset->GetPropertyIdRegistry()->OnPropertyIdActionNeedsRefresh().RemoveAll(this);
			}
		}
	}

	void OnActionsListModified()
	{
		Reset();
	}

	/** Returns true if the underlying list is valid and empty. */
	virtual bool IsEmpty() const override
	{
		return ActionItems.IsEmpty();
	}

	/** Returns number of items in the list. */
	virtual int32 Num() const override
	{
		return ActionItems.Num();
	}

	/** The number of Controllers currently selected */
	virtual int32 NumSelectedLogicItems() const override
	{
		return ListView->GetNumItemsSelected();
	}

	/** Whether the Actions List View currently has focus. */
	virtual bool IsListFocused() const override
	{
		return ListView->HasAnyUserFocus().IsSet() || ContextMenuWidgetCached.IsValid();
	}

	/** Deletes currently selected items from the list view */
	virtual void DeleteSelectedPanelItems() override
	{
		DeleteItemsFromLogicPanel<ActionType>(ActionItems, ListView->GetSelectedItems());
	}

	/** Returns the UI items currently selected by the user (if any). */
	virtual TArray<TSharedPtr<FRCLogicModeBase>> GetSelectedLogicItems() override
	{
		TArray<TSharedPtr<FRCLogicModeBase>> SelectedValidLogicItems;
		if (ListView.IsValid())
		{
			TArray<TSharedPtr<ActionType>> AllSelectedLogicItems = ListView->GetSelectedItems();
			SelectedValidLogicItems.Reserve(AllSelectedLogicItems.Num());

			for (const TSharedPtr<ActionType>& LogicItem : AllSelectedLogicItems)
			{
				if (LogicItem.IsValid())
				{
					SelectedValidLogicItems.Add(LogicItem);
				}
			}
		}
		return SelectedValidLogicItems;
	}

	/** The currently selected Action item */
	TSharedPtr<FRCActionModel> GetSelectedActionItem()
	{
		return SelectedActionItem;
	}

	/** Fetches the parent Action panel */
	TSharedPtr<SRCActionPanel> GetActionPanel()
	{
		return ActionPanelWeakPtr.Pin();
	}

	/** Fetches the Behaviour (UI model) associated with us */
	TSharedPtr<FRCBehaviourModel> GetBehaviourItem()
	{
		return BehaviourItemWeakPtr.Pin();
	}

	/** Adds an Action by Remote Control Field Guid */
	URCAction* AddAction(const FGuid& InRemoteControlFieldId)
	{
		if (const URemoteControlPreset* Preset = GetPreset())
		{
			if (TSharedPtr<const FRemoteControlField> RemoteControlField = Preset->GetExposedEntity<FRemoteControlField>(InRemoteControlFieldId).Pin())
			{
				if (const TSharedPtr<SRCActionPanel> ActionPanel = GetActionPanel())
				{
					return ActionPanel->AddAction(RemoteControlField.ToSharedRef());
				}
			}
		}

		return nullptr;
	}

	virtual void AddNewLogicItem(UObject* InLogicItem) override
	{
		AddRowToList(Cast<URCAction>(InLogicItem));

		RequestRefresh();
	}

	void AddRowToList(URCAction* InAction)
	{
		if (!ensure(InAction))
		{
			return;
		}

		if (TSharedPtr<SRCActionPanel> ActionPanel = GetActionPanel())
		{
			if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ActionPanel->GetRemoteControlPanel())
			{
				if (TSharedPtr<FRCBehaviourModel> BehaviourItem = BehaviourItemWeakPtr.Pin())
				{
					TSharedPtr<ActionType> ActionItem = ActionType::GetModelByActionType(InAction, BehaviourItem, RemoteControlPanel);

					if (ensure(ActionItem.IsValid()))
					{
						ActionItems.Add(ActionItem);
					}
				}
			}
		}
	}

	void RequestRefresh()
	{
		ListView->RequestListRefresh();
	}

	/** Allows Logic panels to add special functionality to the Context Menu based on context */
	virtual void AddSpecialContextMenuOptions(FMenuBuilder& MenuBuilder) override
	{
		if (SelectedActionItem)
		{
			SelectedActionItem->AddSpecialContextMenuOptions(MenuBuilder);
		}
	}

private:

	/** OnGenerateRow delegate for the Actions List View */
	TSharedRef<ITableRow> OnGenerateWidgetForList( TSharedPtr<ActionType> InItem, const TSharedRef<STableViewBase>& OwnerTable )
	{
		if (ensure(InItem))
		{
			return InItem->OnGenerateWidgetForList(InItem, OwnerTable);
		}

		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
			[
				SNullWidget::NullWidget
			];
	}

	/** Responds to the selection of a newly created action. Resets UI state */
	void OnActionAdded(URCAction* InAction)
	{
		// Historical note: Previously we used to call Reset here after adding an Action
		// This is now covered by use of AddRowToList function (invoked from SRCActionPanel)
		// @todo: This improvement needs to be done for "Remove Action" as well.
	}

	/** Responds to the removal of all actions. Rests UI state */
	void OnEmptyActions()
	{
		Reset();
	}

	/** Refreshes the list from the latest state of the data model */
	virtual void Reset() override
	{
		ActionItems.Empty();

		if (TSharedPtr<FRCBehaviourModel> BehaviourItem = BehaviourItemWeakPtr.Pin())
		{
			if (URCBehaviour* Behaviour = Cast<URCBehaviour>(BehaviourItem->GetBehaviour()))
			{
				for (URCAction* Action : Behaviour->ActionContainer->GetActions())
				{
					AddRowToList(Action);
				}
			}
		}

		ListView->RebuildList();
	}
	
	/** Refreshes the list */
	void Refresh()
	{
		ListView->RebuildList();
	}

	/** Handles broadcasting of a successful remove item operation. */
	virtual void BroadcastOnItemRemoved() override {}

	/** Fetches the Remote Control preset associated with the parent panel */
	virtual URemoteControlPreset* GetPreset() override
	{
		if (ActionPanelWeakPtr.IsValid())
		{
			return ActionPanelWeakPtr.Pin()->GetPreset();
		}

		return nullptr;
	}

	/** Removes the given Action UI model item from the list of UI models */
	virtual int32 RemoveModel(const TSharedPtr<FRCLogicModeBase> InModel) override
	{
		if (const TSharedPtr<FRCBehaviourModel> BehaviourModel = BehaviourItemWeakPtr.Pin())
		{
			if (const URCBehaviour* Behaviour = BehaviourModel->GetBehaviour())
			{
				if (const TSharedPtr<ActionType> SelectedAction = StaticCastSharedPtr<ActionType>(InModel))
				{
					// Remove Model from Data Container
					FScopedTransaction Transaction(LOCTEXT("RemoveAction", "Remove Action"));
					Behaviour->ActionContainer->Modify();

					const int32 RemoveCount = Behaviour->ActionContainer->RemoveAction(SelectedAction->GetAction());

					return RemoveCount;
				}
			}
		}

		return 0;
	}

	/*Drag and drop event for creating an Action from an exposed field */
	FReply OnExposedFieldDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
	{
		if (DragDropOperation)
		{
			if (DragDropOperation->IsOfType<FExposedEntityDragDrop>())
			{
				if (TSharedPtr<FExposedEntityDragDrop> DragDropOp = StaticCastSharedPtr<FExposedEntityDragDrop>(DragDropOperation))
				{
					// Fetch the Exposed Entity
					for (const FGuid& ExposedEntityId : DragDropOp->GetSelectedIds())
					{
						if (TSharedPtr<SRCActionPanel> ActionPanel = GetActionPanel())
						{
							// Add Action
							if (ActionPanel->CanHaveActionForField(ExposedEntityId))
							{
								AddAction(ExposedEntityId);
							}
						}
					}
				}
			}
			else if (DragDropOperation->IsOfType<FFieldGroupDragDropOp>())
			{
				if (TSharedPtr<FFieldGroupDragDropOp> DragDropOp = StaticCastSharedPtr<FFieldGroupDragDropOp>(DragDropOperation))
				{
					if (URemoteControlPreset* Preset = GetPreset())
					{
						// Fetch the Group
						const FGuid GroupId = DragDropOp->GetGroupId();
						const FRemoteControlPresetGroup* Group = Preset->Layout.GetGroup(GroupId);

						if (ensure(Group))
						{
							const TArray<FGuid> GroupFields = Group->GetFields();

							// Add Action for all fields in the Group
							for (const FGuid RemoteControlFieldId : GroupFields)
							{
								AddAction(RemoteControlFieldId);
							}
						}
					}
				}
			}
		}

		return FReply::Handled();
	}

	/** Whether drag and drop is possible from the current exposed property to the Actions table */
	bool OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
	{
		if (DragDropOperation)
		{
			if (DragDropOperation->IsOfType<FExposedEntityDragDrop>())
			{
				if (TSharedPtr<FExposedEntityDragDrop> DragDropOp = StaticCastSharedPtr<FExposedEntityDragDrop>(DragDropOperation))
				{
					// Fetch the Exposed Entity
					for (const FGuid& ExposedEntityId : DragDropOp->GetSelectedIds())
					{
						if (TSharedPtr<SRCActionPanel> ActionPanel = GetActionPanel())
						{
							// Add Action
							if (ActionPanel->CanHaveActionForField(ExposedEntityId))
							{
								return true;
							}
						}
					}
				}
			}
			else if (DragDropOperation->IsOfType<FFieldGroupDragDropOp>())
			{
				if (TSharedPtr<FFieldGroupDragDropOp> DragDropOp = StaticCastSharedPtr<FFieldGroupDragDropOp>(DragDropOperation))
				{
					if (URemoteControlPreset* Preset = GetPreset())
					{
						// Fetch the Group
						const FGuid GroupId = DragDropOp->GetGroupId();
						const FRemoteControlPresetGroup* Group = Preset->Layout.GetGroup(GroupId);

						if (ensure(Group))
						{
							const TArray<FGuid> GroupFields = Group->GetFields();

							// For Groups we accept the drag-drop operation if even just one field in the group can have an action added successfully
							for (const FGuid RemoteControlFieldId : GroupFields)
							{
								if (TSharedPtr<SRCActionPanel> ActionPanel = GetActionPanel())
								{
									if (ActionPanel->CanHaveActionForField(RemoteControlFieldId))
									{
										return true; // we have at least one compatible field
									}
								}
							}
						}
					}
				}
			}
		}

		return false;
	}

	/** OnSelectionChanged delegate for Actions List View */
	void OnSelectionChanged(TSharedPtr<ActionType> InItem, ESelectInfo::Type)
	{
		if (SelectedActionItem)
		{
			SelectedActionItem->OnSelectionExit();
		}

		SelectedActionItem = InItem;
	}

private:

	/** The currently selected Action item */
	TSharedPtr<FRCActionModel> SelectedActionItem;

	/** The parent Action Panel widget */
	TWeakPtr<SRCActionPanel> ActionPanelWeakPtr;

	/** The Behaviour (UI model) associated with us */
	TWeakPtr<FRCBehaviourModel> BehaviourItemWeakPtr;

	/** List of Actions (UI model) active in this widget */
	TArray<TSharedPtr<ActionType>> ActionItems;

	/** List View widget for representing our Actions List */
	TSharedPtr<SListView<TSharedPtr<ActionType>>> ListView;

	/** Panel Style reference. */
	const FRCPanelStyle* RCPanelStyle;
};

#undef LOCTEXT_NAMESPACE

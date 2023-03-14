// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCActionPanel.h"

#include "RemoteControlField.h"
#include "RemoteControlPreset.h"

#include "Action/RCActionContainer.h"
#include "Action/RCFunctionAction.h"
#include "Action/RCPropertyAction.h"

#include "Behaviour/RCBehaviour.h"
#include "Controller/RCController.h"

#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraphPin.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"

#include "SlateOptMacros.h"
#include "SRCActionPanelList.h"
#include "SRCBehaviourDetails.h"
#include "Styling/RemoteControlStyles.h"

#include "UI/Behaviour/RCBehaviourModel.h"
#include "UI/Panels/SRCDockPanel.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRemoteControlPanel.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SRCActionPanel"

TSharedPtr<SBox> SRCActionPanel::NoneSelectedWidget = SNew(SBox)
			.Padding(0.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoneSelected", "Select a behavior to view its actions."))
				.TextStyle(&FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText"))
				.Justification(ETextJustify::Center)
			];

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRCActionPanel::Construct(const FArguments& InArgs, const TSharedRef<SRemoteControlPanel>& InPanel)
{
	SRCLogicPanelBase::Construct(SRCLogicPanelBase::FArguments(), InPanel);
	
	RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.MinorPanel");

	WrappedBoxWidget = SNew(SBox);
	UpdateWrappedWidget();
	
	ChildSlot
		.Padding(RCPanelStyle->PanelPadding)
		[
			WrappedBoxWidget.ToSharedRef()
		];

	// Register delegates
	InPanel->OnBehaviourSelectionChanged.AddSP(this, &SRCActionPanel::OnBehaviourSelectionChanged);

	if (URemoteControlPreset* Preset = GetPreset())
	{
		Preset->Layout.OnFieldAdded().AddSP(this, &SRCActionPanel::OnRemoteControlFieldAdded);
		Preset->Layout.OnFieldDeleted().AddSP(this, &SRCActionPanel::OnRemoteControlFieldDeleted);
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRCActionPanel::Shutdown()
{
	NoneSelectedWidget.Reset();
}

void SRCActionPanel::OnBehaviourSelectionChanged(TSharedPtr<FRCBehaviourModel> InBehaviourItem)
{
	SelectedBehaviourItemWeakPtr = InBehaviourItem;
	UpdateWrappedWidget(InBehaviourItem);
}

void SRCActionPanel::UpdateWrappedWidget(TSharedPtr<FRCBehaviourModel> InBehaviourItem)
{
	if (InBehaviourItem.IsValid())
	{
		ActionPanelList = InBehaviourItem->GetActionsListWidget(SharedThis(this));

		// Action Dock Panel
		TSharedPtr<SRCMinorPanel> ActionDockPanel = SNew(SRCMinorPanel)
			.HeaderLabel(LOCTEXT("ActionsLabel", "Actions"))
			.EnableFooter(true)
			[
				ActionPanelList.ToSharedRef()
			];

		// Add New Action Button
		bAddActionMenuNeedsRefresh = true;

		const TSharedRef<SWidget> AddNewActionButton = SNew(SComboButton)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Add Action")))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
			.ForegroundColor(FSlateColor::UseForeground())
			.CollapseMenuOnParentFocus(true)
			.HasDownArrow(false)
			.ContentPadding(FMargin(4.f, 2.f))
			.ButtonContent()
			[
				SNew(SBox)
				.WidthOverride(RCPanelStyle->IconSize.X)
				.HeightOverride(RCPanelStyle->IconSize.Y)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
				]
			]
			.OnGetMenuContent(this, &SRCActionPanel::GetActionMenuContentWidget);
		
		// Add All Button
		TSharedRef<SWidget> AddAllActionsButton = SNew(SButton)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Add All Actions")))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ForegroundColor(FSlateColor::UseForeground())
			.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
			.ToolTipText(LOCTEXT("AddAllToolTip", "Adds all the available actions."))
			.OnClicked(this, &SRCActionPanel::OnAddAllFields)
			.Visibility(this, &SRCActionPanel::HandleAddAllButtonVisibility)
			[
				SNew(SBox)
				.WidthOverride(RCPanelStyle->IconSize.X)
				.HeightOverride(RCPanelStyle->IconSize.Y)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::GetBrush("Icons.Duplicate"))
				]
			];
		
	// Delete Selected Action Button
		TSharedRef<SWidget> DeleteSelectedActionButton = SNew(SButton)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Delete Selected Action")))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ForegroundColor(FSlateColor::UseForeground())
			.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
			.ToolTipText(LOCTEXT("DeleteSelectedActionToolTip", "Deletes all selected Action."))
			.OnClicked(this, &SRCActionPanel::RequestDeleteSelectedItem)
			.IsEnabled_Lambda([this]() { return ActionPanelList.IsValid() && ActionPanelList->NumSelectedLogicItems() > 0; })
			[
				SNew(SBox)
				.WidthOverride(RCPanelStyle->IconSize.X)
				.HeightOverride(RCPanelStyle->IconSize.Y)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::GetBrush("Icons.Delete"))
				]
			];

		ActionDockPanel->AddHeaderToolbarItem(EToolbar::Left, AddNewActionButton);
		ActionDockPanel->AddHeaderToolbarItem(EToolbar::Right, AddAllActionsButton);
		ActionDockPanel->AddFooterToolbarItem(EToolbar::Right, DeleteSelectedActionButton);

		// Header Dock Panel
		TSharedPtr<SRCMinorPanel> BehaviourDetailsPanel = SNew(SRCMinorPanel)
			.EnableHeader(false)
			[
				SAssignNew(BehaviourDetailsWidget, SRCBehaviourDetails, SharedThis(this), InBehaviourItem.ToSharedRef())
			];

		TSharedRef<SRCMajorPanel> ActionsPanel = SNew(SRCMajorPanel)
			.EnableFooter(false)
			.EnableHeader(false)
			.ChildOrientation(Orient_Vertical);

		// Panel size of zero forces use of "SizeToContent" ensuring that each Behaviour only takes up as much space as necessary
		ActionsPanel->AddPanel(BehaviourDetailsPanel.ToSharedRef(), 0.f);

		ActionsPanel->AddPanel(ActionDockPanel.ToSharedRef(), 0.5f);

		WrappedBoxWidget->SetContent(ActionsPanel);
	}
	else
	{
		WrappedBoxWidget->SetContent(NoneSelectedWidget.ToSharedRef());
	}
}

FReply SRCActionPanel::OnClickOverrideBlueprintButton()
{
	if (!SelectedBehaviourItemWeakPtr.IsValid())
	{
		return FReply::Unhandled();
	}

	if (TSharedPtr<FRCBehaviourModel> Behaviour = SelectedBehaviourItemWeakPtr.Pin())
	{
		Behaviour->OnOverrideBlueprint();
	}

	return FReply::Handled();
}

void SRCActionPanel::SetIsBehaviourEnabled(const bool bIsEnabled)
{
	if (BehaviourDetailsWidget)
	{
		BehaviourDetailsWidget->SetIsBehaviourEnabled(bIsEnabled);
	}
}

void SRCActionPanel::RefreshIsBehaviourEnabled(const bool bIsEnabled)
{
	if (ActionPanelList)
	{
		ActionPanelList->SetEnabled(bIsEnabled);
	}
}

TSharedRef<SWidget> SRCActionPanel::GetActionMenuContentWidget()
{
	if (AddNewActionMenuWidget && !bAddActionMenuNeedsRefresh)
	{
		return AddNewActionMenuWidget.ToSharedRef();
	}

	// Either we are creating the menu the first time, or the list needs to be refreshed
	// The latter can occur either when a remote control property is added or removed, or when a behaiour's CanHaveActionForField logic has changed
	// For example, if the user sets the "Allow numeric input as Strings" flag for Bind Behaviour then we need to recalculate the list of eligible actions.

	constexpr bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	if (!SelectedBehaviourItemWeakPtr.IsValid())
	{
		return MenuBuilder.MakeWidget();
	}

	// List of exposed entities
	if (URemoteControlPreset* Preset = GetPreset())
	{
		const TArray<TWeakPtr<FRemoteControlField>>& RemoteControlFields = Preset->GetExposedEntities<FRemoteControlField>();
		for (const TWeakPtr<FRemoteControlField>& RemoteControlFieldWeakPtr : RemoteControlFields)
		{
			if (const TSharedPtr<FRemoteControlField> RemoteControlField = RemoteControlFieldWeakPtr.Pin())
			{
				if (const URCBehaviour* Behaviour = SelectedBehaviourItemWeakPtr.Pin()->GetBehaviour())
				{
					// Skip if we already have an Action created for this exposed entity
					if(!Behaviour->CanHaveActionForField(RemoteControlField))
					{
						continue;
					}

					// Create menu entry
					FUIAction Action(FExecuteAction::CreateSP(this, &SRCActionPanel::OnAddActionClicked, RemoteControlField));
					MenuBuilder.AddMenuEntry(
						FText::Format(LOCTEXT("AddAction", "{0}"), FText::FromName(RemoteControlField->GetLabel())),
						FText::Format(LOCTEXT("AddActionTooltip", "Add {0}"), FText::FromName(RemoteControlField->GetLabel())),
						FSlateIcon(),
						MoveTemp(Action));
				}
			}
		}
	}

	bAddActionMenuNeedsRefresh = false; // reset

	AddNewActionMenuWidget = MenuBuilder.MakeWidget();

	return AddNewActionMenuWidget.ToSharedRef();
}

URCAction* SRCActionPanel::AddAction(const TSharedRef<const FRemoteControlField> InRemoteControlField)
{
	if (const TSharedPtr<FRCBehaviourModel> BehaviourItem = SelectedBehaviourItemWeakPtr.Pin())
	{
		if (const URCBehaviour* Behaviour = BehaviourItem->GetBehaviour())
		{
			Behaviour->ActionContainer->Modify();

			URCAction* NewAction = BehaviourItem->AddAction(InRemoteControlField);

			AddNewActionToList(NewAction);

			// Broadcast new Action to other panels
			if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
			{
				RemoteControlPanel->OnActionAdded.Broadcast(NewAction);
			}

			return NewAction;
		}
	}

	return nullptr;
}

bool SRCActionPanel::CanHaveActionForField(const FGuid& InRemoteControlFieldId)
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		if (TSharedPtr<FRemoteControlField> RemoteControlField = Preset->GetExposedEntity<FRemoteControlField>(InRemoteControlFieldId).Pin())
		{
			if (const TSharedPtr<FRCBehaviourModel> BehaviourItem = SelectedBehaviourItemWeakPtr.Pin())
			{
				if (const URCBehaviour* Behaviour = BehaviourItem->GetBehaviour())
				{
					return Behaviour->CanHaveActionForField(RemoteControlField);
				}
			}
		}
	}

	return false;
}

void SRCActionPanel::OnAddActionClicked(TSharedPtr<FRemoteControlField> InRemoteControlField)
{
	if (!SelectedBehaviourItemWeakPtr.IsValid() || !InRemoteControlField)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("AddActionTransaction", "Add Action"));

	AddAction(InRemoteControlField.ToSharedRef());
}

FReply SRCActionPanel::OnClickEmptyButton()
{
	if (const TSharedPtr<FRCBehaviourModel> BehaviourItem = SelectedBehaviourItemWeakPtr.Pin())
	{
		if (const URCBehaviour* Behaviour = BehaviourItem->GetBehaviour())
		{
			FScopedTransaction Transaction(LOCTEXT("EmptyActionsTransaction", "Empty Actions"));
			Behaviour->ActionContainer->Modify();

			Behaviour->ActionContainer->EmptyActions();
		}
	}

	if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		RemoteControlPanel->OnEmptyActions.Broadcast();
	}

	return FReply::Handled();
}

FReply SRCActionPanel::OnAddAllFields()
{
	if (!SelectedBehaviourItemWeakPtr.IsValid())
	{
		return FReply::Handled();
	}

	if (URemoteControlPreset* Preset = GetPreset())
	{
		if (const TSharedPtr<FRCBehaviourModel> BehaviourItem = SelectedBehaviourItemWeakPtr.Pin())
		{
			const TArray<TWeakPtr<FRemoteControlField>>& RemoteControlFields = Preset->GetExposedEntities<FRemoteControlField>();

			FScopedTransaction Transaction(LOCTEXT("AddAllActionsTransaction", "Add All Actions"));

			// Enumerate the list of Exposed Entities and Functions available in this Preset for our use as Actions
			for (const TWeakPtr<FRemoteControlField>& RemoteControlFieldWeakPtr : RemoteControlFields)
			{
				if (const TSharedPtr<FRemoteControlField> RemoteControlField = RemoteControlFieldWeakPtr.Pin())
				{
					URCBehaviour* Behaviour = BehaviourItem->GetBehaviour();
					
					// Only add the Behaviour if it's listed as addable.
					if (Behaviour && Behaviour->CanHaveActionForField(RemoteControlField))
					{
						AddAction(RemoteControlField.ToSharedRef());
					}
				}
			}
		}
	}

	return FReply::Handled();
}

void SRCActionPanel::OnRemoteControlFieldAdded(const FGuid& GroupId, const FGuid& FieldId, int32 FieldPosition)
{
	bAddActionMenuNeedsRefresh = true;
}

void SRCActionPanel::OnRemoteControlFieldDeleted(const FGuid& GroupId, const FGuid& FieldId, int32 FieldPosition)
{
	bAddActionMenuNeedsRefresh = true;
}

bool SRCActionPanel::IsListFocused() const
{
	return ActionPanelList.IsValid() && ActionPanelList->IsListFocused();
}

void SRCActionPanel::DeleteSelectedPanelItem()
{
	ActionPanelList->DeleteSelectedPanelItem();
}

TSharedPtr<FRCLogicModeBase> SRCActionPanel::GetSelectedLogicItem()
{
	if(ActionPanelList)
	{
		return ActionPanelList->GetSelectedLogicItem();
	}

	return nullptr;
}

void SRCActionPanel::DuplicateAction(URCAction* InAction)
{
	URCBehaviour* BehaviourTarget = nullptr;
	URCBehaviour* BehaviourSource = nullptr;

	// Behaviour Target - The Behaviour into which the input Action is to copied
	if (const TSharedPtr<FRCBehaviourModel> BehaviourItem = SelectedBehaviourItemWeakPtr.Pin())
	{
		BehaviourTarget = BehaviourItem->GetBehaviour();
		if (!ensure(BehaviourTarget))
		{
			return;
		}
	}
	
	// Behaviour Source - The Behaviour holding the input Action
	if (InAction)
	{
		BehaviourSource = InAction->GetParentBehaviour();
		if (!ensure(BehaviourSource))
		{
			return;
		}
	}

	URCAction* NewAction = BehaviourSource->DuplicateAction(InAction, BehaviourTarget);

	AddNewActionToList(NewAction);

	// Broadcast new Action to other panels
	if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		RemoteControlPanel->OnActionAdded.Broadcast(NewAction);
	}
}

void SRCActionPanel::AddNewActionToList(URCAction* NewAction)
{
	if(ActionPanelList.IsValid())
	{
		ActionPanelList->AddNewLogicItem(NewAction);
	}
}

void SRCActionPanel::DuplicateSelectedPanelItem()
{
	if (!ensure(ActionPanelList.IsValid()))
	{
		return;
	}

	if (const TSharedPtr<FRCActionModel> ActionItem = StaticCastSharedPtr<FRCActionModel>(ActionPanelList->GetSelectedLogicItem()))
	{
		DuplicateAction(ActionItem->GetAction());
	}
}

void SRCActionPanel::CopySelectedPanelItem()
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		if (TSharedPtr<FRCActionModel> ActionItem = StaticCastSharedPtr<FRCActionModel>(ActionPanelList->GetSelectedLogicItem()))
		{
			RemoteControlPanel->SetLogicClipboardItem(ActionItem->GetAction(), SharedThis(this));
		}
	}
}

void SRCActionPanel::PasteItemFromClipboard()
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		if (RemoteControlPanel->LogicClipboardItemSource == SharedThis(this))
		{
			if(URCAction* Action = Cast<URCAction>(RemoteControlPanel->GetLogicClipboardItem()))
			{
				DuplicateAction(Action);
			}
		}
	}
}

bool SRCActionPanel::CanPasteClipboardItem(UObject* InLogicClipboardItem)
{
	URCAction* LogicClipboardAction = Cast<URCAction>(InLogicClipboardItem);
	if (!LogicClipboardAction)
	{
		return false;
	}

	if (URCBehaviour* BehaviourSource = LogicClipboardAction->GetParentBehaviour())
	{
		if (TSharedPtr<FRCBehaviourModel> BehaviourItemTarget = SelectedBehaviourItemWeakPtr.Pin())
		{
			if (URCBehaviour* BehaviourTarget = BehaviourItemTarget->GetBehaviour())
			{
				// Copy-paste is allowed between compatible Behaviour types only
				//
				return BehaviourSource->GetClass() == BehaviourTarget->GetClass();
			}
		}
	}

	return false;
}

FText SRCActionPanel::GetPasteItemMenuEntrySuffix()
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		// This function should only have been called if we were the source of the item copied.
		if (ensure(RemoteControlPanel->LogicClipboardItemSource == SharedThis(this)))
		{
			if (URCAction* Action = Cast<URCAction>(RemoteControlPanel->GetLogicClipboardItem()))
			{
				if (URCBehaviour* Behaviour = Action->GetParentBehaviour())
				{
					return FText::Format(FText::FromString("Action {0}"), Behaviour->GetDisplayName());
				}
			}
		}
	}

	return FText::GetEmpty();
}

FReply SRCActionPanel::RequestDeleteSelectedItem()
{
	if (!ActionPanelList.IsValid())
	{
		return FReply::Unhandled();
	}

	const FText WarningMessage = LOCTEXT("DeleteActionWarning", "Delete the selected Actions?");

	EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage);

	if (UserResponse == EAppReturnType::Yes)
	{
		DeleteSelectedPanelItem();
	}

	return FReply::Handled();
}

FReply SRCActionPanel::RequestDeleteAllItems()
{
	if (!ActionPanelList.IsValid())
	{
		return FReply::Unhandled();
	}

	const FText WarningMessage = FText::Format(LOCTEXT("DeleteAllWarning", "You are about to delete {0} actions. Are you sure you want to proceed?"), ActionPanelList->Num());

	EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage);

	if (UserResponse == EAppReturnType::Yes)
	{
		return OnClickEmptyButton();
	}

	return FReply::Handled();
}

EVisibility SRCActionPanel::HandleAddAllButtonVisibility() const
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		return Preset->HasEntities() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
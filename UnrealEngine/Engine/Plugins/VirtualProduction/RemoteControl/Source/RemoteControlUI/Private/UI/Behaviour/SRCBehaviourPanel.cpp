// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCBehaviourPanel.h"
#include "SRCBehaviourPanelList.h"

#include "Behaviour/RCBehaviourBlueprintNode.h"
#include "Behaviour/RCBehaviour.h"
#include "Behaviour/RCBehaviourNode.h"
#include "Controller/RCController.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/MessageDialog.h"
#include "RemoteControlPreset.h"

#include "SlateOptMacros.h"
#include "Styling/RemoteControlStyles.h"

#include "UI/Panels/SRCDockPanel.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRemoteControlPanel.h"
#include "UI/Controller/RCControllerModel.h"
#include "Widgets/Input/SComboButton.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "SRCBehaviourPanel"

TSharedPtr<SBox> SRCBehaviourPanel::NoneSelectedWidget = SNew(SBox)
			.Padding(10.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoneSelected", "Select a controller\nto view its behaviors."))
				.TextStyle(&FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText"))
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			];

void SRCBehaviourPanel::Construct(const FArguments& InArgs, const TSharedRef<SRemoteControlPanel>& InPanel)
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
	InPanel->OnControllerSelectionChanged.AddSP(this, &SRCBehaviourPanel::OnControllerSelectionChanged);
}

void SRCBehaviourPanel::Shutdown()
{
	NoneSelectedWidget.Reset();
}

void SRCBehaviourPanel::OnControllerSelectionChanged(TSharedPtr<FRCControllerModel> InControllerItem)
{
	SelectedControllerItemWeakPtr = InControllerItem;
	UpdateWrappedWidget(InControllerItem);
}

void SRCBehaviourPanel::UpdateWrappedWidget(TSharedPtr<FRCControllerModel> InControllerItem)
{
	TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel();

	if (InControllerItem.IsValid() && RemoteControlPanel.IsValid())
	{
		// Behaviour Dock Panel
		TSharedPtr<SRCMinorPanel> BehaviourDockPanel = SNew(SRCMinorPanel)
			.HeaderLabel(LOCTEXT("BehavioursLabel", "Behavior"))
			.EnableFooter(true)
			[
				SAssignNew(BehaviourPanelList, SRCBehaviourPanelList, SharedThis(this), InControllerItem, RemoteControlPanel.ToSharedRef())
			];

		// Add New Behaviour Button
		const TSharedRef<SWidget> AddNewBehaviourButton = SNew(SComboButton)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Add Behavior")))
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
			.MenuContent()
			[
				GetBehaviourMenuContentWidget()
			];

		// Delete Selected Behaviour Button
		TSharedRef<SWidget> DeleteSelectedBehaviourButton = SNew(SButton)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Delete Selected Behaviour")))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ForegroundColor(FSlateColor::UseForeground())
			.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
			.ToolTipText(LOCTEXT("DeleteSelectedBehaviourToolTip", "Deletes the selected behaviour."))
			.OnClicked(this, &SRCBehaviourPanel::RequestDeleteSelectedItem)
			.IsEnabled_Lambda([this]() { return BehaviourPanelList.IsValid() && BehaviourPanelList->NumSelectedLogicItems(); })
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

		BehaviourDockPanel->AddHeaderToolbarItem(EToolbar::Left, AddNewBehaviourButton);
		BehaviourDockPanel->AddFooterToolbarItem(EToolbar::Right, DeleteSelectedBehaviourButton);

		WrappedBoxWidget->SetContent(BehaviourDockPanel.ToSharedRef());
	}
	else
	{
		WrappedBoxWidget->SetContent(NoneSelectedWidget.ToSharedRef());
	}
}

TSharedRef<SWidget> SRCBehaviourPanel::GetBehaviourMenuContentWidget()
{
	constexpr bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	if (const TSharedPtr<FRCControllerModel> ControllerItem = SelectedControllerItemWeakPtr.Pin())
	{
		if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
		{
			for (TObjectIterator<UClass> It; It; ++It)
			{
				UClass* Class = *It;

				if (Class->IsChildOf(URCBehaviourNode::StaticClass()))
				{
					const bool bIsClassInstantiatable = Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Abstract) || FKismetEditorUtilities::IsClassABlueprintSkeleton(Class);
					if (bIsClassInstantiatable)
					{
						continue;
					}

					if (Class->IsInBlueprint())
					{
						if (Class->GetSuperClass() != URCBehaviourBlueprintNode::StaticClass())
						{
							continue;
						}
					}

					URCBehaviour* Behaviour = Controller->CreateBehaviour(Class);
					if (!Behaviour)
					{
						continue;
					}

					FUIAction Action(FExecuteAction::CreateSP(this, &SRCBehaviourPanel::OnAddBehaviourClicked, Class));
					MenuBuilder.AddMenuEntry(
						FText::Format(LOCTEXT("AddBehaviourNode", "{0}"), Behaviour->GetDisplayName()),
						FText::Format(LOCTEXT("AddBehaviourNodeTooltip", "{0}"), Behaviour->GetBehaviorDescription()),
						FSlateIcon(),
						Action);
				}
			}
		}
	}

	return MenuBuilder.MakeWidget();
}

void SRCBehaviourPanel::OnAddBehaviourClicked(UClass* InClass)
{
	if (const TSharedPtr<FRCControllerModel> ControllerItem = SelectedControllerItemWeakPtr.Pin())
	{
		if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
		{
			FScopedTransaction Transaction(LOCTEXT("AddBehaviour", "Add Behaviour"));
			Controller->Modify();

			URCBehaviour* NewBehaviour = Controller->AddBehaviour(InClass);

			if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
			{
				RemoteControlPanel->OnBehaviourAdded.Broadcast(NewBehaviour);
			}
		}
	}
}

FReply SRCBehaviourPanel::OnClickEmptyButton()
{
	if (const TSharedPtr<FRCControllerModel> ControllerItem = SelectedControllerItemWeakPtr.Pin())
	{
		if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
		{
			FScopedTransaction Transaction(LOCTEXT("EmptyBehaviours", "Empty Behaviours"));
			Controller->Modify();

			Controller->EmptyBehaviours();
		}
	}

	if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		RemoteControlPanel->OnEmptyBehaviours.Broadcast();
	}

	return FReply::Handled();
}

bool SRCBehaviourPanel::IsListFocused() const
{
	return BehaviourPanelList.IsValid() && BehaviourPanelList->IsListFocused();
}

void SRCBehaviourPanel::DeleteSelectedPanelItem()
{
	BehaviourPanelList->DeleteSelectedPanelItem();
}

void SRCBehaviourPanel::DuplicateBehaviour(URCBehaviour* InBehaviour)
{
	if (URCController* Controller = GetParentController())
	{
		URCBehaviour* NewBehaviour = URCController::DuplicateBehaviour(Controller, InBehaviour);

		BehaviourPanelList->AddNewLogicItem(NewBehaviour);
	}
}

void SRCBehaviourPanel::DuplicateSelectedPanelItem()
{
	FScopedTransaction Transaction(LOCTEXT("DuplicateBehaviour", "Duplicate Behaviour"));

	if (TSharedPtr<FRCBehaviourModel> SelectedBehaviourItem = BehaviourPanelList->GetSelectedBehaviourItem())
	{
		DuplicateBehaviour(SelectedBehaviourItem->GetBehaviour());
	}
}

void SRCBehaviourPanel::CopySelectedPanelItem()
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		if (TSharedPtr<FRCBehaviourModel> SelectedBehaviourItem = BehaviourPanelList->GetSelectedBehaviourItem())
		{
			RemoteControlPanel->SetLogicClipboardItem(SelectedBehaviourItem->GetBehaviour(), SharedThis(this));
		}
	}
}

void SRCBehaviourPanel::PasteItemFromClipboard()
{
	FScopedTransaction Transaction(LOCTEXT("PasteBehaviour", "Paste Behaviour"));

	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		if (RemoteControlPanel->LogicClipboardItemSource == SharedThis(this))
		{
			if(URCBehaviour* Behaviour = Cast<URCBehaviour>(RemoteControlPanel->GetLogicClipboardItem()))
			{
				DuplicateBehaviour(Behaviour);
			}
		}
	}
}

bool SRCBehaviourPanel::CanPasteClipboardItem(UObject* InLogicClipboardItem)
{
	URCBehaviour* LogicClipboardBehaviour = Cast<URCBehaviour>(InLogicClipboardItem);

	URCController* ControllerSource = LogicClipboardBehaviour->ControllerWeakPtr.Get();
	if (!ControllerSource)
	{
		return false;
	}

	URCController* ControllerTarget = GetParentController();
	if (!ControllerTarget)
	{
		return false;
	}

	// Copy-Paste is only permitted between Controllers of the same type
	//
	return ControllerSource->GetValueType() == ControllerTarget->GetValueType();
}

FText SRCBehaviourPanel::GetPasteItemMenuEntrySuffix()
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		// This function should only have been called if we were the source of the item copied.
		if (ensure(RemoteControlPanel->LogicClipboardItemSource == SharedThis(this)))
		{
			if (URCBehaviour* Behaviour = Cast<URCBehaviour>(RemoteControlPanel->GetLogicClipboardItem()))
			{
				return FText::Format(FText::FromString("Behaviour {0}"), Behaviour->GetDisplayName());
			}
		}
	}

	return FText::GetEmpty();
}

TSharedPtr<FRCLogicModeBase> SRCBehaviourPanel::GetSelectedLogicItem()
{
	if (BehaviourPanelList)
	{
		return BehaviourPanelList->GetSelectedBehaviourItem();
	}

	return nullptr;
}

URCController* SRCBehaviourPanel::GetParentController()
{
	if (TSharedPtr<FRCControllerModel> ControllerItem = SelectedControllerItemWeakPtr.Pin())
	{
		return Cast<URCController>(ControllerItem->GetVirtualProperty());
	}

	return nullptr;
}

FReply SRCBehaviourPanel::RequestDeleteSelectedItem()
{
	if (!BehaviourPanelList.IsValid())
	{
		return FReply::Unhandled();
	}

	const FText WarningMessage = LOCTEXT("DeleteBehaviourlWarning", "Delete the selected Behaviours?");

	EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage);

	if (UserResponse == EAppReturnType::Yes)
	{
		DeleteSelectedPanelItem();
	}

	return FReply::Handled();
}

FReply SRCBehaviourPanel::RequestDeleteAllItems()
{
	if (!BehaviourPanelList.IsValid())
	{
		return FReply::Unhandled();
	}

	const FText WarningMessage = FText::Format(LOCTEXT("DeleteAllWarning", "You are about to delete {0} behaviors. Are you sure you want to proceed?"), BehaviourPanelList->Num());

	EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage);

	if (UserResponse == EAppReturnType::Yes)
	{
		return OnClickEmptyButton();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
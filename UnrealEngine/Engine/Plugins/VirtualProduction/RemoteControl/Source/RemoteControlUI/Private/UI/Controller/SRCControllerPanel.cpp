// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCControllerPanel.h"

#include "Controller/RCController.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "RCControllerModel.h"
#include "RCVirtualPropertyContainer.h"
#include "RemoteControlLogicConfig.h"
#include "RemoteControlPreset.h"
#include "SlateOptMacros.h"
#include "SRCControllerPanelList.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/BaseLogicUI/RCLogicModeBase.h"
#include "UI/Panels/SRCDockPanel.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/RCUIHelpers.h"
#include "UI/SRemoteControlPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "SRCControllerPanel"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRCControllerPanel::Construct(const FArguments& InArgs, const TSharedRef<SRemoteControlPanel>& InPanel)
{
	SRCLogicPanelBase::Construct(SRCLogicPanelBase::FArguments(), InPanel);
	
	bIsInLiveMode = InArgs._LiveMode;

	RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.MinorPanel");

	// Controller Dock Panel
	TSharedPtr<SRCMinorPanel> ControllerDockPanel = SNew(SRCMinorPanel)
		.HeaderLabel(LOCTEXT("ControllersLabel", "Controller"))
		.EnableFooter(true)
		[
			SAssignNew(ControllerPanelList, SRCControllerPanelList, SharedThis(this), InPanel)
		];

	// Add New Controller Button
	const TSharedRef<SWidget> AddNewControllerButton = SNew(SComboButton)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Add Controller")))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.IsEnabled_Lambda([this]() { return !bIsInLiveMode.Get(); })
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
			GetControllerMenuContentWidget()
		];

	// Delete Selected Controller Button
	TSharedRef<SWidget> DeleteSelectedControllerButton = SNew(SButton)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Delete Selected Controller")))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ForegroundColor(FSlateColor::UseForeground())
		.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
		.ToolTipText(LOCTEXT("DeleteSelectedControllerToolTip", "Deletes the selected controller."))
		.OnClicked(this, &SRCControllerPanel::RequestDeleteSelectedItem)
		.IsEnabled_Lambda([this]() { return ControllerPanelList.IsValid() && !bIsInLiveMode.Get() && ControllerPanelList->NumSelectedLogicItems() > 0; })
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

	ControllerDockPanel->AddHeaderToolbarItem(EToolbar::Left, AddNewControllerButton);
	ControllerDockPanel->AddFooterToolbarItem(EToolbar::Right, DeleteSelectedControllerButton);

	ChildSlot
		.Padding(RCPanelStyle->PanelPadding)
		[
			ControllerDockPanel.ToSharedRef()
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

static UObject* GetBaseStructForType(FName StructType)
{
	if (StructType == NAME_Vector)
	{
		return TBaseStructure<FVector>::Get();
	}
	else if (StructType == NAME_Color)
	{
		return TBaseStructure<FColor>::Get();
	}
	else if (StructType == NAME_Rotator)
	{
		return TBaseStructure<FRotator>::Get();
	}

	ensureMsgf(false, TEXT("Found unsupported struct type %s in config."), *StructType.ToString());

	return nullptr;
}

bool SRCControllerPanel::IsListFocused() const
{
	return ControllerPanelList->IsListFocused();
}

void SRCControllerPanel::DeleteSelectedPanelItem()
{
	if (ControllerPanelList)
	{
		ControllerPanelList->DeleteSelectedPanelItem();
	}
}

void SRCControllerPanel::DuplicateSelectedPanelItem()
{
	if (ControllerPanelList)
	{
		if(TSharedPtr<FRCControllerModel> ControllerItem = ControllerPanelList->GetSelectedControllerItem())
		{
			if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
			{
				DuplicateController(Controller);
			}
		}
	}
}

void SRCControllerPanel::CopySelectedPanelItem()
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		if (TSharedPtr<FRCControllerModel> ControllerItem = ControllerPanelList->GetSelectedControllerItem())
		{
			if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
			{
				RemoteControlPanel->SetLogicClipboardItem(Controller, SharedThis(this));
			}
		}
	}
}

void SRCControllerPanel::PasteItemFromClipboard()
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		if(RemoteControlPanel->LogicClipboardItemSource == SharedThis(this))
		{
			if (URCController* Controller = Cast<URCController>(RemoteControlPanel->GetLogicClipboardItem()))
			{
				DuplicateController(Controller);
			}
		}
	}
}

FText SRCControllerPanel::GetPasteItemMenuEntrySuffix()
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		// This function should only have been called if we were the source of the item copied.
		if (ensure(RemoteControlPanel->LogicClipboardItemSource == SharedThis(this)))
		{
			if (URCController* Controller = Cast<URCController>(RemoteControlPanel->GetLogicClipboardItem()))
			{
				return FText::Format(FText::FromString("Controller {0}"), FText::FromName(Controller->DisplayName));
			}
		}
	}

	return FText::GetEmpty();
}

TSharedPtr<FRCLogicModeBase> SRCControllerPanel::GetSelectedLogicItem()
{
	if (ControllerPanelList)
	{
		return ControllerPanelList->GetSelectedControllerItem();
	}

	return nullptr;
}

void SRCControllerPanel::DuplicateController(URCController* InController)
{
	if (!ensure(InController))
	{
		return;
	}

	if (URemoteControlPreset* Preset = GetPreset())
	{
		if (URCController* NewController = Cast<URCController>(Preset->DuplicateController(InController)))
		{
			NewController->SetDisplayIndex(ControllerPanelList->NumControllerItems());

			ControllerPanelList->RequestRefresh();
		}
	}
}

FReply SRCControllerPanel::RequestDeleteSelectedItem()
{
	if (!ControllerPanelList.IsValid())
	{
		return FReply::Unhandled();
	}

	const FText WarningMessage = LOCTEXT("DeleteSelectedItemlWarning", "Delete the selected Controller?");

	EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage);

	if (UserResponse == EAppReturnType::Yes)
	{
		DeleteSelectedPanelItem();
	}

	return FReply::Handled();
}

FReply SRCControllerPanel::RequestDeleteAllItems()
{
	if (!ControllerPanelList.IsValid())
	{
		return FReply::Unhandled();
	}

	const FText WarningMessage = FText::Format(LOCTEXT("DeleteAllWarning", "You are about to delete {0} controllers. Are you sure you want to proceed?"), ControllerPanelList->Num());

	EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage);

	if (UserResponse == EAppReturnType::Yes)
	{
		return OnClickEmptyButton();
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SRCControllerPanel::GetControllerMenuContentWidget() const
{
	constexpr  bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	TArray<TPair<EPropertyBagPropertyType, UObject*>> VirtualPropertyFieldClassNames;

	// See Config file: BaseRemoteControl.ini
	const URemoteControlLogicConfig* RCLogicConfig = GetDefault<URemoteControlLogicConfig>();
	for (const EPropertyBagPropertyType ControllerType : RCLogicConfig->SupportedControllerTypes)
	{
		if (ControllerType == EPropertyBagPropertyType::Struct)
		{
			for (const FName StructType : RCLogicConfig->SupportedControllerStructTypes)
			{
				UObject* ValueTypeObject = GetBaseStructForType(StructType);
				if (ensure(ValueTypeObject))
				{
					VirtualPropertyFieldClassNames.Add({ ControllerType, ValueTypeObject });
				}
			}
		}
		else
		{
			VirtualPropertyFieldClassNames.Add({ ControllerType, nullptr });
		}
	}

	// Generate a menu from the list of supported Controllers
	for (const TPair<EPropertyBagPropertyType, UObject*>& Pair : VirtualPropertyFieldClassNames)
	{
		// Display Name
		const FName DefaultName = URCVirtualPropertyBase::GetVirtualPropertyTypeDisplayName(Pair.Key, Pair.Value);

		// Type Color
		FLinearColor TypeColor = FColor::White;

		// Generate a transient virtual property for deducing type color:
		FInstancedPropertyBag Bag;
		Bag.AddProperty(DefaultName, Pair.Key, Pair.Value);
		const FPropertyBagPropertyDesc* BagPropertyDesc = Bag.FindPropertyDescByName(DefaultName);
		if (!ensure(BagPropertyDesc))
		{
			continue;
		}

		// Variable Color Bar
		TSharedRef<SWidget> MenuItemWidget = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Fill)
			[
				UE::RCUIHelpers::GetTypeColorWidget(BagPropertyDesc->CachedProperty)
			]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(6.f, 0.f))
			[
				SNew(STextBlock)
				.Text(FText::FromName(DefaultName))
			];

		// Menu Item
		FUIAction Action(FExecuteAction::CreateSP(this, &SRCControllerPanel::OnAddControllerClicked, Pair.Key, Pair.Value));

		MenuBuilder.AddMenuEntry(Action, MenuItemWidget);
	}

	return MenuBuilder.MakeWidget();
}

void SRCControllerPanel::OnAddControllerClicked(const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject) const
{
	// Add to the asset
	if (URemoteControlPreset* Preset = GetPreset())
	{
		FScopedTransaction Transaction(LOCTEXT("AddController", "Add Controller"));
		Preset->Modify();

		URCVirtualPropertyInContainer* NewVirtualProperty = Preset->AddController(URCController::StaticClass(), InValueType, InValueTypeObject);

		if (ControllerPanelList.IsValid())
		{
			NewVirtualProperty->DisplayIndex = ControllerPanelList->NumControllerItems();
		}

		// Refresh list
		const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel();
		check(RemoteControlPanel);
		RemoteControlPanel->OnControllerAdded.Broadcast(NewVirtualProperty->PropertyName);
	}
}

void SRCControllerPanel::EnterRenameMode()
{
	if (ControllerPanelList)
	{
		ControllerPanelList->EnterRenameMode();
	}
}

FReply SRCControllerPanel::OnClickEmptyButton()
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		FScopedTransaction Transaction(LOCTEXT("EmptyControllers", "Empty Controllers"));
		Preset->Modify();

		Preset->ResetControllers();
	}

	if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = GetRemoteControlPanel())
	{
		RemoteControlPanel->OnEmptyControllers.Broadcast();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCBehaviourDetails.h"

#include "Action/RCActionContainer.h"
#include "Action/RCFunctionAction.h"
#include "Action/RCPropertyAction.h"
#include "Behaviour/RCBehaviour.h"
#include "Controller/RCController.h"
#include "RemoteControlPreset.h"
#include "SlateOptMacros.h"
#include "SRCActionPanel.h"
#include "Styling/CoreStyle.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/Behaviour/RCBehaviourModel.h"
#include "UI/BaseLogicUI/RCLogicHelpers.h"
#include "UI/RCUIHelpers.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRemoteControlPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "SRCBehaviourDetails"

void SRCBehaviourDetails::Construct(const FArguments& InArgs, TSharedRef<SRCActionPanel> InActionPanel, TSharedRef<FRCBehaviourModel> InBehaviourItem)
{
	RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.MinorPanel");

	ActionPanelWeakPtr = InActionPanel;
	BehaviourItemWeakPtr = InBehaviourItem;

	URCBehaviour* Behaviour = BehaviourItemWeakPtr.Pin()->GetBehaviour();
	const FText BehaviourDisplayName = Behaviour->GetDisplayName();
	const FText BehaviourDescription = Behaviour->GetBehaviorDescription();

	const FSlateFontInfo& FontBehaviorDesc = FRemoteControlPanelStyle::Get()->GetFontStyle("RemoteControlPanel.Behaviours.BehaviorDescription");

	FLinearColor TypeColor;
	FString TypeDisplayName;
	if (URCController* Controller = Behaviour->ControllerWeakPtr.Get())
	{
		TypeColor = UE::RCUIHelpers::GetFieldClassTypeColor(Behaviour->ControllerWeakPtr->GetProperty());
		TypeDisplayName = FName::NameToDisplayString(UE::RCUIHelpers::GetFieldClassDisplayName(Behaviour->ControllerWeakPtr->GetProperty()).ToString(), false);
	}

	BehaviourDetailsWidget = InBehaviourItem->GetBehaviourDetailsWidget();

	const bool bIsChecked = InBehaviourItem->IsBehaviourEnabled();
	
	ChildSlot
		.Padding(RCPanelStyle->PanelPadding)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.Padding(6.f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// Behaviour Name
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.FillWidth(1.f)
				.Padding(4.f)
				[
					SAssignNew(BehaviourTitleWidget, STextBlock)
					.ColorAndOpacity(FLinearColor::White)
					.Text(BehaviourDisplayName)
					.TextStyle(&RCPanelStyle->SectionHeaderTextStyle)
					.ToolTipText(BehaviourDescription)
				]

				// Toggle Behaviour Button
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(4.f, 0.f)
				[
					SNew(SCheckBox)
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Toggle Behavior")))
					.ToolTipText(LOCTEXT("EditModeTooltip", "Enable/Disable this Behaviour.\nWhen a behaviour is disabled its Actions will not be processed when the Controller value changes"))
					.HAlign(HAlign_Center)
					.Style(&RCPanelStyle->ToggleButtonStyle)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsChecked_Lambda([this]() { return BehaviourItemWeakPtr.IsValid() && BehaviourItemWeakPtr.Pin()->IsBehaviourEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged(this, &SRCBehaviourDetails::OnToggleEnableBehaviour)
				]
			]
			
			// Border (separating Header and Behaviour specific details panel)
			+ SVerticalBox::Slot()
			.Padding(2.f, 4.f)
			.AutoHeight()
			[
				SNew(SSeparator)
				.SeparatorImage(FAppStyle::Get().GetBrush("Separator"))
				.Thickness(2.f)
				.Orientation(EOrientation::Orient_Horizontal)
				.Visibility_Lambda([this]() { return this->BehaviourDetailsWidget.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			// Behaviour Specific Details Panel
			+ SVerticalBox::Slot()
			.Padding(8.f, 4.f)
			.AutoHeight()
			[
				BehaviourDetailsWidget.ToSharedRef()
			]

			// Spacer to fill the gap.
			+ SVerticalBox::Slot()
			.Padding(0)
			.FillHeight(1.f)
			[
				SNew(SSpacer)
			]
		];

	RefreshIsBehaviourEnabled(bIsChecked);
}

void SRCBehaviourDetails::OnToggleEnableBehaviour(ECheckBoxState State)
{
	SetIsBehaviourEnabled(State == ECheckBoxState::Checked);
}

void SRCBehaviourDetails::SetIsBehaviourEnabled(const bool bIsEnabled)
{
	if (TSharedPtr<FRCBehaviourModel> BehaviourItem = BehaviourItemWeakPtr.Pin())
	{
		BehaviourItem->SetIsBehaviourEnabled(bIsEnabled);

		RefreshIsBehaviourEnabled(bIsEnabled);
	}
}

void SRCBehaviourDetails::RefreshIsBehaviourEnabled(const bool bIsEnabled)
{
	if(BehaviourDetailsWidget && BehaviourTitleWidget)
	{
		BehaviourDetailsWidget->SetEnabled(bIsEnabled);
		BehaviourTitleWidget->SetEnabled(bIsEnabled);
	}

	if (TSharedPtr<SRCActionPanel> ActionPanel = ActionPanelWeakPtr.Pin())
	{
		ActionPanel->RefreshIsBehaviourEnabled(bIsEnabled);
	}
}

#undef LOCTEXT_NAMESPACE
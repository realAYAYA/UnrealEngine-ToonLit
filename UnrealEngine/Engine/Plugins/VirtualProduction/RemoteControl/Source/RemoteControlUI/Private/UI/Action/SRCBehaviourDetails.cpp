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

	FLinearColor TypeColor = FLinearColor::White;
	FString TypeDisplayName = (LOCTEXT("NoneDisplayName", "None")).ToString();
	if (URCController* Controller = Behaviour->ControllerWeakPtr.Get())
	{
		if (FProperty* Property = Behaviour->ControllerWeakPtr->GetProperty())
		{
			TypeColor = UE::RCUIHelpers::GetFieldClassTypeColor(Property);
			TypeDisplayName = FName::NameToDisplayString(UE::RCUIHelpers::GetFieldClassDisplayName(Property).ToString(), false);
		}
	}

	BehaviourDetailsWidget = InBehaviourItem->GetBehaviourDetailsWidget();

	const bool bIsChecked = InBehaviourItem->IsBehaviourEnabled();
	
	ChildSlot
		.Padding(RCPanelStyle->PanelPadding)
		[
			SNew(SVerticalBox)

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
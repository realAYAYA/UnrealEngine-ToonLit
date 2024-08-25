// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCBehaviourModel.h"

#include "Behaviour/RCBehaviourNode.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/Action/RCActionModel.h"
#include "UI/Action/SRCActionPanel.h"
#include "UI/Action/SRCActionPanelList.h"
#include "UI/BaseLogicUI/RCLogicHelpers.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRemoteControlPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FRCBehaviourModel"

FRCBehaviourModel::FRCBehaviourModel(URCBehaviour* InBehaviour
	, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel /*= nullptr*/)
	: FRCLogicModeBase(InRemoteControlPanel)
	, BehaviourWeakPtr(InBehaviour)
{
	RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.BehaviourPanel");

	if (BehaviourWeakPtr.IsValid())
	{
		const FText BehaviourDisplayName = BehaviourWeakPtr->GetDisplayName();
		
		SAssignNew(BehaviourTitleText, STextBlock)
			.Text(BehaviourDisplayName)
			.TextStyle(&RCPanelStyle->HeaderTextStyle);

		RefreshIsBehaviourEnabled(BehaviourWeakPtr->bIsEnabled);
	}
}

URCAction* FRCBehaviourModel::AddAction()
{
	URCAction* NewAction = nullptr;
	if (URCBehaviour* Behaviour = BehaviourWeakPtr.Get())
	{
		NewAction = Behaviour->AddAction();
		OnActionAdded(NewAction);
	}
	return NewAction;
}

URCAction* FRCBehaviourModel::AddAction(FName InFieldId)
{
	URCAction* NewAction = nullptr;
	if (URCBehaviour* Behaviour = BehaviourWeakPtr.Get())
	{
		NewAction = Behaviour->AddAction(InFieldId);
		OnActionAdded(NewAction);
	}
	return NewAction;
}

URCAction* FRCBehaviourModel::AddAction(const TSharedRef<const FRemoteControlField> InRemoteControlField)
{
	URCAction* NewAction = nullptr;

	if (URCBehaviour* Behaviour = BehaviourWeakPtr.Get())
	{
		NewAction = Behaviour->AddAction(InRemoteControlField);

		OnActionAdded(NewAction);
	}

	return NewAction;
}

TSharedRef<SWidget> FRCBehaviourModel::GetWidget() const
{
	if (!ensure(BehaviourWeakPtr.IsValid()))
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SHorizontalBox)
		.Clipping(EWidgetClipping::OnDemand)
		// Behaviour name
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(FMargin(8.f))
		[
			BehaviourTitleText.ToSharedRef()
		];
}

bool FRCBehaviourModel::HasBehaviourDetailsWidget()
{
	return false;
}

TSharedRef<SWidget> FRCBehaviourModel::GetBehaviourDetailsWidget()
{
	return SNullWidget::NullWidget;
}

void FRCBehaviourModel::OnOverrideBlueprint() const
{
	if (URCBehaviour* Behaviour = BehaviourWeakPtr.Get())
	{
		UBlueprint* Blueprint = Behaviour->GetBlueprint();
		if (!Blueprint)
		{
			Blueprint = UE::RCLogicHelpers::CreateBlueprintWithDialog(Behaviour->BehaviourNodeClass, Behaviour->GetPackage(), UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
			Behaviour->SetOverrideBehaviourBlueprintClass(Blueprint);
		}

		UE::RCLogicHelpers::OpenBlueprintEditor(Blueprint);
	}
}

bool FRCBehaviourModel::IsBehaviourEnabled() const
{
	if (URCBehaviour* Behaviour = BehaviourWeakPtr.Get())
	{
		return Behaviour->bIsEnabled;
	}

	return false;
}

void FRCBehaviourModel::SetIsBehaviourEnabled(const bool bIsEnabled)
{
	if (URCBehaviour* Behaviour = BehaviourWeakPtr.Get())
	{
		Behaviour->bIsEnabled = bIsEnabled;

		RefreshIsBehaviourEnabled(bIsEnabled);
	}
}

void FRCBehaviourModel::RefreshIsBehaviourEnabled(const bool bIsEnabled)
{
	BehaviourTitleText->SetEnabled(bIsEnabled);
}

TSharedPtr<SRCLogicPanelListBase> FRCBehaviourModel::GetActionsListWidget(TSharedRef<SRCActionPanel> InActionPanel)
{
	// Returns the default Actions List; child classes can override as required

	return SNew(SRCActionPanelList<FRCActionModel>, InActionPanel, SharedThis(this));
}

URCBehaviour* FRCBehaviourModel::GetBehaviour() const
{
	return BehaviourWeakPtr.Get();
}

#undef LOCTEXT_NAMESPACE
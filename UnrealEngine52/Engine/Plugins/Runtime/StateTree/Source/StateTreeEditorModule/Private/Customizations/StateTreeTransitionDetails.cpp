// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTransitionDetails.h"
#include "StateTreeTypes.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "StateTreePropertyHelpers.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

TSharedRef<IPropertyTypeCustomization> FStateTreeTransitionDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeTransitionDetails);
}

void FStateTreeTransitionDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

	TriggerProperty = StructProperty->GetChildHandle(TEXT("Trigger"));
	PriorityProperty = StructProperty->GetChildHandle(TEXT("Priority"));
	EventTagProperty = StructProperty->GetChildHandle(TEXT("EventTag"));
	StateProperty = StructProperty->GetChildHandle(TEXT("State"));
	DelayTransitionProperty = StructProperty->GetChildHandle(TEXT("bDelayTransition"));
	DelayDurationProperty = StructProperty->GetChildHandle(TEXT("DelayDuration"));
	DelayRandomVarianceProperty = StructProperty->GetChildHandle(TEXT("DelayRandomVariance"));
	ConditionsProperty = StructProperty->GetChildHandle(TEXT("Conditions"));

	HeaderRow
		.RowTag(StructProperty->GetProperty()->GetFName())
		.WholeRowContent()
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			// Description
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &FStateTreeTransitionDetails::GetDescription)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				StructPropertyHandle->CreateDefaultPropertyButtonWidgets()
			]
		];
}
 
void FStateTreeTransitionDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	check(TriggerProperty);
	check(EventTagProperty);
	check(DelayTransitionProperty);
	check(DelayDurationProperty);
	check(DelayRandomVarianceProperty);
	check(StateProperty);
	check(ConditionsProperty);

	auto IsTickOrEventTransition = [this]()
	{
		return !EnumHasAnyFlags(GetTrigger(), EStateTreeTransitionTrigger::OnStateCompleted) ? EVisibility::Visible : EVisibility::Collapsed;
	};

	// Trigger
	StructBuilder.AddProperty(TriggerProperty.ToSharedRef());

	// Show event only when the trigger is set to Event. 
	StructBuilder.AddProperty(EventTagProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([this]()
		{
			return (GetTrigger() == EStateTreeTransitionTrigger::OnEvent) ? EVisibility::Visible : EVisibility::Collapsed;
		})));

	// State
	StructBuilder.AddProperty(StateProperty.ToSharedRef());

	// Priority
	StructBuilder.AddProperty(PriorityProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(IsTickOrEventTransition)));

	// Delay
	StructBuilder.AddProperty(DelayTransitionProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(IsTickOrEventTransition)));
	StructBuilder.AddProperty(DelayDurationProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(IsTickOrEventTransition)));
	StructBuilder.AddProperty(DelayRandomVarianceProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(IsTickOrEventTransition)));

	// Show conditions always expanded, with simplified header (remove item count)
	IDetailPropertyRow& ConditionsRow = StructBuilder.AddProperty(ConditionsProperty.ToSharedRef());
	ConditionsRow.ShouldAutoExpand(true);

	constexpr bool bShowChildren = true;
	ConditionsRow.CustomWidget(bShowChildren)
		.RowTag(ConditionsProperty->GetProperty()->GetFName())
		.NameContent()
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 2.0f))
			[
				SNew(STextBlock)
				.Text(ConditionsProperty->GetPropertyDisplayName())
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
		]
		.ValueContent()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(SBox) // Empty, suppress noisy array details.
		];
}

EStateTreeTransitionTrigger FStateTreeTransitionDetails::GetTrigger() const
{
	check(TriggerProperty);
	EStateTreeTransitionTrigger TriggerValue = EStateTreeTransitionTrigger::None;
	if (TriggerProperty.IsValid())
	{
		TriggerProperty->GetValue((uint8&)TriggerValue);
	}
	return TriggerValue;
}

bool FStateTreeTransitionDetails::GetDelayTransition() const
{
	check(DelayTransitionProperty);
	bool bDelayTransition = false;
	if (DelayTransitionProperty.IsValid())
	{
		DelayTransitionProperty->GetValue(bDelayTransition);
	}
	return bDelayTransition;
}

FText FStateTreeTransitionDetails::GetDescription() const
{
	check(StateProperty);
	if (StateProperty->GetNumPerObjectValues() != 1)
	{
		return LOCTEXT("MultipleSelected", "Multiple Selected");
	}

	EStateTreeTransitionTrigger Trigger = GetTrigger();
	FText TriggerText = UEnum::GetDisplayValueAsText(Trigger);

	if (Trigger == EStateTreeTransitionTrigger::OnEvent)
	{
		FGameplayTag EventTag;
		UE::StateTree::PropertyHelpers::GetStructValue<FGameplayTag>(EventTagProperty, EventTag);
		TriggerText = FText::Format(LOCTEXT("TransitionOnEvent", "On Event {0}"), FText::FromName(EventTag.GetTagName()));
	}
	
	FText TargetText;
	TArray<void*> RawData;
	StateProperty->AccessRawData(RawData);
	check(RawData.Num() > 0);
	
	const FStateTreeStateLink* State = static_cast<FStateTreeStateLink*>(RawData[0]);
	if (State != nullptr)
	{
		switch (State->LinkType)
		{
		case EStateTreeTransitionType::None:
			TargetText = LOCTEXT("TransitionNone", "None");
			break;
		case EStateTreeTransitionType::Succeeded:
			TargetText = LOCTEXT("TransitionTreeSucceeded", "Tree Succeeded");
			break;
		case EStateTreeTransitionType::Failed:
			TargetText = LOCTEXT("TransitionTreeFailed", "Tree Failed");
			break;
		case EStateTreeTransitionType::NextState:
			TargetText = LOCTEXT("TransitionNextState", "Next State");
			break;
		case EStateTreeTransitionType::GotoState:
			{
				TargetText = FText::Format(LOCTEXT("TransitionGotoState", "Go to State {0}"), FText::FromName(State->Name));
			}
			break;
		}
	}

	return FText::Format(LOCTEXT("TransitionDesc", "{0} {1}"), TriggerText, TargetText);
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTransitionDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "StateTreeState.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

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
	EventTagProperty = StructProperty->GetChildHandle(TEXT("EventTag"));
	StateProperty = StructProperty->GetChildHandle(TEXT("State"));
	GateDelayProperty = StructProperty->GetChildHandle(TEXT("GateDelay"));
	ConditionsProperty = StructProperty->GetChildHandle(TEXT("Conditions"));

	HeaderRow
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
	if (TriggerProperty)
	{
		StructBuilder.AddProperty(TriggerProperty.ToSharedRef());
	}

	if (EventTagProperty)
	{
		IDetailPropertyRow& EventTagRow = StructBuilder.AddProperty(EventTagProperty.ToSharedRef());
		EventTagRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([this]()
		{
			uint8 TriggerValue = 0; 
			if (TriggerProperty.IsValid())
			{
				TriggerProperty->GetValue(TriggerValue);
			}
			return TriggerValue == (uint8)EStateTreeTransitionTrigger::OnEvent ? EVisibility::Visible : EVisibility::Collapsed;
		})));
	}
	
	if (GateDelayProperty)
	{
		StructBuilder.AddProperty(GateDelayProperty.ToSharedRef());
	}

	if (StateProperty)
	{
		StructBuilder.AddProperty(StateProperty.ToSharedRef());
	}

	if (ConditionsProperty)
	{
		// Show conditions always expanded, with simplified header (remove item count)
		IDetailPropertyRow& Property = StructBuilder.AddProperty(ConditionsProperty.ToSharedRef());
		Property.ShouldAutoExpand(true);

		static const bool bShowChildren = true;
		Property.CustomWidget(bShowChildren)
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
}

FText FStateTreeTransitionDetails::GetDescription() const
{
	if (StateProperty)
	{
		TArray<void*> RawData;
		StateProperty->AccessRawData(RawData);
		if (RawData.Num() == 1)
		{
			FStateTreeStateLink* State = static_cast<FStateTreeStateLink*>(RawData[0]);
			if (State != nullptr)
			{
				switch (State->Type)
				{
				case EStateTreeTransitionType::NotSet:
					return LOCTEXT("TransitionNotSet", "None");
					break;
				case EStateTreeTransitionType::Succeeded:
					return LOCTEXT("TransitionTreeSucceeded", "Tree Succeeded");
					break;
				case EStateTreeTransitionType::Failed:
					return LOCTEXT("TransitionTreeFailed", "Tree Failed");
					break;
				case EStateTreeTransitionType::NextState:
					return LOCTEXT("TransitionNextState", "Next State");
					break;
				case EStateTreeTransitionType::GotoState:
					{
						FFormatNamedArguments Args;
						Args.Add(TEXT("State"), FText::FromName(State->Name));
						return FText::Format(LOCTEXT("TransitionActionGotoState", "Go to State {State}"), Args);
					}
					break;
				}
			}
		}
		else
		{
			return LOCTEXT("MultipleSelected", "Multiple Selected");
		}
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE

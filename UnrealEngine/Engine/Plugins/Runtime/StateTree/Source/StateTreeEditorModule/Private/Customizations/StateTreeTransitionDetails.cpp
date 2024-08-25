// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTransitionDetails.h"
#include "Debugger/StateTreeDebuggerUIExtensions.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "ScopedTransaction.h"
#include "StateTreeEditor.h"
#include "StateTreeEditorData.h"
#include "StateTreePropertyHelpers.h"
#include "StateTreeTypes.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

TSharedRef<IPropertyTypeCustomization> FStateTreeTransitionDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeTransitionDetails);
}

void FStateTreeTransitionDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities();

	// Find StateTreeEditorData associated with this panel.
	UStateTreeEditorData* EditorData = nullptr;
	const TArray<TWeakObjectPtr<>>& Objects = PropUtils->GetSelectedObjects();
	for (const TWeakObjectPtr<>& WeakObject : Objects)
	{
		if (const UObject* Object = WeakObject.Get())
		{
			if (UStateTreeEditorData* OuterEditorData = Object->GetTypedOuter<UStateTreeEditorData>())
			{
				EditorData = OuterEditorData;
				break;
			}
		}
	}

	TriggerProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FStateTreeTransition, Trigger));
	PriorityProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FStateTreeTransition, Priority));
	EventTagProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FStateTreeTransition, EventTag));
	StateProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FStateTreeTransition, State));
	DelayTransitionProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FStateTreeTransition, bDelayTransition));
	DelayDurationProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FStateTreeTransition, DelayDuration));
	DelayRandomVarianceProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FStateTreeTransition, DelayRandomVariance));
	ConditionsProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FStateTreeTransition, Conditions));
	IDProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FStateTreeTransition, ID));

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
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.FillWidth(1.0f)
			[
				UE::StateTreeEditor::DebuggerExtensions::CreateTransitionWidget(StructPropertyHandle, EditorData)
			]
		]
		.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FStateTreeTransitionDetails::OnCopyTransition)))
		.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FStateTreeTransitionDetails::OnPasteTransition)));
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
	check(IDProperty);

	TWeakPtr<FStateTreeTransitionDetails> WeakSelf = SharedThis(this);
	auto IsTickOrEventTransition = [WeakSelf]()
	{
		if (const TSharedPtr<FStateTreeTransitionDetails> Self = WeakSelf.Pin())
		{
			return !EnumHasAnyFlags(Self->GetTrigger(), EStateTreeTransitionTrigger::OnStateCompleted) ? EVisibility::Visible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	};

	if (UE::StateTree::Editor::GbDisplayItemIds)
	{
		StructBuilder.AddProperty(IDProperty.ToSharedRef());
	}
	
	// Trigger
	StructBuilder.AddProperty(TriggerProperty.ToSharedRef());

	// Show event only when the trigger is set to Event. 
	StructBuilder.AddProperty(EventTagProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([WeakSelf]()
		{
			if (const TSharedPtr<FStateTreeTransitionDetails> Self = WeakSelf.Pin())
			{
				return (Self->GetTrigger() == EStateTreeTransitionTrigger::OnEvent) ? EVisibility::Visible : EVisibility::Collapsed;
			}
			return EVisibility::Collapsed;
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
		case EStateTreeTransitionType::NextSelectableState:
			TargetText = LOCTEXT("TransitionNextSelectableState", "Next Selectable State");
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

void FStateTreeTransitionDetails::OnCopyTransition() const
{
	FString Value;
	// Use PPF_Copy so that all properties get copied.
	if (StructProperty->GetValueAsFormattedString(Value, PPF_Copy) == FPropertyAccess::Success)
	{
		FPlatformApplicationMisc::ClipboardCopy(*Value);
	} 
}

UStateTreeEditorData* FStateTreeTransitionDetails::GetEditorData() const
{
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (UObject* Outer : OuterObjects)
	{
		UStateTreeEditorData* OuterEditorData = Cast<UStateTreeEditorData>(Outer);
		if (OuterEditorData == nullptr)
		{
			OuterEditorData = Outer->GetTypedOuter<UStateTreeEditorData>();
		}
		if (OuterEditorData)
		{
			return OuterEditorData;
		}
	}
	return nullptr;
}

void FStateTreeTransitionDetails::OnPasteTransition() const
{
	UStateTreeEditorData* EditorData = GetEditorData();
	if (!EditorData)
	{
		return;
	}
	FStateTreeEditorPropertyBindings* Bindings = EditorData->GetPropertyEditorBindings();
	if (!Bindings)
	{
		return;
	}

	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	if (PastedText.IsEmpty())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("PasteTransition", "Paste Transition"));

	StructProperty->NotifyPreChange();

	// Make sure we instantiate new objects when setting the value.
	StructProperty->SetValueFromFormattedString(PastedText, EPropertyValueSetFlags::InstanceObjects);

	// Reset GUIDs on paste
	TArray<void*> RawData;
	StructProperty->AccessRawData(RawData);
	for (int32 Index = 0; Index < RawData.Num(); Index++)
	{
		if (FStateTreeTransition* Transition = static_cast<FStateTreeTransition*>(RawData[Index]))
		{
			Transition->ID = FGuid::NewGuid();

			for (FStateTreeEditorNode& Condition : Transition->Conditions)
			{
				const FGuid OldStructID = Condition.ID;
				Condition.ID = FGuid::NewGuid();
				if (OldStructID.IsValid())
				{
					Bindings->CopyBindings(OldStructID, Condition.ID);
				}
			}
		}
	}

	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();

	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE

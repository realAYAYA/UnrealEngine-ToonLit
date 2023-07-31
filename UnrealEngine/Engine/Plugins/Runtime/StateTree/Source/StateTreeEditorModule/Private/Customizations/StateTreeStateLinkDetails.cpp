// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeStateLinkDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "StateTree.h"
#include "StateTreeState.h"
#include "StateTreeEditorData.h"
#include "StateTreeDelegates.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "StateTreePropertyHelpers.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

TSharedRef<IPropertyTypeCustomization> FStateTreeStateLinkDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeStateLinkDetails);
}

void FStateTreeStateLinkDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

	NameProperty = StructProperty->GetChildHandle(TEXT("Name"));
	IDProperty = StructProperty->GetChildHandle(TEXT("ID"));
	TypeProperty = StructProperty->GetChildHandle(TEXT("Type"));

	if (const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty())
	{
		static const FName NAME_DirectStatesOnly = "DirectStatesOnly";
		static const FName NAME_SubtreesOnly = "SubtreesOnly";
		
		bDirectStatesOnly = MetaDataProperty->HasMetaData(NAME_DirectStatesOnly);
		bSubtreesOnly = MetaDataProperty->HasMetaData(NAME_SubtreesOnly);
	}
	
	CacheStates();

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &FStateTreeStateLinkDetails::OnGetStateContent)
			.ContentPadding(FMargin(6.f, 0.f))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &FStateTreeStateLinkDetails::GetCurrentStateDesc)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];

	UE::StateTree::Delegates::OnIdentifierChanged.AddSP(this, &FStateTreeStateLinkDetails::OnIdentifierChanged);
}

void FStateTreeStateLinkDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FStateTreeStateLinkDetails::OnIdentifierChanged(const UStateTree& StateTree)
{
	CacheStates();
}

void FStateTreeStateLinkDetails::CacheStates(const UStateTreeState* State)
{
	if (State == nullptr)
	{
		return;
	}

	bool bShouldAdd = true;
	if (bSubtreesOnly && State->Type != EStateTreeStateType::Subtree)
	{
		bShouldAdd = false;
	}

	if (bShouldAdd)
	{
		CachedNames.Add(State->Name);
		CachedIDs.Add(State->ID);
	}

	for (UStateTreeState* ChildState : State->Children)
	{
		CacheStates(ChildState);
	}
}

void FStateTreeStateLinkDetails::CacheStates()
{
	CachedNames.Reset();
	CachedIDs.Reset();

	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (int32 ObjectIdx = 0; ObjectIdx < OuterObjects.Num(); ObjectIdx++)
	{
		UStateTree* OuterStateTree = OuterObjects[ObjectIdx]->GetTypedOuter<UStateTree>();
		if (OuterStateTree)
		{
			if (UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(OuterStateTree->EditorData))
			{
				for (const UStateTreeState* SubTree : TreeData->SubTrees)
				{
					CacheStates(SubTree);
				}
			}
			break;
		}
	}

}

void FStateTreeStateLinkDetails::OnStateComboChange(int Idx)
{
	if (NameProperty && IDProperty)
	{
		FScopedTransaction Transaction(FText::Format(LOCTEXT("SetPropertyValue", "Set {0}"), StructProperty->GetPropertyDisplayName()));
		if (Idx >= 0)
		{
			TypeProperty->SetValue((uint8)EStateTreeTransitionType::GotoState);
			NameProperty->SetValue(CachedNames[Idx], EPropertyValueSetFlags::NotTransactable);
			UE::StateTree::PropertyHelpers::SetStructValue<FGuid>(IDProperty, CachedIDs[Idx], EPropertyValueSetFlags::NotTransactable);
		}
		else
		{
			switch (Idx)
			{
			case ComboSucceeded:
				TypeProperty->SetValue((uint8)EStateTreeTransitionType::Succeeded);
				break;
			case ComboFailed:
				TypeProperty->SetValue((uint8)EStateTreeTransitionType::Failed);
				break;
			case ComboNextState:
				TypeProperty->SetValue((uint8)EStateTreeTransitionType::NextState);
				break;
			case ComboNotSet:
			default:
				TypeProperty->SetValue((uint8)EStateTreeTransitionType::NotSet);
				break;
			}
			// Clear name and id.
			NameProperty->SetValue(FName(), EPropertyValueSetFlags::NotTransactable);
			UE::StateTree::PropertyHelpers::SetStructValue<FGuid>(IDProperty, FGuid(), EPropertyValueSetFlags::NotTransactable);
		}
	}
}

TSharedRef<SWidget> FStateTreeStateLinkDetails::OnGetStateContent() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	if (!bDirectStatesOnly)
	{
		FUIAction NextItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, ComboNextState));
		MenuBuilder.AddMenuEntry(LOCTEXT("TransitionNextState", "Next State"), LOCTEXT("TransitionNextTooltip", "Goto next sibling State."), FSlateIcon(), NextItemAction);

		FUIAction NotSetItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, ComboNotSet));
		MenuBuilder.AddMenuEntry(LOCTEXT("TransitionBlock", "Block Transition"), LOCTEXT("TransitionBlockTooltip", "Will not transition to any state, but will block other transitions to trigger if the condition passes."), FSlateIcon(), NotSetItemAction);

		FUIAction SucceededItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, ComboSucceeded));
		MenuBuilder.AddMenuEntry(LOCTEXT("TransitionTreeSucceeded", "Tree Succeeded"), LOCTEXT("TransitionTreeSuccessTooltip", "Complete tree with success."), FSlateIcon(), SucceededItemAction);

		FUIAction FailedItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, ComboFailed));
		MenuBuilder.AddMenuEntry(LOCTEXT("TransitionTreeFailed", "Tree Failed"), LOCTEXT("TransitionTreeFailedTooltip", "Complete tree with failure."), FSlateIcon(), FailedItemAction);
	}

	if (CachedNames.Num() > 0)
	{
		MenuBuilder.BeginSection(FName(), LOCTEXT("TransitionGotoState", "Goto State"));

		for (int32 Idx = 0; Idx < CachedNames.Num(); Idx++)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("StateName"), FText::FromName(CachedNames[Idx]));

			FUIAction ItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, Idx));
			MenuBuilder.AddMenuEntry(FText::FromName(CachedNames[Idx]), FText::Format(LOCTEXT("TransitionGotoStateTooltip", "Goto State {StateName}."), Args), FSlateIcon(), ItemAction);
		}

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

FText FStateTreeStateLinkDetails::GetCurrentStateDesc() const
{
	const EStateTreeTransitionType TransitionType = GetTransitionType().Get(EStateTreeTransitionType::Failed);

	if (TransitionType == EStateTreeTransitionType::NotSet)
	{
		return LOCTEXT("TransitionNotSet", "None");
	}
	else if (TransitionType == EStateTreeTransitionType::NextState)
	{
		return LOCTEXT("TransitionNextState", "Next State");
	}
	else if (TransitionType == EStateTreeTransitionType::Succeeded)
	{
		return LOCTEXT("TransitionTreeSucceeded", "Tree Succeeded");
	}
	else if (TransitionType == EStateTreeTransitionType::Failed)
	{
		return LOCTEXT("TransitionTreeFailed", "Tree Failed");
	}
	else if (TransitionType == EStateTreeTransitionType::GotoState)
	{
		FName OldName;
		if (NameProperty && IDProperty)
		{
			NameProperty->GetValue(OldName);

			FGuid StateID;
			if (UE::StateTree::PropertyHelpers::GetStructValue<FGuid>(IDProperty, StateID) == FPropertyAccess::Success)
			{
				if (!StateID.IsValid())
				{
					return LOCTEXT("None", "None");
				}
				else
				{
					for (int32 Idx = 0; Idx < CachedIDs.Num(); Idx++)
					{
						if (CachedIDs[Idx] == StateID)
						{
							return FText::FromName(CachedNames[Idx]);
						}
					}
				}
			}
		}

		FFormatNamedArguments Args;
		Args.Add(TEXT("Identifier"), FText::FromName(OldName));
		return FText::Format(LOCTEXT("InvalidReference", "Invalid Reference {Identifier}"), Args);
	}

	return LOCTEXT("TransitionInvalid", "Invalid");
}

TOptional<EStateTreeTransitionType> FStateTreeStateLinkDetails::GetTransitionType() const
{
	if (TypeProperty)
	{
		uint8 Value;
		if (TypeProperty->GetValue(Value) == FPropertyAccess::Success)
		{
			return EStateTreeTransitionType(Value);
		}
	}
	return TOptional<EStateTreeTransitionType>();
}

#undef LOCTEXT_NAMESPACE

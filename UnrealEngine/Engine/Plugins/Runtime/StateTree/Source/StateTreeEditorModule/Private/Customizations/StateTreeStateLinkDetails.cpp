// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeStateLinkDetails.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeDelegates.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
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
	LinkTypeProperty = StructProperty->GetChildHandle(TEXT("LinkType"));

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
			.ButtonContent()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0,0,4,0)
				[
					SNew(SBox)
					[
						SNew(SImage)
						.ToolTipText(LOCTEXT("MissingState", "The specified state cannot be found."))
						.Visibility_Lambda([this]()
						{
							return IsValidLink() ? EVisibility::Collapsed : EVisibility::Visible;
						})
						.Image(FAppStyle::GetBrush("Icons.ErrorWithColor"))
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FStateTreeStateLinkDetails::GetCurrentStateDesc)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
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

	if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::None)
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
			LinkTypeProperty->SetValue((uint8)EStateTreeTransitionType::GotoState);
			NameProperty->SetValue(CachedNames[Idx], EPropertyValueSetFlags::NotTransactable);
			UE::StateTree::PropertyHelpers::SetStructValue<FGuid>(IDProperty, CachedIDs[Idx], EPropertyValueSetFlags::NotTransactable);
		}
		else
		{
			switch (Idx)
			{
			case ComboSucceeded:
				LinkTypeProperty->SetValue((uint8)EStateTreeTransitionType::Succeeded);
				break;
			case ComboFailed:
				LinkTypeProperty->SetValue((uint8)EStateTreeTransitionType::Failed);
				break;
			case ComboNextState:
				LinkTypeProperty->SetValue((uint8)EStateTreeTransitionType::NextState);
				break;
			case ComboNextSelectableState:
				LinkTypeProperty->SetValue((uint8)EStateTreeTransitionType::NextSelectableState);
				break;
			case ComboNotSet:
			default:
				LinkTypeProperty->SetValue((uint8)EStateTreeTransitionType::None);
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
		const FUIAction NotSetItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, ComboNotSet));
		MenuBuilder.AddMenuEntry(LOCTEXT("TransitionNone", "None"), LOCTEXT("TransitionNoneTooltip", "No transition."), FSlateIcon(), NotSetItemAction);

		const FUIAction NextItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, ComboNextState));
		MenuBuilder.AddMenuEntry(LOCTEXT("TransitionNextState", "Next State"), LOCTEXT("TransitionNextTooltip", "Goto next sibling State."), FSlateIcon(), NextItemAction);

		const FUIAction NextSelectableItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, ComboNextSelectableState));
		MenuBuilder.AddMenuEntry(LOCTEXT("TransitionNextSelectableState", "Next Selectable State"), LOCTEXT("TransitionNextSelectableTooltip", "Goto next sibling state, whose enter conditions pass."), FSlateIcon(), NextSelectableItemAction);

		const FUIAction SucceededItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, ComboSucceeded));
		MenuBuilder.AddMenuEntry(LOCTEXT("TransitionTreeSucceeded", "Tree Succeeded"), LOCTEXT("TransitionTreeSuccessTooltip", "Complete tree with success."), FSlateIcon(), SucceededItemAction);

		const FUIAction FailedItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, ComboFailed));
		MenuBuilder.AddMenuEntry(LOCTEXT("TransitionTreeFailed", "Tree Failed"), LOCTEXT("TransitionTreeFailedTooltip", "Complete tree with failure."), FSlateIcon(), FailedItemAction);
	}

	if (CachedNames.Num() > 0)
	{
		MenuBuilder.BeginSection(FName(), LOCTEXT("TransitionGotoStateSection", "Go to State"));

		for (int32 Idx = 0; Idx < CachedNames.Num(); Idx++)
		{
			FUIAction ItemAction(FExecuteAction::CreateSP(const_cast<FStateTreeStateLinkDetails*>(this), &FStateTreeStateLinkDetails::OnStateComboChange, Idx));
			MenuBuilder.AddMenuEntry(FText::FromName(CachedNames[Idx]), FText::Format(LOCTEXT("TransitionGotoStateTooltip", "Go to State {0}."), FText::FromName(CachedNames[Idx])), FSlateIcon(), ItemAction);
		}

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

FText FStateTreeStateLinkDetails::GetCurrentStateDesc() const
{
	const EStateTreeTransitionType TransitionType = GetTransitionType().Get(EStateTreeTransitionType::Failed);

	FText Result;
	switch (TransitionType)
	{
	case EStateTreeTransitionType::None:
		Result = LOCTEXT("TransitionNone", "None");
		break;
	case EStateTreeTransitionType::NextState:
		Result = LOCTEXT("TransitionNextState", "Next State");
		break;
	case EStateTreeTransitionType::NextSelectableState:
		Result = LOCTEXT("TransitionNextSelectableState", "Next Selectable State");
		break;
	case EStateTreeTransitionType::Succeeded:
		Result = LOCTEXT("TransitionTreeSucceeded", "Tree Succeeded");
		break;
	case EStateTreeTransitionType::Failed:
		Result = LOCTEXT("TransitionTreeFailed", "Tree Failed");
		break;
	case EStateTreeTransitionType::GotoState:
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

			return FText::FromName(OldName);
		}
		break;
	default:
		ensureMsgf(false, TEXT("FStateTreeStateLinkDetails: Unhnandled enum %s"), *UEnum::GetValueAsString(TransitionType));
		Result = LOCTEXT("TransitionInvalid", "Invalid");
		break;
	}

	return Result;
}

bool FStateTreeStateLinkDetails::IsValidLink() const
{
	const EStateTreeTransitionType TransitionType = GetTransitionType().Get(EStateTreeTransitionType::Failed);

	bool bIsValid = false;
	switch (TransitionType)
	{
	// all these are unconditionally valid		
	case EStateTreeTransitionType::None: // fall through on purpose
	case EStateTreeTransitionType::NextState: // fall through on purpose
	case EStateTreeTransitionType::NextSelectableState: // fall through on purpose
	case EStateTreeTransitionType::Succeeded: // fall through on purpose
	case EStateTreeTransitionType::Failed:
		bIsValid = true;
		break;
	case EStateTreeTransitionType::GotoState:
		{
			bIsValid = false;
			if (NameProperty && IDProperty)
			{
				FGuid StateID;
				if (UE::StateTree::PropertyHelpers::GetStructValue<FGuid>(IDProperty, StateID) == FPropertyAccess::Success)
				{
					bIsValid = !StateID.IsValid() || CachedIDs.Contains(StateID);
				}
			}
		}
		break;
	default:
		bIsValid = false;
		break;
	}

	return bIsValid;
}

TOptional<EStateTreeTransitionType> FStateTreeStateLinkDetails::GetTransitionType() const
{
	if (LinkTypeProperty)
	{
		uint8 Value;
		if (LinkTypeProperty->GetValue(Value) == FPropertyAccess::Success)
		{
			return EStateTreeTransitionType(Value);
		}
	}
	return TOptional<EStateTreeTransitionType>();
}

#undef LOCTEXT_NAMESPACE

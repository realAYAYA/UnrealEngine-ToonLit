// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStateTreeViewRow.h"
#include "SStateTreeView.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Templates/SharedPointer.h"

#include "Styling/AppStyle.h"
#include "EditorFontGlyphs.h"
#include "StateTreeEditorStyle.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Layout/SScrollBox.h"

#include "StateTree.h"
#include "StateTreeState.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "StateTreeViewModel.h"
#include "Algo/ForEach.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

void SStateTreeViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TWeakObjectPtr<UStateTreeState> InState, const TSharedPtr<SScrollBox>& ViewBox, TSharedRef<FStateTreeViewModel> InStateTreeViewModel)
{
	StateTreeViewModel = InStateTreeViewModel;
	WeakState = InState;

	STableRow<TWeakObjectPtr<UStateTreeState>>::ConstructInternal(STableRow::FArguments()
        .Padding(5.0f)
        .OnDragDetected(this, &SStateTreeViewRow::HandleDragDetected)
        .OnCanAcceptDrop(this, &SStateTreeViewRow::HandleCanAcceptDrop)
        .OnAcceptDrop(this, &SStateTreeViewRow::HandleAcceptDrop)
        .Style(&FStateTreeEditorStyle::Get()->GetWidgetStyle<FTableRowStyle>("StateTree.Selection"))
    , InOwnerTableView);

	static const FLinearColor TasksBackground = FLinearColor(FColor(17, 117, 131));
	static const FLinearColor LinkBackground = FLinearColor(FColor(84, 84, 84));

	this->ChildSlot
    .HAlign(HAlign_Fill)
    [
	    SNew(SBox)
	    .MinDesiredWidth_Lambda([ViewBox]()
			{
    			// Make the row at least as wide as the view.
	    		// The -1 is needed or we'll see a scrollbar. 
				return ViewBox->GetTickSpaceGeometry().GetLocalSize().X - 1;
			})
		[
	        SNew(SHorizontalBox)
	        + SHorizontalBox::Slot()
	        .VAlign(VAlign_Fill)
	        .HAlign(HAlign_Left)
	        .AutoWidth()
	        [
	            SNew(SExpanderArrow, SharedThis(this))
	            .ShouldDrawWires(true)
	            .IndentAmount(32)
	            .BaseIndentLevel(0)
	        ]

	        // State Box
	        + SHorizontalBox::Slot()
	        .VAlign(VAlign_Center)
	        .Padding(FMargin(0.0f, 4.0f))
	        .AutoWidth()
	        [
	            SNew(SBox)
	            .HeightOverride(28.0f)
	            .VAlign(VAlign_Fill)
	            [
	                SNew(SBorder)
	                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
	                .BorderBackgroundColor(this, &SStateTreeViewRow::GetTitleColor)
	                .Padding(FMargin(16.0f, 0.0f, 16.0f, 0.0f))
	                [
	                    // Conditions icon
	                    SNew(SHorizontalBox)
	                    + SHorizontalBox::Slot()
	                    .VAlign(VAlign_Center)
	                    .AutoWidth()
	                    [
	                        SNew(SBox)
	                        .Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
	                        .Visibility(this, &SStateTreeViewRow::GetConditionVisibility)
	                        [
	                            SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Help"))
							]
						]

						// Selector icon
	                    + SHorizontalBox::Slot()
	                    .VAlign(VAlign_Center)
	                    .AutoWidth()
	                    [
	                        SNew(SBox)
	                        .Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
	                        .Visibility(this, &SStateTreeViewRow::GetSelectorVisibility)
	                        [
	                            SNew(STextBlock)
	                            .Text(this, &SStateTreeViewRow::GetSelectorDesc)
	                            .TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
	                        ]
	                    ]

	                    // State Name
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SAssignNew(NameTextBlock, SInlineEditableTextBlock)
							.Style(FStateTreeEditorStyle::Get(), "StateTree.State.TitleInlineEditableText")
							.OnVerifyTextChanged_Lambda([](const FText& NewLabel, FText& OutErrorMessage)
							{
								return !NewLabel.IsEmptyOrWhitespace();
							})
							.OnTextCommitted(this, &SStateTreeViewRow::HandleNodeLabelTextCommitted)
							.Text(this, &SStateTreeViewRow::GetStateDesc)
							.Clipping(EWidgetClipping::ClipToBounds)
							.IsSelected(this, &SStateTreeViewRow::IsStateSelected)
						]
					]
				]
			]

			// Tasks Box
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 4.0f))
			.AutoWidth()
			[
				SNew(SBox)
				.HeightOverride(28.0f)
				.VAlign(VAlign_Fill)
				.Visibility(this, &SStateTreeViewRow::GetTasksVisibility)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(TasksBackground)
					.Padding(FMargin(12.0f, 0.0f, 16.0f, 0.0f))
					[
						// Task icon
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(FEditorFontGlyphs::Paper_Plane)
							.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.DetailsIcon")
						]

						// Tasks list
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(this, &SStateTreeViewRow::GetTasksDesc)
							.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Details")
						]
					]
				]
			]
			
			// Linked State box
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 4.0f))
			.AutoWidth()
			[
				SNew(SBox)
				.HeightOverride(28.0f)
				.VAlign(VAlign_Fill)
				.Visibility(this, &SStateTreeViewRow::GetLinkedStateVisibility)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(LinkBackground)
					.Padding(FMargin(12.0f, 0.0f, 16.0f, 0.0f))
					[
						// Link icon
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(FEditorFontGlyphs::Link)
							.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.DetailsIcon")
						]

						// Linked State
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(this, &SStateTreeViewRow::GetLinkedStateDesc)
							.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Details")
						]
					]
				]
			]
			
			// Completed transitions
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(FMargin(8.0f, 0.0f, 0, 0.0f))
				[
					SNew(STextBlock)
					.Text(this, &SStateTreeViewRow::GetCompletedTransitionsIcon)
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
					.Visibility(this, &SStateTreeViewRow::GetCompletedTransitionVisibility)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(FMargin(4.0f, 0, 0, 0))
				[
					SNew(STextBlock)
					.Text(this, &SStateTreeViewRow::GetCompletedTransitionsDesc)
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Details")
					.Visibility(this, &SStateTreeViewRow::GetCompletedTransitionVisibility)
				]
			]

			// Succeeded transitions
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(8.0f, 0.0f, 0, 0))
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FEditorFontGlyphs::Check_Circle)
	                .ColorAndOpacity(FLinearColor(FColor(110,143,67)))
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
					.Visibility(this, &SStateTreeViewRow::GetSucceededTransitionVisibility)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(4.0f, 0.0f, 0, 0))
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(this, &SStateTreeViewRow::GetSucceededTransitionIcon)
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
					.Visibility(this, &SStateTreeViewRow::GetSucceededTransitionVisibility)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(FMargin(4.0f, 0, 0, 0))
				[
					SNew(STextBlock)
					.Text(this, &SStateTreeViewRow::GetSucceededTransitionDesc)
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Details")
					.Visibility(this, &SStateTreeViewRow::GetSucceededTransitionVisibility)
				]
			]

			// Failed transitions
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(8.0f, 0.0f, 0, 0))
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FEditorFontGlyphs::Times_Circle)
	                .ColorAndOpacity(FLinearColor(FColor(187,77,42)))
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
					.Visibility(this, &SStateTreeViewRow::GetFailedTransitionVisibility)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(4.0f, 0.0f, 0, 0))
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(this, &SStateTreeViewRow::GetFailedTransitionIcon)
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
					.Visibility(this, &SStateTreeViewRow::GetFailedTransitionVisibility)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(FMargin(4.0f, 0, 0, 0))
				[
					SNew(STextBlock)
					.Text(this, &SStateTreeViewRow::GetFailedTransitionDesc)
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Details")
					.Visibility(this, &SStateTreeViewRow::GetFailedTransitionVisibility)
				]
			]
			// Transitions
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(8.0f, 0.0f, 0, 0))
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Help"))
	                .ColorAndOpacity(FLinearColor(FColor(31,151,167)))
					.Visibility(this, &SStateTreeViewRow::GetConditionalTransitionsVisibility)
				]
	            + SHorizontalBox::Slot()
	            .VAlign(VAlign_Center)
	            .Padding(FMargin(4.0f, 0.0f, 0, 0))
	            .AutoWidth()
	            [
	                SNew(STextBlock)
	                .Text(FEditorFontGlyphs::Long_Arrow_Right)
	                .TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
	                .Visibility(this, &SStateTreeViewRow::GetConditionalTransitionsVisibility)
	            ]
	            + SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(FMargin(4.0f, 0, 0, 0))
				[
					SNew(STextBlock)
					.Text(this, &SStateTreeViewRow::GetConditionalTransitionsDesc)
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Details")
					.Visibility(this, &SStateTreeViewRow::GetConditionalTransitionsVisibility)
				]
			]
		]
	];
}

void SStateTreeViewRow::RequestRename() const
{
	if (NameTextBlock)
	{
		NameTextBlock->EnterEditingMode();
	}
}

FSlateColor SStateTreeViewRow::GetTitleColor() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (StateTreeViewModel && StateTreeViewModel->IsSelected(State))
		{
			return FLinearColor(FColor(236, 134, 39));
		}
		if (IsRootState() || State->Type == EStateTreeStateType::Subtree)
		{
			return FLinearColor(FColor(17, 117, 131));
		}
	}

	return FLinearColor(FColor(31, 151, 167));
}

FText SStateTreeViewRow::GetStateDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return FText::FromName(State->Name);
	}
	return FText::FromName(FName());
}

EVisibility SStateTreeViewRow::GetConditionVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return State->EnterConditions.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetSelectorVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (State->Type == EStateTreeStateType::State || State->Type == EStateTreeStateType::Group)
		{
			return State->Children.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
		}
		else if (IsRootState() || State->Type == EStateTreeStateType::Subtree)
		{
			return EVisibility::Visible;
		}
		else if (State->Type == EStateTreeStateType::Linked)
		{
			return EVisibility::Visible;
		}
	}
	
	return EVisibility::Collapsed;
}


FText SStateTreeViewRow::GetSelectorDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (State->Type == EStateTreeStateType::State || State->Type == EStateTreeStateType::Group)
		{
			return FEditorFontGlyphs::Level_Down; // Selector
		}
		else if (IsRootState() || State->Type == EStateTreeStateType::Subtree)
		{
			return FEditorFontGlyphs::Cogs;
		}
		else if (State->Type == EStateTreeStateType::Linked)
		{
			return FEditorFontGlyphs::Link;
		}
	}

	return FText::GetEmpty(); 
}

EVisibility SStateTreeViewRow::GetTasksVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		int32 ValidCount = 0;
		for (int32 i = 0; i < State->Tasks.Num(); i++)
		{
			if (const FStateTreeTaskBase* Task = State->Tasks[i].Node.GetPtr<FStateTreeTaskBase>())
			{
				ValidCount++;
			}
		}
		return ValidCount > 0 ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

FText SStateTreeViewRow::GetTasksDesc() const
{
	const UStateTreeState* State = WeakState.Get();
	if (!State)
	{
		return FText::GetEmpty();
	}

	TArray<FText> Names;
	for (int32 i = 0; i < State->Tasks.Num(); i++)
	{
		if (const FStateTreeTaskBase* Task = State->Tasks[i].Node.GetPtr<FStateTreeTaskBase>())
		{
			Names.Add(FText::FromName(Task->Name));
		}
	}

	return FText::Join((FText::FromString(TEXT(" & "))), Names);
}

EVisibility SStateTreeViewRow::GetLinkedStateVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return State->Type == EStateTreeStateType::Linked ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

FText SStateTreeViewRow::GetLinkedStateDesc() const
{
	const UStateTreeState* State = WeakState.Get();
	if (!State)
	{
		return FText::GetEmpty();
	}

	if (State->Type == EStateTreeStateType::Linked)
	{
		return FText::FromName(State->LinkedSubtree.Name);
	}
	
	return FText::GetEmpty();
}

bool SStateTreeViewRow::HasParentTransitionForTrigger(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger) const
{
	EStateTreeTransitionTrigger CombinedTrigger = EStateTreeTransitionTrigger::None;
	for (const UStateTreeState* ParentState = State.Parent; ParentState != nullptr; ParentState = ParentState->Parent)
	{
		for (const FStateTreeTransition& Transition : ParentState->Transitions)
		{
			CombinedTrigger |= Transition.Trigger;
		}
	}
	return EnumHasAllFlags(CombinedTrigger, Trigger);
}

FText SStateTreeViewRow::GetTransitionsDesc(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger, const bool bUseMask) const
{
	TArray<FText> DescItems;
	for (const FStateTreeTransition& Transition : State.Transitions)
	{
		const bool bMatch = bUseMask ? EnumHasAnyFlags(Transition.Trigger, Trigger) : Transition.Trigger == Trigger;
		if (bMatch)
		{
			switch (Transition.State.Type)
			{
			case EStateTreeTransitionType::NotSet:
				DescItems.Add(LOCTEXT("TransitionNoneStyled", "[None]"));
				break;
			case EStateTreeTransitionType::Succeeded:
				DescItems.Add(LOCTEXT("TransitionTreeSucceededStyled", "[Succeeded]"));
				break;
			case EStateTreeTransitionType::Failed:
				DescItems.Add(LOCTEXT("TransitionTreeFailedStyled", "[Failed]"));
				break;
			case EStateTreeTransitionType::NextState:
				DescItems.Add(LOCTEXT("TransitionNextStateStyled", "[Next]"));
				break;
			case EStateTreeTransitionType::GotoState:
				DescItems.Add(FText::FromName(Transition.State.Name));
				break;
			default:
				ensureMsgf(false, TEXT("Unhandled transition type."));
				break;
			}
		}
	}

	if (State.Children.Num() == 0
		&& State.Type == EStateTreeStateType::State
		&& DescItems.Num() == 0
		&& EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnStateCompleted))
	{
		if (HasParentTransitionForTrigger(State, Trigger))
		{
			DescItems.Add(LOCTEXT("TransitionActionHandleInParentStyled", "[Parent]"));
		}
		else
		{
			DescItems.Add(LOCTEXT("TransitionActionMissingTransition", "Missing Transition"));
		}
	}
	
	return FText::Join(FText::FromString(TEXT(", ")), DescItems);
}

FText SStateTreeViewRow::GetTransitionsIcon(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger, const bool bUseMask) const
{
	enum EIconType
	{
		IconNone = 0,
        IconRightArrow =	1 << 0,
        IconDownArrow =		1 << 1,
        IconLevelUp =		1 << 2,
        IconWarning =		1 << 3,
    };
	uint8 IconType = IconNone;
	
	for (const FStateTreeTransition& Transition : State.Transitions)
	{
		// The icons here depict "transition direction", not the type specifically.
		const bool bMatch = bUseMask ? EnumHasAnyFlags(Transition.Trigger, Trigger) : Transition.Trigger == Trigger;
		if (bMatch)
		{
			switch (Transition.State.Type)
			{
			case EStateTreeTransitionType::NotSet:
				IconType |= IconRightArrow;
				break;
			case EStateTreeTransitionType::Succeeded:
				IconType |= IconRightArrow;
				break;
			case EStateTreeTransitionType::Failed:
				IconType |= IconRightArrow;
				break;
			case EStateTreeTransitionType::NextState:
				IconType |= IconDownArrow;
				break;
			case EStateTreeTransitionType::GotoState:
				IconType |= IconRightArrow;
				break;
			default:
				ensureMsgf(false, TEXT("Unhandled transition type."));
				break;
			}
		}
	}

	if (FMath::CountBits(static_cast<uint64>(IconType)) > 1)
	{
		// Prune down to just one icon.
		IconType = IconRightArrow;
	}
	
	if (State.Children.Num() == 0
		&& State.Type == EStateTreeStateType::State
        && IconType == IconNone
        && EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnStateCompleted))
	{
		if (HasParentTransitionForTrigger(State, Trigger))
		{
			IconType = IconLevelUp;
		}
		else
		{
			IconType = IconWarning;
		}
	}

	switch (IconType)
	{
		case IconRightArrow:
			return FEditorFontGlyphs::Long_Arrow_Right;
		case IconDownArrow:
			return FEditorFontGlyphs::Long_Arrow_Down;
		case IconLevelUp:
			return FEditorFontGlyphs::Level_Up;
		case IconWarning:
			return FEditorFontGlyphs::Exclamation_Triangle;
		default:
			return FText::GetEmpty();
	}

	return FText::GetEmpty();
}

EVisibility SStateTreeViewRow::GetTransitionsVisibility(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger) const
{
	// Handle completed, succeeeded and failed transitions.
	if (EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnStateCompleted))
	{
		const bool bIsLeafState = (State.Children.Num() == 0);
		EStateTreeTransitionTrigger HandledTriggers = EStateTreeTransitionTrigger::None;
		bool bExactMatch = false;

		for (const FStateTreeTransition& Transition : State.Transitions)
		{
			HandledTriggers |= Transition.Trigger;
			bExactMatch |= (Transition.Trigger == Trigger);
		}

		// Assume that leaf states should have completion transitions.
		if (!bExactMatch && bIsLeafState)
		{
			// Find the missing transition type, note: Completed = Succeeded|Failed.
			const EStateTreeTransitionTrigger MissingTriggers = HandledTriggers ^ EStateTreeTransitionTrigger::OnStateCompleted;
			return MissingTriggers == Trigger ? EVisibility::Visible : EVisibility::Collapsed;
		}
		
		return bExactMatch ? EVisibility::Visible : EVisibility::Collapsed;
	}

	// Handle the test
	for (const FStateTreeTransition& Transition : State.Transitions)
	{
		if (EnumHasAnyFlags(Trigger, Transition.Trigger))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetCompletedTransitionVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsVisibility(*State, EStateTreeTransitionTrigger::OnStateCompleted);
	}
	return EVisibility::Visible;
}

FText SStateTreeViewRow::GetCompletedTransitionsDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateCompleted);
	}
	return LOCTEXT("Invalid", "Invalid");
}

FText SStateTreeViewRow::GetCompletedTransitionsIcon() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsIcon(*State, EStateTreeTransitionTrigger::OnStateCompleted);
	}
	return FText::GetEmpty();
}

EVisibility SStateTreeViewRow::GetSucceededTransitionVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsVisibility(*State, EStateTreeTransitionTrigger::OnStateSucceeded);
	}
	return EVisibility::Collapsed;
}

FText SStateTreeViewRow::GetSucceededTransitionDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateSucceeded);
	}
	return FText::GetEmpty();
}

FText SStateTreeViewRow::GetSucceededTransitionIcon() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsIcon(*State, EStateTreeTransitionTrigger::OnStateSucceeded);
	}
	return FText::GetEmpty();
}

EVisibility SStateTreeViewRow::GetFailedTransitionVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsVisibility(*State, EStateTreeTransitionTrigger::OnStateFailed);
	}
	return EVisibility::Collapsed;
}

FText SStateTreeViewRow::GetFailedTransitionDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateFailed);
	}
	return LOCTEXT("Invalid", "Invalid");
}

FText SStateTreeViewRow::GetFailedTransitionIcon() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsIcon(*State, EStateTreeTransitionTrigger::OnStateFailed);
	}
	return FEditorFontGlyphs::Ban;
}

EVisibility SStateTreeViewRow::GetConditionalTransitionsVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsVisibility(*State, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent);
	}
	return EVisibility::Collapsed;
}

FText SStateTreeViewRow::GetConditionalTransitionsDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent, /*bUseMask*/true);
	}
	return FText::GetEmpty();
}

bool SStateTreeViewRow::IsRootState() const
{
	// Routines can be identified by not having parent state.
	const UStateTreeState* State = WeakState.Get();
	return State ? State->Parent == nullptr : false;
}

bool SStateTreeViewRow::IsStateSelected() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (StateTreeViewModel)
		{
			return StateTreeViewModel->IsSelected(State);
		}
	}
	return false;
}

void SStateTreeViewRow::HandleNodeLabelTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType) const
{
	if (StateTreeViewModel)
	{
		if (UStateTreeState* State = WeakState.Get())
		{
			StateTreeViewModel->RenameState(State, FName(*FText::TrimPrecedingAndTrailing(NewLabel).ToString()));
		}
	}
}

FReply SStateTreeViewRow::HandleDragDetected(const FGeometry&, const FPointerEvent&) const
{
	return FReply::Handled().BeginDragDrop(FActionTreeViewDragDrop::New(WeakState.Get()));
}

TOptional<EItemDropZone> SStateTreeViewRow::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TWeakObjectPtr<UStateTreeState> TargetState) const
{
	const TSharedPtr<FActionTreeViewDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FActionTreeViewDragDrop>();
	if (DragDropOperation.IsValid())
	{
		// Cannot drop on selection or child of selection.
		if (StateTreeViewModel && StateTreeViewModel->IsChildOfSelection(TargetState.Get()))
		{
			return TOptional<EItemDropZone>();
		}

		return DropZone;
	}

	return TOptional<EItemDropZone>();
}

FReply SStateTreeViewRow::HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TWeakObjectPtr<UStateTreeState> TargetState) const
{
	const TSharedPtr<FActionTreeViewDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FActionTreeViewDragDrop>();
	if (DragDropOperation.IsValid())
	{
		if (StateTreeViewModel)
		{
			if (DropZone == EItemDropZone::AboveItem)
			{
				StateTreeViewModel->MoveSelectedStatesBefore(TargetState.Get());
			}
			else if (DropZone == EItemDropZone::BelowItem)
			{
				StateTreeViewModel->MoveSelectedStatesAfter(TargetState.Get());
			}
			else
			{
				StateTreeViewModel->MoveSelectedStatesInto(TargetState.Get());
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE

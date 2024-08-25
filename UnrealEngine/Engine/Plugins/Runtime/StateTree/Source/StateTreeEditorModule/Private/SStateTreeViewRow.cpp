// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStateTreeViewRow.h"
#include "SStateTreeView.h"


#include "EditorFontGlyphs.h"
#include "StateTreeEditor.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorStyle.h"

#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"

#include "StateTree.h"
#include "StateTreeState.h"
#include "StateTreeTaskBase.h"
#include "StateTreeViewModel.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

namespace UE::StateTree::Editor
{
	FLinearColor LerpColorSRGB(const FLinearColor ColorA, FLinearColor ColorB, float T)
	{
		const FColor A = ColorA.ToFColorSRGB();
		const FColor B = ColorB.ToFColorSRGB();
		return FLinearColor(FColor(
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.R) * (1.f - T) + static_cast<float>(B.R) * T)),
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.G) * (1.f - T) + static_cast<float>(B.G) * T)),
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.B) * (1.f - T) + static_cast<float>(B.B) * T)),
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.A) * (1.f - T) + static_cast<float>(B.A) * T))));
	}
} // UE:StateTree::Editor

void SStateTreeViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TWeakObjectPtr<UStateTreeState> InState, const TSharedPtr<SScrollBox>& ViewBox, TSharedPtr<FStateTreeViewModel> InStateTreeViewModel)
{
	StateTreeViewModel = InStateTreeViewModel;
	WeakState = InState;
	const UStateTreeState* State = InState.Get();
	WeakTreeData = State != nullptr ? State->GetTypedOuter<UStateTreeEditorData>() : nullptr;

	ConstructInternal(STableRow::FArguments()
		.Padding(5.0f)
		.OnDragDetected(this, &SStateTreeViewRow::HandleDragDetected)
		.OnCanAcceptDrop(this, &SStateTreeViewRow::HandleCanAcceptDrop)
		.OnAcceptDrop(this, &SStateTreeViewRow::HandleAcceptDrop)
		.Style(&FStateTreeEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("StateTree.Selection"))
		, InOwnerTableView);

	static const FLinearColor LinkBackground = FLinearColor(FColor(84, 84, 84));
	static constexpr FLinearColor IconTint = FLinearColor(1, 1, 1, 0.5f);

	this->ChildSlot
	.HAlign(HAlign_Fill)
	[
		SNew(SBox)
		.MinDesiredWidth_Lambda([WeakOwnerViewBox = ViewBox.ToWeakPtr()]()
			{
				// Captured as weak ptr so we don't prevent our parent widget from being destroyed (circular pointer reference).
				if (const TSharedPtr<SScrollBox> OwnerViewBox = WeakOwnerViewBox.Pin())
				{
					// Make the row at least as wide as the view.
					// The -1 is needed or we'll see a scrollbar.
					return OwnerViewBox->GetTickSpaceGeometry().GetLocalSize().X - 1;
				}
				return 0.f;
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

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Left)
			.Padding(FMargin(0.0f, 4.0f))
			.AutoWidth()
			[
				SNew(SBox)
				.HeightOverride(28.0f)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FStateTreeEditorStyle::Get().GetBrush("StateTree.State.Border"))
					.BorderBackgroundColor(this, &SStateTreeViewRow::GetActiveStateColor)
					[
						SNew(SHorizontalBox)

						// Sub tree marker
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(4.0f)
							.HeightOverride(28.0f)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
								.BorderBackgroundColor(this, &SStateTreeViewRow::GetSubTreeMarkerColor)
							]
						]

						// State Box
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SBox)
							.HeightOverride(28.0f)
							.VAlign(VAlign_Fill)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
								.BorderBackgroundColor(this, &SStateTreeViewRow::GetTitleColor)
								.Padding(FMargin(4.0f, 0.0f, 12.0f, 0.0f))
								.IsEnabled_Lambda([InState]
									{
										const UStateTreeState* State = InState.Get();
										return State != nullptr && State->bEnabled;
									})
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(SHorizontalBox)
										// Conditions icon
										+SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										.AutoWidth()
										[
											SNew(SBox)
											.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
											.Visibility(this, &SStateTreeViewRow::GetConditionVisibility)
											[
												SNew(SImage)
												.ColorAndOpacity(IconTint)
												.Image(FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Conditions"))
												.ToolTipText(LOCTEXT("StateHasEnterConditions", "State selection is guarded with enter conditions."))
											]
										]

										// Selector icon
										+ SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										.AutoWidth()
										[
											SNew(SBox)
											.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
											[
												SNew(SImage)
												.Image(this, &SStateTreeViewRow::GetSelectorIcon)
												.ColorAndOpacity(IconTint)
												.ToolTipText(this, &SStateTreeViewRow::GetSelectorTooltip)
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
											.ToolTipText(this, &SStateTreeViewRow::GetStateTypeTooltip)
											.Clipping(EWidgetClipping::ClipToBounds)
											.IsSelected(this, &SStateTreeViewRow::IsStateSelected)
										]

										// State ID
										+ SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										.AutoWidth()
										[
											SNew(STextBlock)
											.Visibility_Lambda([]()
											{
												return UE::StateTree::Editor::GbDisplayItemIds ? EVisibility::Visible : EVisibility::Collapsed;
											})
											.Text(this, &SStateTreeViewRow::GetStateIDDesc)
											.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Details")
										]
									]
									+ SOverlay::Slot()
									[
										SNew(SHorizontalBox)

										// State breakpoint box
										+ SHorizontalBox::Slot()
										.VAlign(VAlign_Top)
										.HAlign(HAlign_Left)
										.AutoWidth()
										[
											SNew(SBox)
											.Padding(FMargin(-12.0f, -6.0f, 0.0f, 0.0f))
											[
												SNew(SImage)
												.DesiredSizeOverride(FVector2D(12.f, 12.f))
												.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
												.Visibility(this, &SStateTreeViewRow::GetStateBreakpointVisibility)
												.ToolTipText(this, &SStateTreeViewRow::GetStateBreakpointTooltipText)
											]
										]
									]
								]
							]
						]
						
						// Linked State box
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Fill)
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
								.Padding(FMargin(6.0f, 0.0f, 12.0f, 0.0f))
								[
									// Link icon
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.AutoWidth()
									.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
									[
										SNew(SImage)
										.ColorAndOpacity(IconTint)
										.Image(FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.StateLinked"))
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
						// Tasks
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Fill)
						.AutoWidth()
						[
							SNew(SBox)
							.VAlign(VAlign_Fill)
							.Visibility(this, &SStateTreeViewRow::GetTasksVisibility)
							[
								CreateTasksWidget()
							]
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
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(STextBlock)
						.Text(this, &SStateTreeViewRow::GetCompletedTransitionsIcon)
						.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
						.Visibility(this, &SStateTreeViewRow::GetCompletedTransitionVisibility)
					]
					+ SOverlay::Slot()
					[
						// Breakpoint box
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Top)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SBox)
							.Padding(FMargin(0.0f, -10.0f, 0.0f, 0.0f))
							[
								SNew(SImage)
								.DesiredSizeOverride(FVector2D(10.f, 10.f))
								.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
								.Visibility(this, &SStateTreeViewRow::GetCompletedTransitionBreakpointVisibility)
								.ToolTipText_Lambda([this]
									{
										return FText::Format(LOCTEXT("TransitionBreakpointTooltip","Break when executing transition: {0}"),
											GetCompletedTransitionWithBreakpointDesc());
									})
							]
						]
					]
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
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(STextBlock)
						.Text(this, &SStateTreeViewRow::GetSucceededTransitionIcon)
						.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
						.Visibility(this, &SStateTreeViewRow::GetSucceededTransitionVisibility)
					]
					+ SOverlay::Slot()
					[
						// Breakpoint box
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Top)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SBox)
							.Padding(FMargin(0.0f, -10.0f, 0.0f, 0.0f))
							[
								SNew(SImage)
								.DesiredSizeOverride(FVector2D(10.f, 10.f))
								.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
								.Visibility(this, &SStateTreeViewRow::GetSucceededTransitionBreakpointVisibility)
								.ToolTipText_Lambda([this]
									{
										return FText::Format(LOCTEXT("TransitionBreakpointTooltip", "Break when executing transition: {0}"),
											GetSucceededTransitionWithBreakpointDesc());
									})
							]
						]
					]
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
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(STextBlock)
						.Text(this, &SStateTreeViewRow::GetFailedTransitionIcon)
						.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
						.Visibility(this, &SStateTreeViewRow::GetFailedTransitionVisibility)
					]
					+ SOverlay::Slot()
					[
						// Breakpoint box
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Top)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SBox)
							.Padding(FMargin(0.0f, -10.0f, 0.0f, 0.0f))
							[
								SNew(SImage)
								.DesiredSizeOverride(FVector2D(10.f, 10.f))
								.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
								.Visibility(this, &SStateTreeViewRow::GetFailedTransitionBreakpointVisibility)
								.ToolTipText_Lambda([this]
									{
										return FText::Format(LOCTEXT("TransitionBreakpointTooltip", "Break when executing transition: {0}"),
											GetFailedTransitionWithBreakpointDesc());
									})
							]
						]
					]
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
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(STextBlock)
						.Text(FEditorFontGlyphs::Long_Arrow_Right)
						.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Icon")
						.Visibility(this, &SStateTreeViewRow::GetConditionalTransitionsVisibility)
					]
					+ SOverlay::Slot()
					[
						// Breakpoint box
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Top)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SBox)
							.Padding(FMargin(0.0f, -10.0f, 0.0f, 0.0f))
							[
								SNew(SImage)
								.DesiredSizeOverride(FVector2D(10.f, 10.f))
								.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
								.Visibility(this, &SStateTreeViewRow::GetConditionalTransitionsBreakpointVisibility)
								.ToolTipText_Lambda([this]
									{
										return FText::Format(LOCTEXT("TransitionBreakpointTooltip", "Break when executing transition: {0}"),
											GetConditionalTransitionsWithBreakpointDesc());
									})
							]
						]
					]
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

TSharedRef<SHorizontalBox> SStateTreeViewRow::CreateTasksWidget()
{
	const TSharedRef<SHorizontalBox> TasksBox = SNew(SHorizontalBox);
	
	const UStateTreeState* State = WeakState.Get();
	if (State == nullptr || State->Tasks.IsEmpty())
	{
		return TasksBox;
	}

	TWeakObjectPtr<UStateTreeEditorData> WeakEditorData = State->GetTypedOuter<UStateTreeEditorData>();

	for (int32 TaskIndex = 0; TaskIndex < State->Tasks.Num(); TaskIndex++)
	{
		if (const FStateTreeTaskBase* Task = State->Tasks[TaskIndex].Node.GetPtr<FStateTreeTaskBase>())
		{
			FGuid TaskId = State->Tasks[TaskIndex].ID;
			auto IsTaskEnabledFunc = [WeakState=WeakState, TaskIndex]
				{
					const UStateTreeState* State = WeakState.Get();
					if (State != nullptr && State->Tasks.IsValidIndex(TaskIndex))
					{
						if (const FStateTreeTaskBase* Task = State->Tasks[TaskIndex].Node.GetPtr<FStateTreeTaskBase>())
						{
							return (State->bEnabled && Task->bTaskEnabled);
						}
					}
					return true;
				};

			auto IsTaskBreakpointEnabledFunc = [WeakEditorData, TaskId]
				{
#if WITH_STATETREE_DEBUGGER
					const UStateTreeEditorData* EditorData = WeakEditorData.Get();
					if (EditorData != nullptr && EditorData->HasAnyBreakpoint(TaskId))
					{
						return EVisibility::Visible;
					}
#endif // WITH_STATETREE_DEBUGGER
					return EVisibility::Hidden;
				};
			
			auto GetTaskBreakpointTooltipFunc = [WeakEditorData, TaskId]
				{
#if WITH_STATETREE_DEBUGGER
					if (const UStateTreeEditorData* EditorData = WeakEditorData.Get())
					{
						const bool bHasBreakpointOnEnter = EditorData->HasBreakpoint(TaskId, EStateTreeBreakpointType::OnEnter);
						const bool bHasBreakpointOnExit = EditorData->HasBreakpoint(TaskId, EStateTreeBreakpointType::OnExit);
						if (bHasBreakpointOnEnter && bHasBreakpointOnExit)
						{
							return LOCTEXT("StateTreeTaskBreakpointOnEnterAndOnExitTooltip","Break when entering or exiting task");
						}

						if (bHasBreakpointOnEnter)
						{
							return LOCTEXT("StateTreeTaskBreakpointOnEnterTooltip","Break when entering task");
						}

						if (bHasBreakpointOnExit)
						{
							return LOCTEXT("StateTreeTaskBreakpointOnExitTooltip","Break when exiting task");
						}
					}
#endif // WITH_STATETREE_DEBUGGER
					return FText::GetEmpty();
				};

			FText TaskName;
			if (UE::StateTree::Editor::GbDisplayItemIds)
			{
				TaskName = FText::FromString(FString::Printf(TEXT("%s (%s)"), *Task->Name.ToString(), *LexToString(TaskId)));
			}
			else
			{
				TaskName = FText::FromName(Task->Name);
			}

			TasksBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.VAlign(VAlign_Center)
					.BorderImage(FStateTreeEditorStyle::Get().GetBrush("StateTree.Task.Rect"))
					.BorderBackgroundColor(this, &SStateTreeViewRow::GetTitleColor)
					.Padding(0)
					.IsEnabled_Lambda(IsTaskEnabledFunc)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(STextBlock)
							.Margin(FMargin(4.f, 0.f))
							.Text(TaskName)
							.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Task.Title")
							.IsEnabled_Lambda(IsTaskEnabledFunc)
							.ToolTipText(FText::FromName(Task->Name))
						]
						+ SOverlay::Slot()
						[
							// Task Breakpoint box
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Top)
							.HAlign(HAlign_Left)
							.AutoWidth()
							[
								SNew(SBox)
								.Padding(FMargin(0.0f, -10.0f, 0.0f, 0.0f))
								[
									SNew(SImage)
									.DesiredSizeOverride(FVector2D(10.f, 10.f))
									.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
									.Visibility_Lambda(IsTaskBreakpointEnabledFunc)
									.ToolTipText_Lambda(GetTaskBreakpointTooltipFunc)
								]
							]
						]
					]
				];
		}
	}

	return TasksBox;
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
	const UStateTreeState* State = WeakState.Get();
	const UStateTreeEditorData* EditorData = WeakTreeData.Get();

	if (State != nullptr && EditorData != nullptr)
	{
		if (const FStateTreeEditorColor* FoundColor = EditorData->FindColor(State->ColorRef))
		{
			if (IsRootState() || State->Type == EStateTreeStateType::Subtree)
			{
				return UE::StateTree::Editor::LerpColorSRGB(FoundColor->Color, FColor::Black, 0.25f);
			}
			return FoundColor->Color;
		}
	}

	return FLinearColor(FColor(31, 151, 167));
}

FSlateColor SStateTreeViewRow::GetActiveStateColor() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (StateTreeViewModel && StateTreeViewModel->IsStateActiveInDebugger(*State))
		{
			return FLinearColor::Yellow;
		}
		if (StateTreeViewModel && StateTreeViewModel->IsSelected(State))
		{
			return FLinearColor(FColor(236, 134, 39));
		}
	}

	return FLinearColor::Transparent;
}

FSlateColor SStateTreeViewRow::GetSubTreeMarkerColor() const
{
	// Show color for subtree.
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (IsRootState() || State->Type == EStateTreeStateType::Subtree)
		{
			const FSlateColor TitleColor = GetTitleColor();
			return UE::StateTree::Editor::LerpColorSRGB(TitleColor.GetSpecifiedColor(), FLinearColor::White, 0.2f);
		}
	}

	return GetTitleColor();
}

FText SStateTreeViewRow::GetStateDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return FText::FromName(State->Name);
	}
	return FText::FromName(FName());
}

FText SStateTreeViewRow::GetStateIDDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return FText::FromString(*LexToString(State->ID));
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

EVisibility SStateTreeViewRow::GetStateBreakpointVisibility() const
{
#if WITH_STATETREE_DEBUGGER
	const UStateTreeState* State = WeakState.Get();
	const UStateTreeEditorData* EditorData = WeakTreeData.Get();
	if (State != nullptr && EditorData != nullptr)
	{
		return (EditorData != nullptr && EditorData->HasAnyBreakpoint(State->ID)) ? EVisibility::Visible : EVisibility::Hidden;
	}
#endif // WITH_STATETREE_DEBUGGER
	return EVisibility::Hidden;
}

FText SStateTreeViewRow::GetStateBreakpointTooltipText() const
{
#if WITH_STATETREE_DEBUGGER
	const UStateTreeState* State = WeakState.Get();
	const UStateTreeEditorData* EditorData = WeakTreeData.Get();
	if (State != nullptr && EditorData != nullptr)
	{
		const bool bHasBreakpointOnEnter = EditorData->HasBreakpoint(State->ID, EStateTreeBreakpointType::OnEnter);
		const bool bHasBreakpointOnExit = EditorData->HasBreakpoint(State->ID, EStateTreeBreakpointType::OnExit);

		if (bHasBreakpointOnEnter && bHasBreakpointOnExit)
		{
			return LOCTEXT("StateTreeStateBreakpointOnEnterAndOnExitTooltip","Break when entering or exiting state");
		}

		if (bHasBreakpointOnEnter)
		{
			return LOCTEXT("StateTreeStateBreakpointOnEnterTooltip","Break when entering state");
		}

		if (bHasBreakpointOnExit)
		{
			return LOCTEXT("StateTreeStateBreakpointOnExitTooltip","Break when exiting state");
		}
	}
#endif // WITH_STATETREE_DEBUGGER
	return FText::GetEmpty();
}

const FSlateBrush* SStateTreeViewRow::GetSelectorIcon() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::None)
		{
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.SelectNone");
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TryEnterState)
		{
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryEnterState");			
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder)
		{
			if (State->Children.IsEmpty())
			{
				// Backwards compatible behavior
				return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryEnterState");			
			}
			else
			{
				return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TrySelectChildrenInOrder");
			}
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TryFollowTransitions)
		{
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryFollowTransitions");
		}
	}

	return nullptr;
}

FText SStateTreeViewRow::GetSelectorTooltip() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		const UEnum* Enum = StaticEnum<EStateTreeStateSelectionBehavior>();
		check(Enum);
		const int32 Index = Enum->GetIndexByValue((int64)State->SelectionBehavior);
		
		if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::None)
		{
			return Enum->GetToolTipTextByIndex(Index);
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TryEnterState)
		{
			return Enum->GetToolTipTextByIndex(Index);
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder)
		{
			if (State->Children.IsEmpty())
			{
				const int32 EnterStateIndex = Enum->GetIndexByValue((int64)EStateTreeStateSelectionBehavior::TryEnterState);
				return FText::Format(LOCTEXT("ConvertedToEnterState", "{0}\nAutomatically converted from '{1}' becase the State has no child States."),
					Enum->GetToolTipTextByIndex(EnterStateIndex), UEnum::GetDisplayValueAsText(State->SelectionBehavior));
			}
			else
			{
				return Enum->GetToolTipTextByIndex(Index);
			}
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TryFollowTransitions)
		{
			return Enum->GetToolTipTextByIndex(Index);
		}
	}

	return FText::GetEmpty();
}

FText SStateTreeViewRow::GetStateTypeTooltip() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		const UEnum* Enum = StaticEnum<EStateTreeStateType>();
		check(Enum);
		const int32 Index = Enum->GetIndexByValue((int64)State->Type);
		return Enum->GetToolTipTextByIndex(Index);
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
			if (State->Tasks[i].Node.GetPtr<FStateTreeTaskBase>())
			{
				ValidCount++;
			}
		}
		return ValidCount > 0 ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetLinkedStateVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return (State->Type == EStateTreeStateType::Linked || State->Type == EStateTreeStateType::LinkedAsset) ? EVisibility::Visible : EVisibility::Collapsed;
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
	else if (State->Type == EStateTreeStateType::LinkedAsset)
	{
		return FText::FromString(GetNameSafe(State->LinkedAsset.Get()));
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


FText SStateTreeViewRow::GetLinkDescription(const FStateTreeStateLink& Link)
{
	switch (Link.LinkType)
	{
	case EStateTreeTransitionType::None:
		return LOCTEXT("TransitionNoneStyled", "[None]");
		break;
	case EStateTreeTransitionType::Succeeded:
		return LOCTEXT("TransitionTreeSucceededStyled", "[Succeeded]");
		break;
	case EStateTreeTransitionType::Failed:
		return LOCTEXT("TransitionTreeFailedStyled", "[Failed]");
		break;
	case EStateTreeTransitionType::NextState:
		return LOCTEXT("TransitionNextStateStyled", "[Next]");
		break;
	case EStateTreeTransitionType::NextSelectableState:
		return LOCTEXT("TransitionNextSelectableStateStyled", "[Next Selectable]");
		break;
	case EStateTreeTransitionType::GotoState:
		return FText::FromName(Link.Name);
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled transition type."));
		break;
	}

	return FText::GetEmpty();
};

FText SStateTreeViewRow::GetTransitionsDesc(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger, const FTransitionDescFilterOptions FilterOptions) const
{
	TArray<FText> DescItems;
	const UStateTreeEditorData* TreeEditorData = WeakTreeData.Get();

	for (const FStateTreeTransition& Transition : State.Transitions)
	{
		// Apply filter for enabled/disabled transitions
		if ((FilterOptions.Enabled == ETransitionDescRequirement::RequiredTrue && Transition.bTransitionEnabled == false)
			|| (FilterOptions.Enabled == ETransitionDescRequirement::RequiredFalse && Transition.bTransitionEnabled))
		{
			continue;
		}

#if WITH_STATETREE_DEBUGGER
		// Apply filter for transitions with/without breakpoint
		const bool bHasBreakpoint = TreeEditorData != nullptr && TreeEditorData->HasBreakpoint(Transition.ID, EStateTreeBreakpointType::OnTransition);
		if ((FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredTrue && bHasBreakpoint == false)
			|| (FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredFalse && bHasBreakpoint))
		{
			continue;
		}
#endif // WITH_STATETREE_DEBUGGER

		const bool bMatch = FilterOptions.bUseMask ? EnumHasAnyFlags(Transition.Trigger, Trigger) : Transition.Trigger == Trigger;
		if (bMatch)
		{
			DescItems.Add(GetLinkDescription(Transition.State));
		}
	}

	// Find states from transition tasks
	if (EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent))
	{
		auto AddLinksFromStruct = [&DescItems](FStateTreeDataView Struct)
		{
			if (!Struct.IsValid())
			{
				return;
			}
			for (TPropertyValueIterator<FStructProperty> It(Struct.GetStruct(), Struct.GetMemory()); It; ++It)
			{
				const UScriptStruct* StructType = It.Key()->Struct;
				if (StructType == TBaseStructure<FStateTreeStateLink>::Get())
				{
					const FStateTreeStateLink& Link = *static_cast<const FStateTreeStateLink*>(It.Value());
					if (Link.LinkType != EStateTreeTransitionType::None)
					{
						DescItems.Add(GetLinkDescription(Link));
					}
				}
			}
		};
		
		for (const FStateTreeEditorNode& Task : State.Tasks)
		{
			AddLinksFromStruct(FStateTreeDataView(Task.Node.GetScriptStruct(), const_cast<uint8*>(Task.Node.GetMemory())));
			AddLinksFromStruct(Task.GetInstance());
		}

		AddLinksFromStruct(FStateTreeDataView(State.SingleTask.Node.GetScriptStruct(), const_cast<uint8*>(State.SingleTask.Node.GetMemory())));
		AddLinksFromStruct(State.SingleTask.GetInstance());
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
			DescItems.Add(LOCTEXT("TransitionActionRoot", "[Root]"));
		}
	}
	
	return FText::Join(FText::FromString(TEXT(", ")), DescItems);
}

FText SStateTreeViewRow::GetTransitionsIcon(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger, const FTransitionDescFilterOptions FilterOptions) const
{
	enum EIconType
	{
		IconNone = 0,
		IconRightArrow =	1 << 0,
		IconDownArrow =		1 << 1,
		IconLevelUp =		1 << 2,
	};
	uint8 IconType = IconNone;
	
	const UStateTreeEditorData* TreeEditorData = WeakTreeData.Get();
	
	for (const FStateTreeTransition& Transition : State.Transitions)
	{
		// Apply filter for enabled/disabled transitions
		if ((FilterOptions.Enabled == ETransitionDescRequirement::RequiredTrue && Transition.bTransitionEnabled == false)
			|| (FilterOptions.Enabled == ETransitionDescRequirement::RequiredFalse && Transition.bTransitionEnabled))
		{
			continue;
		}

#if WITH_STATETREE_DEBUGGER
		// Apply filter for transitions with/without breakpoint
		const bool bHasBreakpoint = TreeEditorData != nullptr && TreeEditorData->HasBreakpoint(Transition.ID, EStateTreeBreakpointType::OnTransition);
		if ((FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredTrue && bHasBreakpoint == false)
			|| (FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredFalse && bHasBreakpoint))
		{
			continue;
		}
#endif // WITH_STATETREE_DEBUGGER
		
		// The icons here depict "transition direction", not the type specifically.
		const bool bMatch = FilterOptions.bUseMask ? EnumHasAnyFlags(Transition.Trigger, Trigger) : Transition.Trigger == Trigger;
		if (bMatch)
		{
			switch (Transition.State.LinkType)
			{
			case EStateTreeTransitionType::None:
				IconType |= IconRightArrow;
				break;
			case EStateTreeTransitionType::Succeeded:
				IconType |= IconRightArrow;
				break;
			case EStateTreeTransitionType::Failed:
				IconType |= IconRightArrow;
				break;
			case EStateTreeTransitionType::NextState:
			case EStateTreeTransitionType::NextSelectableState:
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
		// Transition is handled on parent state, or implicit Root.
		IconType = IconLevelUp;
	}

	switch (IconType)
	{
		case IconRightArrow:
			return FEditorFontGlyphs::Long_Arrow_Right;
		case IconDownArrow:
			return FEditorFontGlyphs::Long_Arrow_Down;
		case IconLevelUp:
			return FEditorFontGlyphs::Level_Up;
		default:
			return FText::GetEmpty();
	}
}

EVisibility SStateTreeViewRow::GetTransitionsVisibility(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger) const
{
	// Handle completed, succeeded and failed transitions.
	if (EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnStateCompleted))
	{
		const bool bIsLeafState = (State.Children.Num() == 0);
		EStateTreeTransitionTrigger HandledTriggers = EStateTreeTransitionTrigger::None;
		bool bExactMatch = false;

		for (const FStateTreeTransition& Transition : State.Transitions)
		{
			// Skip disabled transitions
			if (Transition.bTransitionEnabled == false)
			{
				continue;
			}

			HandledTriggers |= Transition.Trigger;
			bExactMatch |= (Transition.Trigger == Trigger);

			if (bExactMatch)
			{
				break;
			}
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

	// Find states from transition tasks
	if (EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent))
	{
		auto HasAnyLinksInStruct = [](FStateTreeDataView Struct) -> bool
		{
			if (!Struct.IsValid())
			{
				return false;
			}
			for (TPropertyValueIterator<FStructProperty> It(Struct.GetStruct(), Struct.GetMemory()); It; ++It)
			{
				const UScriptStruct* StructType = It.Key()->Struct;
				if (StructType == TBaseStructure<FStateTreeStateLink>::Get())
				{
					const FStateTreeStateLink& Link = *static_cast<const FStateTreeStateLink*>(It.Value());
					if (Link.LinkType != EStateTreeTransitionType::None)
					{
						return true;
					}
				}
			}
			return false;
		};
		
		for (const FStateTreeEditorNode& Task : State.Tasks)
		{
			if (HasAnyLinksInStruct(FStateTreeDataView(Task.Node.GetScriptStruct(), const_cast<uint8*>(Task.Node.GetMemory())))
				|| HasAnyLinksInStruct(Task.GetInstance()))
			{
				return EVisibility::Visible;
			}
		}

		if (HasAnyLinksInStruct(FStateTreeDataView(State.SingleTask.Node.GetScriptStruct(), const_cast<uint8*>(State.SingleTask.Node.GetMemory())))
			|| HasAnyLinksInStruct(State.SingleTask.GetInstance()))
		{
			return EVisibility::Visible;
		}
	}
	
	// Handle the test
	for (const FStateTreeTransition& Transition : State.Transitions)
	{
		// Skip disabled transitions
		if (Transition.bTransitionEnabled == false)
		{
			continue;
		}

		if (EnumHasAnyFlags(Trigger, Transition.Trigger))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetTransitionsBreakpointVisibility(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger) const
{
#if WITH_STATETREE_DEBUGGER
	if (const UStateTreeEditorData* TreeEditorData = WeakTreeData.Get())
	{
		for (const FStateTreeTransition& Transition : State.Transitions)
		{
			if (Transition.bTransitionEnabled && EnumHasAnyFlags(Trigger, Transition.Trigger))
			{
				if (TreeEditorData->HasBreakpoint(Transition.ID, EStateTreeBreakpointType::OnTransition))
				{
					return GetTransitionsVisibility(State, Trigger);
				}
			}
		}
	}
#endif // WITH_STATETREE_DEBUGGER
	
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

EVisibility SStateTreeViewRow::GetCompletedTransitionBreakpointVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsBreakpointVisibility(*State, EStateTreeTransitionTrigger::OnStateCompleted);
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

FText SStateTreeViewRow::GetCompletedTransitionWithBreakpointDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.WithBreakpoint = ETransitionDescRequirement::RequiredTrue;
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateCompleted, FilterOptions);
	}
	return FText::GetEmpty();
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

EVisibility SStateTreeViewRow::GetSucceededTransitionBreakpointVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsBreakpointVisibility(*State, EStateTreeTransitionTrigger::OnStateSucceeded);
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

FText SStateTreeViewRow::GetSucceededTransitionWithBreakpointDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.WithBreakpoint = ETransitionDescRequirement::RequiredTrue;
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateSucceeded, FilterOptions);
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

EVisibility SStateTreeViewRow::GetFailedTransitionBreakpointVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsBreakpointVisibility(*State, EStateTreeTransitionTrigger::OnStateFailed);
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

FText SStateTreeViewRow::GetFailedTransitionWithBreakpointDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.WithBreakpoint = ETransitionDescRequirement::RequiredTrue;
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateFailed, FilterOptions);
	}
	return FText::GetEmpty();
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

EVisibility SStateTreeViewRow::GetConditionalTransitionsBreakpointVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsBreakpointVisibility(*State, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent);
	}
	return EVisibility::Collapsed;
}

FText SStateTreeViewRow::GetConditionalTransitionsDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.bUseMask = true;
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent, FilterOptions);
	}
	return FText::GetEmpty();
}

FText SStateTreeViewRow::GetConditionalTransitionsWithBreakpointDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.WithBreakpoint = ETransitionDescRequirement::RequiredTrue;
		FilterOptions.bUseMask = true;
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent, FilterOptions);
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

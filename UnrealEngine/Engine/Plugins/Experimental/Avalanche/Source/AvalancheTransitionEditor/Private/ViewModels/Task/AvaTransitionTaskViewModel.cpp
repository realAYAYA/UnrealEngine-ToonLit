// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionTaskViewModel.h"
#include "AvaTransitionNodeContext.h"
#include "AvaTransitionTreeEditorData.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorStyle.h"
#include "StateTreeState.h"
#include "Tasks/AvaTransitionTask.h"
#include "ViewModels/AvaTransitionViewModelSharedData.h"
#include "ViewModels/AvaTransitionViewModelUtils.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvaTransitionTaskViewModel"

FAvaTransitionTaskViewModel::FAvaTransitionTaskViewModel(const FStateTreeEditorNode& InEditorNode)
	: FAvaTransitionNodeViewModel(InEditorNode)
{
}

FText FAvaTransitionTaskViewModel::GetTaskDescription() const
{
	return TaskDescription;
}

void FAvaTransitionTaskViewModel::UpdateTaskDescription()
{
	TaskDescription = FText::GetEmpty();

	const FStateTreeEditorNode* EditorNode = GetEditorNode();
	if (!EditorNode)
	{
		return;
	}

	// Advanced Mode: Show Task Descriptions from the Node Name (Raw) if name not none
	if (GetSharedData()->GetEditorMode() == EAvaTransitionEditorMode::Advanced)
	{
		const FStateTreeNodeBase* Task = EditorNode->Node.GetPtr<FStateTreeNodeBase>();
		if (Task && Task->Name != NAME_None)
		{
			TaskDescription = FText::FromName(Task->Name);
			return;
		}
	}

	if (const FAvaTransitionTask* Task = EditorNode->Node.GetPtr<FAvaTransitionTask>())
	{
		TaskDescription = Task->GenerateDescription(FAvaTransitionNodeContext(EditorNode->GetInstance()));
	}

	const UScriptStruct* Struct = EditorNode->Node.GetScriptStruct();
	if (Struct && TaskDescription.IsEmpty())
	{
		TaskDescription = Struct->GetDisplayNameText();
	}
}

bool FAvaTransitionTaskViewModel::IsEnabled() const
{
	const FStateTreeTaskBase* Task = GetNodeOfType<FStateTreeTaskBase>();
	return Task && Task->bTaskEnabled;
}

EVisibility FAvaTransitionTaskViewModel::GetBreakpointVisibility() const
{
#if WITH_STATETREE_DEBUGGER
	const UAvaTransitionTreeEditorData* EditorData = GetEditorData();
	if (EditorData && EditorData->HasAnyBreakpoint(GetNodeId()))
	{
		return EVisibility::Visible;
	}
#endif
	return EVisibility::Hidden;
}

FText FAvaTransitionTaskViewModel::GetBreakpointTooltip() const
{
#if WITH_STATETREE_DEBUGGER
	if (const UAvaTransitionTreeEditorData* EditorData = GetEditorData())
	{
		const bool bHasBreakpointOnEnter = EditorData->HasBreakpoint(GetNodeId(), EStateTreeBreakpointType::OnEnter);
		const bool bHasBreakpointOnExit  = EditorData->HasBreakpoint(GetNodeId(), EStateTreeBreakpointType::OnExit);
		if (bHasBreakpointOnEnter && bHasBreakpointOnExit)
		{
			return LOCTEXT("BreakpointOnEnterAndOnExitTooltip","Break when entering or exiting task");
		}

		if (bHasBreakpointOnEnter)
		{
			return LOCTEXT("BreakpointOnEnterTooltip","Break when entering task");
		}

		if (bHasBreakpointOnExit)
		{
			return LOCTEXT("BreakpointOnExitTooltip","Break when exiting task");
		}
	}
#endif
	return FText::GetEmpty();
}

TArrayView<FStateTreeEditorNode> FAvaTransitionTaskViewModel::GetNodes(UStateTreeState& InState) const
{
	if (InState.Tasks.IsEmpty())
	{
		return MakeArrayView(&InState.SingleTask, 1);
	}
	return InState.Tasks;
}

TSharedRef<SWidget> FAvaTransitionTaskViewModel::CreateWidget()
{
	UpdateTaskDescription();

	return SNew(SBorder)
		.VAlign(VAlign_Fill)
		.Padding(0)
		.IsEnabled(this, &FAvaTransitionTaskViewModel::IsEnabled)
		.BorderImage(FStateTreeEditorStyle::Get().GetBrush("StateTree.Task.Rect"))
		.BorderBackgroundColor(this, &FAvaTransitionTaskViewModel::GetStateColor)
		[
			SNew(SOverlay)
			// Task Description
			+ SOverlay::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Margin(FMargin(4.f, 0.f))
				.Text(this, &FAvaTransitionTaskViewModel::GetTaskDescription)
				.ToolTipText(this, &FAvaTransitionTaskViewModel::GetTaskDescription)
				.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Task.Title")
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
			// Task Breakpoint
			+ SOverlay::Slot()
			[
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
						.Image(FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid"))
						.Visibility(this, &FAvaTransitionTaskViewModel::GetBreakpointVisibility)
						.ToolTipText(this, &FAvaTransitionTaskViewModel::GetBreakpointTooltip)
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE

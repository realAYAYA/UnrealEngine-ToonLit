// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionConditionViewModel.h"
#include "StateTreeState.h"

FAvaTransitionConditionViewModel::FAvaTransitionConditionViewModel(const FStateTreeEditorNode& InEditorNode)
	: FAvaTransitionNodeViewModel(InEditorNode)
{
}

TArrayView<FStateTreeEditorNode> FAvaTransitionConditionViewModel::GetNodes(UStateTreeState& InState) const
{
	return InState.EnterConditions;
}

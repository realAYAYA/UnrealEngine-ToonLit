// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionTreeEditorData.h"

UStateTreeState& UAvaTransitionTreeEditorData::CreateState(const UStateTreeState& InSiblingState, bool bInAfter)
{
	UObject* Outer = this;
	if (InSiblingState.Parent)
	{
		Outer = InSiblingState.Parent;
	}

	check(Outer);
	UStateTreeState* State = NewObject<UStateTreeState>(Outer, NAME_None, RF_Transactional);
	check(State);

	State->Parent = InSiblingState.Parent;

	TArray<TObjectPtr<UStateTreeState>>& Children = State->Parent
		? State->Parent->Children
		: SubTrees;

	int32 ChildIndex = Children.IndexOfByKey(&InSiblingState);
	ChildIndex = FMath::Clamp(ChildIndex, 0, Children.Num() - 1);

	if (bInAfter)
	{
		++ChildIndex;
	}

	Children.Insert(State, ChildIndex);
	return *State;
}

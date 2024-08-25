// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionContainerViewModel.h"
#include "StateTreeState.h"

FAvaTransitionContainerViewModel::FAvaTransitionContainerViewModel(UStateTreeState* InState)
	: StateWeak(InState)
{
}

UStateTreeState* FAvaTransitionContainerViewModel::GetState() const
{
	return StateWeak.Get();
}

bool FAvaTransitionContainerViewModel::IsValid() const
{
	return StateWeak.IsValid();
}

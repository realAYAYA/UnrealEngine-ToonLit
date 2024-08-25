// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeExecutionTypes.h"

class FAvaTransitionNodeContext;
class UAvaTransitionSubsystem;
struct FAvaTransitionContext;
struct FStateTreeLinker;

class FAvaTransitionNode
{
public:
	AVALANCHETRANSITION_API bool LinkNode(FStateTreeLinker& InLinker);

	/** Generates a more user-friendly dynamic description of the Node based on its settings */
	virtual FText GenerateDescription(const FAvaTransitionNodeContext& InContext) const
	{
		return FText::GetEmpty();
	}

	TStateTreeExternalDataHandle<FAvaTransitionContext> TransitionContextHandle;

	TStateTreeExternalDataHandle<UAvaTransitionSubsystem> TransitionSubsystemHandle;
};

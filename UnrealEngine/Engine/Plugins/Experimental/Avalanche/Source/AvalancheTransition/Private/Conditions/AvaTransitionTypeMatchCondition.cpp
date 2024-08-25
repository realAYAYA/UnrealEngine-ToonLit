// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/AvaTransitionTypeMatchCondition.h"
#include "AvaTransitionContext.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaTransitionTypeMatchCondition"

FText FAvaTransitionTypeMatchCondition::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	return FText::Format(LOCTEXT("ConditionDescription", "transitioning {0}")
		, UEnum::GetDisplayValueAsText(TransitionType).ToLower());
}

bool FAvaTransitionTypeMatchCondition::TestCondition(FStateTreeExecutionContext& InContext) const
{
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);
	return TransitionContext.GetTransitionType() == TransitionType;
}

#undef LOCTEXT_NAMESPACE

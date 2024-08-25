// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/AvaTransitionStateMatchCondition.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"

#define LOCTEXT_NAMESPACE "AvaTransitionSceneMatchCondition"

FText FAvaTransitionStateMatchCondition::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	return FText::Format(LOCTEXT("ConditionDescription", "{0} scene in {1}")
		, UEnum::GetDisplayValueAsText(TransitionState).ToLower()
		, GetLayerQueryText());
}

bool FAvaTransitionStateMatchCondition::TestCondition(FStateTreeExecutionContext& InContext) const
{
	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances = QueryBehaviorInstances(InContext);

	bool bIsLayerRunning = BehaviorInstances.ContainsByPredicate(
		[](const FAvaTransitionBehaviorInstance* InInstance)
		{
			check(InInstance);
			return InInstance->IsRunning();
		});

	switch (TransitionState)
	{
	case EAvaTransitionRunState::Running:
		return bIsLayerRunning;

	case EAvaTransitionRunState::Finished:
		return !bIsLayerRunning;
	}

	checkNoEntry();
	return false;
}

#undef LOCTEXT_NAMESPACE

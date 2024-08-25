// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/AvaTransitionLayerMatchCondition.h"
#include "AvaTransitionContext.h"
#include "StateTreeExecutionContext.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"

#define LOCTEXT_NAMESPACE "AvaTransitionLayerMatchCondition"

FText FAvaTransitionLayerMatchCondition::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	return FText::Format(LOCTEXT("ConditionDescription", "scenes transitioning in {0}"), GetLayerQueryText());
}

bool FAvaTransitionLayerMatchCondition::TestCondition(FStateTreeExecutionContext& InContext) const
{
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);

	if (LayerType == EAvaTransitionLayerCompareType::Same && TransitionContext.GetTransitionType() == EAvaTransitionType::In)
	{
		return true;
	}

	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances = QueryBehaviorInstances(InContext);
	return BehaviorInstances.ContainsByPredicate([](const FAvaTransitionBehaviorInstance* InInstance)
		{
			return InInstance && InInstance->GetTransitionType() == EAvaTransitionType::In;
		});
}

#undef LOCTEXT_NAMESPACE

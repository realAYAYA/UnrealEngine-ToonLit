// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/AvaTransitionSceneMatchCondition.h"
#include "AvaTransitionContext.h"
#include "AvaTransitionScene.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaTransitionSceneMatchCondition"

FText FAvaTransitionSceneMatchCondition::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	return FText::Format(LOCTEXT("ConditionDescription", "{0} scene in {1}")
		, UEnum::GetDisplayValueAsText(SceneComparisonType).ToLower()
		, GetLayerQueryText());
}

bool FAvaTransitionSceneMatchCondition::TestCondition(FStateTreeExecutionContext& InContext) const
{
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);

	const FAvaTransitionScene* TransitionScene = TransitionContext.GetTransitionScene();
	if (!TransitionScene)
	{
		return false;
	}

	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances = QueryBehaviorInstances(InContext);
	if (BehaviorInstances.IsEmpty())
	{
		return false;
	}

	bool bHasMatchingInstance = BehaviorInstances.ContainsByPredicate(
		[TransitionScene, this](const FAvaTransitionBehaviorInstance* InInstance)
		{
			const FAvaTransitionScene* OtherTransitionScene = InInstance->GetTransitionContext().GetTransitionScene();

			const EAvaTransitionComparisonResult ComparisonResult = OtherTransitionScene
				? TransitionScene->Compare(*OtherTransitionScene)
				: EAvaTransitionComparisonResult::None;

			return ComparisonResult == SceneComparisonType;
		});

	return bHasMatchingInstance;
}

#undef LOCTEXT_NAMESPACE

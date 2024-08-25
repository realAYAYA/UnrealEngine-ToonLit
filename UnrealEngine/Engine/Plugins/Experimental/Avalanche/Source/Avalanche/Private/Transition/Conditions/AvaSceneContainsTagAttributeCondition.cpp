// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/Conditions/AvaSceneContainsTagAttributeCondition.h"
#include "AvaSceneState.h"
#include "AvaSceneSubsystem.h"
#include "AvaTransitionLayer.h"
#include "AvaTransitionLayerUtils.h"
#include "AvaTransitionSubsystem.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"
#include "IAvaSceneInterface.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

#define LOCTEXT_NAMESPACE "AvaSceneContainsTagAttributeConditionBase"

FText FAvaSceneContainsTagAttributeConditionBase::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	FFormatNamedArguments Arguments;

	switch (SceneType)
	{
	case EAvaTransitionSceneType::This:
		Arguments.Add(TEXT("IndefinitePronoun"), FText::GetEmpty());

		Arguments.Add(TEXT("Scene"), LOCTEXT("ThisScene", "this scene"));

		Arguments.Add(TEXT("Contains"), bInvertCondition ? LOCTEXT("ThisDoesntContain", "does not contain") : LOCTEXT("ThisContains", "contains"));
		break;

	case EAvaTransitionSceneType::Other:
		Arguments.Add(TEXT("IndefinitePronoun"), bInvertCondition ? LOCTEXT("NoScene", "no ") : LOCTEXT("AnyScene", "a "));

		Arguments.Add(TEXT("Scene"), FText::Format(LOCTEXT("OtherScene", "scene in {0}"), FAvaTransitionLayerUtils::GetLayerQueryText(LayerType, *SpecificLayers.ToString())));

		Arguments.Add(TEXT("Contains"), LOCTEXT("OtherSceneContains", "contains"));
		break;
	}

	Arguments.Add(TEXT("TagAttribute"), FText::FromName(TagAttribute.ToName()));

	return FText::Format(LOCTEXT("ConditionDescription", "{IndefinitePronoun}{Scene} {Contains} tag attribute '{TagAttribute}'"), Arguments);
}

bool FAvaSceneContainsTagAttributeConditionBase::Link(FStateTreeLinker& InLinker)
{
	FAvaTransitionCondition::Link(InLinker);
	InLinker.LinkExternalData(SceneSubsystemHandle);
	return true;
}

bool FAvaSceneContainsTagAttributeConditionBase::TestCondition(FStateTreeExecutionContext& InContext) const
{
	return ContainsTagAttribute(InContext) ^ bInvertCondition;
}

bool FAvaSceneContainsTagAttributeConditionBase::ContainsTagAttribute(FStateTreeExecutionContext& InContext) const
{
	TArray<const FAvaTransitionScene*> TransitionScenes = GetTransitionScenes(InContext);
	if (TransitionScenes.IsEmpty())
	{
		return false;
	}

	const UAvaSceneSubsystem& SceneSubsystem = InContext.GetExternalData(SceneSubsystemHandle);

	for (const FAvaTransitionScene* TransitionScene : TransitionScenes)
	{
		if (!TransitionScene)
		{
			continue;
		}

		const IAvaSceneInterface* Scene = SceneSubsystem.GetSceneInterface(TransitionScene->GetLevel());
		if (!Scene)
		{
			continue;
		}

		const UAvaSceneState* SceneState = Scene->GetSceneState();
		if (SceneState && SceneState->ContainsTagAttribute(TagAttribute))
		{
			return true;
		}
	}

	return false;
}

TArray<const FAvaTransitionScene*> FAvaSceneContainsTagAttributeConditionBase::GetTransitionScenes(FStateTreeExecutionContext& InContext) const
{
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);

	if (SceneType == EAvaTransitionSceneType::This)
	{
		return { TransitionContext.GetTransitionScene() };
	}

	ensureMsgf(SceneType == EAvaTransitionSceneType::Other
		, TEXT("FAvaSceneContainsAttributeCondition::GetTargetSceneContexts did not recognize the provided transition scene type")
		, *UEnum::GetValueAsString(SceneType));

	// Get all the Behavior Instances from Query 
	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances;
	{
		UAvaTransitionSubsystem& TransitionSubsystem = InContext.GetExternalData(TransitionSubsystemHandle);

		FAvaTransitionLayerComparator Comparator = FAvaTransitionLayerUtils::BuildComparator(TransitionContext, LayerType, SpecificLayers);

		BehaviorInstances = FAvaTransitionLayerUtils::QueryBehaviorInstances(TransitionSubsystem, Comparator);
	}

	TArray<const FAvaTransitionScene*> TransitionScenes;
	TransitionScenes.Reserve(BehaviorInstances.Num());

	for (const FAvaTransitionBehaviorInstance* BehaviorInstance : BehaviorInstances)
	{
		const FAvaTransitionContext& OtherTransitionContext = BehaviorInstance->GetTransitionContext();
		const FAvaTransitionScene* TransitionScene = OtherTransitionContext.GetTransitionScene();

		// do not add if scene is marked as needing discard
		if (TransitionScene && !TransitionScene->HasAllFlags(EAvaTransitionSceneFlags::NeedsDiscard))
		{
			TransitionScenes.AddUnique(TransitionScene);
		}
	}

	return TransitionScenes;
}

#undef LOCTEXT_NAMESPACE

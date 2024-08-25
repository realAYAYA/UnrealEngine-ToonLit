// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/Conditions/AvaTransitionRCControllerMatchCondition.h"
#include "AvaSceneSubsystem.h"
#include "AvaTransitionContext.h"
#include "AvaTransitionLayer.h"
#include "AvaTransitionLayerUtils.h"
#include "AvaTransitionLog.h"
#include "AvaTransitionScene.h"
#include "AvaTransitionSubsystem.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"
#include "IAvaSceneInterface.h"
#include "RCVirtualProperty.h"
#include "RemoteControlPreset.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Transition/Extensions/IAvaTransitionRCExtension.h"

#define LOCTEXT_NAMESPACE "AvaTransitionRCControllerMatchCondition"

FText FAvaTransitionRCControllerMatchCondition::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	return FText::Format(LOCTEXT("ConditionDescription", "'{0}' is {1}")
		, ControllerId.ToText()
		, UEnum::GetDisplayValueAsText(ValueComparisonType).ToLower());
}

bool FAvaTransitionRCControllerMatchCondition::Link(FStateTreeLinker& InLinker)
{
	FAvaTransitionCondition::Link(InLinker);
	InLinker.LinkExternalData(SceneSubsystemHandle);
	return true;
}

bool FAvaTransitionRCControllerMatchCondition::TestCondition(FStateTreeExecutionContext& InContext) const
{
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);
	UAvaTransitionSubsystem& TransitionSubsystem   = InContext.GetExternalData(TransitionSubsystemHandle);
	const UAvaSceneSubsystem& SceneSubsystem       = InContext.GetExternalData(SceneSubsystemHandle);
	const FAvaTransitionScene* TransitionScene     = TransitionContext.GetTransitionScene();

	URCVirtualPropertyBase* Controller = GetController(SceneSubsystem, TransitionScene);
	if (!Controller)
	{
		return false;
	}

	// Get all the Behavior Instances in the same Layer
	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances;
	{
		FAvaTransitionLayerComparator Comparator = FAvaTransitionLayerUtils::BuildComparator(TransitionContext, EAvaTransitionLayerCompareType::Same, FAvaTagHandle());
		BehaviorInstances = FAvaTransitionLayerUtils::QueryBehaviorInstances(TransitionSubsystem, Comparator);
	}

	if (BehaviorInstances.IsEmpty())
	{
		return false;
	}

	// Optional Extension to override Controller Comparison
	IAvaRCTransitionExtension* const RCTransitionExtension = TransitionScene->FindExtension<IAvaRCTransitionExtension>();

	for (const FAvaTransitionBehaviorInstance* BehaviorInstance : BehaviorInstances)
	{
		check(BehaviorInstance);

		const FAvaTransitionScene* OtherTransitionScene = BehaviorInstance->GetTransitionContext().GetTransitionScene();
		if (!OtherTransitionScene)
		{
			continue;
		}

		EAvaTransitionComparisonResult Result;
		if (RCTransitionExtension)
		{
			Result = RCTransitionExtension->CompareControllers(Controller->Id
				, *TransitionScene
				, *OtherTransitionScene);
		}
		else if (URCVirtualPropertyBase* OtherController = GetController(SceneSubsystem, OtherTransitionScene))
		{
			Result = Controller->IsValueEqual(OtherController)
				? EAvaTransitionComparisonResult::Same
				: EAvaTransitionComparisonResult::Different;
		}
		else
		{
			Result = EAvaTransitionComparisonResult::None;
		}

		if (ValueComparisonType == Result)
		{
			return true;
		}
	}

	return false;
}

URCVirtualPropertyBase* FAvaTransitionRCControllerMatchCondition::GetController(const UAvaSceneSubsystem& InSceneSubsystem, const FAvaTransitionScene* InTransitionScene) const
{
	if (!InTransitionScene)
	{
		return nullptr;
	}

	IAvaSceneInterface* SceneInterface = InSceneSubsystem.GetSceneInterface(InTransitionScene->GetLevel());
	if (!SceneInterface)
	{
		return nullptr;
	}

	if (URemoteControlPreset* RemoteControlPreset = SceneInterface->GetRemoteControlPreset())
	{
		return ControllerId.FindController(RemoteControlPreset);
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE

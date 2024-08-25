// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/AvaTransitionLayerTask.h"
#include "AvaTransitionContext.h"
#include "AvaTransitionLayerUtils.h"
#include "AvaTransitionSubsystem.h"
#include "StateTreeExecutionContext.h"

TArray<const FAvaTransitionBehaviorInstance*> FAvaTransitionLayerTask::QueryBehaviorInstances(const FStateTreeExecutionContext& InContext) const
{
	UAvaTransitionSubsystem& TransitionSubsystem   = InContext.GetExternalData(TransitionSubsystemHandle);
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);
	const FAvaTransitionLayerComparator Comparator = FAvaTransitionLayerUtils::BuildComparator(TransitionContext, LayerType, SpecificLayer);

	return FAvaTransitionLayerUtils::QueryBehaviorInstances(TransitionSubsystem, Comparator);
}

FText FAvaTransitionLayerTask::GetLayerQueryText() const
{
	return FAvaTransitionLayerUtils::GetLayerQueryText(LayerType, SpecificLayer.ToName());
}

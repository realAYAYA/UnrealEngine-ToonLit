// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionLayerUtils.h"
#include "AvaTag.h"
#include "AvaTransitionContext.h"
#include "AvaTransitionSubsystem.h"
#include "Containers/Array.h"
#include "Execution/IAvaTransitionExecutor.h"

#define LOCTEXT_NAMESPACE "AvaTransitionLayerUtils"

TArray<const FAvaTransitionBehaviorInstance*> FAvaTransitionLayerUtils::QueryBehaviorInstances(UAvaTransitionSubsystem& InTransitionSubsystem, const FAvaTransitionLayerComparator& InComparator)
{
	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances;

	// Get all the Behavior Instances that match the Layer Query
	InTransitionSubsystem.ForEachTransitionExecutor(
		[&BehaviorInstances, &InComparator](IAvaTransitionExecutor& InExecutor)->EAvaTransitionIterationResult
		{
			BehaviorInstances.Append(InExecutor.GetBehaviorInstances(InComparator));
			return EAvaTransitionIterationResult::Continue;
		});

	return BehaviorInstances;
}

FAvaTransitionLayerComparator FAvaTransitionLayerUtils::BuildComparator(const FAvaTransitionContext& InTransitionContext, EAvaTransitionLayerCompareType InCompareType, const FAvaTagHandle& InSpecificLayer)
{
	return BuildComparator(InTransitionContext, InCompareType, FAvaTagHandleContainer(InSpecificLayer));
}

FAvaTransitionLayerComparator FAvaTransitionLayerUtils::BuildComparator(const FAvaTransitionContext& InTransitionContext, EAvaTransitionLayerCompareType InCompareType, const FAvaTagHandleContainer& InSpecificLayers)
{
	FAvaTransitionLayerComparator LayerComparator;
	LayerComparator.LayerCompareType = InCompareType;
	LayerComparator.ExcludedContext  = &InTransitionContext;

	if (InCompareType == EAvaTransitionLayerCompareType::MatchingTag)
	{
		LayerComparator.LayerContext = InSpecificLayers;
	}
	else
	{
		LayerComparator.LayerContext = FAvaTagHandleContainer(InTransitionContext.GetTransitionLayer());
	}

	return LayerComparator;
}

FText FAvaTransitionLayerUtils::GetLayerQueryText(EAvaTransitionLayerCompareType InLayerType, FName InSpecificLayerName)
{
	if (InLayerType == EAvaTransitionLayerCompareType::MatchingTag)
	{
		return FText::Format(LOCTEXT("LayerTagText", "'{0}' layer"), FText::FromName(InSpecificLayerName));
	}
	return FText::Format(LOCTEXT("LayerTypeText", "{0} layer"), UEnum::GetDisplayValueAsText(InLayerType).ToLower());
}

#undef LOCTEXT_NAMESPACE

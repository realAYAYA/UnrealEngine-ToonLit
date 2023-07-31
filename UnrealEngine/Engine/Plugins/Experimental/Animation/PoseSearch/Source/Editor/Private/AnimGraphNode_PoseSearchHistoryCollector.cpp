// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_PoseSearchHistoryCollector.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_PoseSearchHistoryCollector"

FLinearColor UAnimGraphNode_PoseSearchHistoryCollector::GetNodeTitleColor() const
{
	return FColor(86, 182, 194);
}

FText UAnimGraphNode_PoseSearchHistoryCollector::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Use this pose for matching");
}

FText UAnimGraphNode_PoseSearchHistoryCollector::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Pose History");
}

FText UAnimGraphNode_PoseSearchHistoryCollector::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Pose Search");
}

#undef LOCTEXT_NAMESPACE

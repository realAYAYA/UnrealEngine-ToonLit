// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_PoseSearchHistoryCollector.h"
#include "AnimationGraphSchema.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_PoseSearchHistoryCollector"

/////////////////////////////////////////////////////
// UAnimGraphNode_PoseSearchHistoryCollector_Base

FLinearColor UAnimGraphNode_PoseSearchHistoryCollector_Base::GetNodeTitleColor() const
{
	return FColor(86, 182, 194);
}

FText UAnimGraphNode_PoseSearchHistoryCollector_Base::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Collects bones transforms for motion matching");
}

FText UAnimGraphNode_PoseSearchHistoryCollector_Base::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Pose Search");
}

/////////////////////////////////////////////////////
// UAnimGraphNode_PoseSearchHistoryCollector

FText UAnimGraphNode_PoseSearchHistoryCollector::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Pose History");
}

/////////////////////////////////////////////////////
// UAnimGraphNode_PoseSearchComponentSpaceHistoryCollector

FText UAnimGraphNode_PoseSearchComponentSpaceHistoryCollector::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitleComponentSpace", "Component Space Pose History");
}

void UAnimGraphNode_PoseSearchComponentSpaceHistoryCollector::CreateOutputPins()
{
	CreatePin(EGPD_Output, UAnimationGraphSchema::PC_Struct, FComponentSpacePoseLink::StaticStruct(), TEXT("Pose"));
}

#undef LOCTEXT_NAMESPACE

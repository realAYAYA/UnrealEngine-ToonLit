// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_CorrectPose.h"

#define LOCTEXT_NAMESPACE "CorrectPose"

//////////////////////////////////////////////////////////////////////////

UAnimGraphNode_CorrectPose::UAnimGraphNode_CorrectPose(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

// Node Title
FText UAnimGraphNode_CorrectPose::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "RBF Pose Corrective");
}

// Node Tooltip
FText UAnimGraphNode_CorrectPose::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Applies rbf pose corrective");
}

FText UAnimGraphNode_CorrectPose::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Pose Correctives");
}


#undef LOCTEXT_NAMESPACE

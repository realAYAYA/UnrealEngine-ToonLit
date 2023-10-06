// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_DeadBlending.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_DeadBlending"


FLinearColor UAnimGraphNode_DeadBlending::GetNodeTitleColor() const
{
	return FLinearColor(0.0f, 0.1f, 0.2f);
}

FText UAnimGraphNode_DeadBlending::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Performs inertialization using the Dead Blending algorithm.");
}

FText UAnimGraphNode_DeadBlending::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Dead Blending");
}

FText UAnimGraphNode_DeadBlending::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Animation|Misc.");
}

void UAnimGraphNode_DeadBlending::GetInputLinkAttributes(FNodeAttributeArray& OutAttributes) const
{
	OutAttributes.Add(UE::Anim::IInertializationRequester::Attribute);
}

#undef LOCTEXT_NAMESPACE

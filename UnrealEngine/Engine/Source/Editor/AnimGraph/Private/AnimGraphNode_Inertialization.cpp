// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_Inertialization.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_Inertialization"


FLinearColor UAnimGraphNode_Inertialization::GetNodeTitleColor() const
{
	return FLinearColor(0.0f, 0.1f, 0.2f);
}

FText UAnimGraphNode_Inertialization::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Inertialization");
}

FText UAnimGraphNode_Inertialization::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Inertialization");
}

FText UAnimGraphNode_Inertialization::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Animation|Misc.");
}

void UAnimGraphNode_Inertialization::GetInputLinkAttributes(FNodeAttributeArray& OutAttributes) const
{
	OutAttributes.Add(UE::Anim::IInertializationRequester::Attribute);
}

#undef LOCTEXT_NAMESPACE

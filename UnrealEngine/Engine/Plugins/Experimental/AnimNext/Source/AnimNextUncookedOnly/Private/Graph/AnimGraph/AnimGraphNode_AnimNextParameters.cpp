// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimGraph/AnimGraphNode_AnimNextParameters.h"
#include "Graph/AnimGraph/AnimBlueprintExtension_AnimNextParameters.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_AnimNextParameters"

FText UAnimGraphNode_AnimNextParameters::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "AnimNext Parameter Block");
}

FText UAnimGraphNode_AnimNextParameters::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Pushes parameter blocks that can be used by all nodes leaf-wards");
}

FText UAnimGraphNode_AnimNextParameters::GetMenuCategory() const
{
	return LOCTEXT("Category", "AnimNext");
}

void UAnimGraphNode_AnimNextParameters::GetRequiredExtensions(TArray<TSubclassOf<UAnimBlueprintExtension>>& OutExtensions) const
{
	OutExtensions.Add(UAnimBlueprintExtension_AnimNextParameters::StaticClass());
}

#undef LOCTEXT_NAMESPACE
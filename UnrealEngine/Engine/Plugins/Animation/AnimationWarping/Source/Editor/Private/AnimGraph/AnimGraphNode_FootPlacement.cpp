// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraph/AnimGraphNode_FootPlacement.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "DetailLayoutBuilder.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_FootPlacement

#define LOCTEXT_NAMESPACE "AnimGraphNode_FootPlacement"

UAnimGraphNode_FootPlacement::UAnimGraphNode_FootPlacement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_FootPlacement::GetControllerDescription() const
{
	return LOCTEXT("FootPlacement", "Foot Placement");
}

FText UAnimGraphNode_FootPlacement::GetTooltipText() const
{
	return LOCTEXT("FootPlacementTooltip", "Foot Placement.");
}

FLinearColor UAnimGraphNode_FootPlacement::GetNodeTitleColor() const
{
	return FLinearColor(FColor(153.f, 0.f, 0.f));
}

FText UAnimGraphNode_FootPlacement::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

#undef LOCTEXT_NAMESPACE

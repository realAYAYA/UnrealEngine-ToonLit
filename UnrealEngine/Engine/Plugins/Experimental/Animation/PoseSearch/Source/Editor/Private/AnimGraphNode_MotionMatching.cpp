// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_MotionMatching.h"
#include "Animation/AnimRootMotionProvider.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_MotionMatching"


FLinearColor UAnimGraphNode_MotionMatching::GetNodeTitleColor() const
{
	return FColor(86, 182, 194);
}

FText UAnimGraphNode_MotionMatching::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Motion Matching");
}

FText UAnimGraphNode_MotionMatching::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Motion Matching");
}

FText UAnimGraphNode_MotionMatching::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Pose Search");
}

void UAnimGraphNode_MotionMatching::GetOutputLinkAttributes(FNodeAttributeArray& OutAttributes) const
{
	if (UE::Anim::IAnimRootMotionProvider::Get())
	{
		OutAttributes.Add(UE::Anim::IAnimRootMotionProvider::AttributeName);
	}
}

void UAnimGraphNode_MotionMatching::BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	AnimBlueprint->FindOrAddGroup(Node.GetGroupName());
}

bool UAnimGraphNode_MotionMatching::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset* UAnimGraphNode_MotionMatching::GetAnimationAsset() const
{
	return nullptr;
}

const TCHAR* UAnimGraphNode_MotionMatching::GetTimePropertyName() const
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_MotionMatching::GetTimePropertyStruct() const
{
	return FAnimNode_MotionMatching::StaticStruct();
}

#undef LOCTEXT_NAMESPACE

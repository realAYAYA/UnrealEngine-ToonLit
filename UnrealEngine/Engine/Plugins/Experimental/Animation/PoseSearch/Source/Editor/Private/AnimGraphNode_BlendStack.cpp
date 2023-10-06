// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_BlendStack.h"
#include "Animation/AnimRootMotionProvider.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_BlendStack"


FLinearColor UAnimGraphNode_BlendStack::GetNodeTitleColor() const
{
	return FColor(86, 182, 194);
}

FText UAnimGraphNode_BlendStack::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Blend Stack");
}

FText UAnimGraphNode_BlendStack::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Blend Stack");
}

FText UAnimGraphNode_BlendStack::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Pose Search");
}

void UAnimGraphNode_BlendStack::GetOutputLinkAttributes(FNodeAttributeArray& OutAttributes) const
{
	if (UE::Anim::IAnimRootMotionProvider::Get())
	{
		OutAttributes.Add(UE::Anim::IAnimRootMotionProvider::AttributeName);
	}
}

void UAnimGraphNode_BlendStack::BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	AnimBlueprint->FindOrAddGroup(Node.GetGroupName());
}

bool UAnimGraphNode_BlendStack::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset* UAnimGraphNode_BlendStack::GetAnimationAsset() const
{
	return nullptr;
}

const TCHAR* UAnimGraphNode_BlendStack::GetTimePropertyName() const
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_BlendStack::GetTimePropertyStruct() const
{
	return FAnimNode_BlendStack::StaticStruct();
}

#undef LOCTEXT_NAMESPACE

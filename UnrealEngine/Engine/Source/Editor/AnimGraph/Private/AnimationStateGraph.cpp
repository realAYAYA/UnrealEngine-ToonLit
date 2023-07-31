// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationStateGraph.h"

#define LOCTEXT_NAMESPACE "AnimationStateGraph"

/////////////////////////////////////////////////////
// UAnimationStateGraph

UAnimationStateGraph::UAnimationStateGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAnimGraphNode_StateResult* UAnimationStateGraph::GetResultNode()
{
	return MyResultNode;
}

#undef LOCTEXT_NAMESPACE

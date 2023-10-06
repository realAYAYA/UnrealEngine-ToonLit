// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlFlowConditionalLoop.h"
#include "ControlFlow.h"

FConditionalLoop::FConditionalLoop()
	: FlowLoop(MakeShared<FControlFlow>())
{}

FControlFlow& FConditionalLoop::SetCheckConditionFirst(bool bInValue)
{
	CheckConditionalFirst.Emplace(bInValue);
	return FlowLoop.Get();
}

FControlFlow& FConditionalLoop::ExecuteAtLeastOnce()
{
	return SetCheckConditionFirst(false);
}

FControlFlow& FConditionalLoop::CheckConditionFirst()
{
	return SetCheckConditionFirst(true);
}

FControlFlow& FConditionalLoop::RunLoopFirst()
{
	return SetCheckConditionFirst(false);
}
